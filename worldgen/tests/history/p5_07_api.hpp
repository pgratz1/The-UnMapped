#pragma once

// The one place of the P5-07 tests' names and values that the spec (docs/specs/p5-07-mountain-depths.md r2) gives
// but that the 0.28.0 library does not have yet, and the switch that turns the cases that need the 0.29.0 engine on.
//
// The P5-07 tests are written from the spec before the implementation (on 0.28.0). Every library name the spec adds
// (Timeline::mine_depth_at and sealed_year of 11.1, inspect::mine_depth_word of 11.1, MonstersSubsystem::mine_depth
// and AllianceView::forge_depth of 7.1, ConflictSubsystem::deep_flight_plan of 7.3, RelationsSubsystem::credit_units
// of 7.5, the segment kind `tagged` of 12 item 4; r2: Timeline::warren_depth_at of 11.1) is reached only through the
// functions below, and only when
// UNMAPPED_P507_API is defined: the signatures are the spec's, so at integration a compile error here names a
// signature that differs from the spec, which is a finding, not a test to adjust silently.
//
// Integration: define UNMAPPED_P507_API (the commented line below, or -DUNMAPPED_P507_API in
// worldgen/tests/CMakeLists.txt) once the implementation is merged. Until then INTEGRATED is false and every case that
// needs 0.29.0 starts with P507_REQUIRE_INTEGRATED(), which returns with a MESSAGE; the cases that check the spec's
// arithmetic by hand (history_depths_unit_test.cpp, p5_07_checks.hpp's p507_f) run either way.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "p5_02_monsters_content.hpp"
#include "p5_03_relations_api.hpp"
#include "p5_06_figures_runs.hpp"
#include "p5_07_depths_content.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"

#define UNMAPPED_P507_API 1

namespace p507api {

namespace wd = worldgen::data;
namespace h = worldgen::history;
namespace db = worldgen::db;
using p503api::V;

inline constexpr const char* GENERATOR_SPEC = "0.29.0";            // header
inline constexpr std::int64_t SCHEMA_SPEC = 13;                    // 9
inline constexpr std::int64_t STREAMS_SPEC = 107;                  // 3: nine appended after hist.alliances.renew (r2)
inline constexpr std::int64_t KINDS_SPEC = 99;                     // 8.1: 93 to 99 (r2)
inline constexpr const char* REPORT_HEADER_SPEC = "# worldgen history v10";   // 10 item 1
inline constexpr std::int64_t WAKE_DEEP = 3;                       // 9: monster_kind.wake 3 deep
inline constexpr std::int64_t K_DELVING = 93, K_WOKEN = 94, K_SEALED = 95, K_LOST = 96;   // 8.1
inline constexpr std::int64_t K_AIDED = 97, K_WARREN_DELVED = 98, K_WARREN_SEALED = 99;   // 8.1 (r2)
inline constexpr std::int64_t SITE_LAIR = 1, SITE_MINE = 3;        // P5-02 11.2, P5-04 9.2
inline constexpr std::int64_t ENDING_LOST = 4;                     // site.ending 4 lost (6.6 "The mine")
inline constexpr std::int64_t SETTLEMENT_ENDING_OVERRUN = 2;       // 6.6 "The ending"

#if defined(UNMAPPED_P507_API)
inline constexpr bool INTEGRATED = true;
#else
inline constexpr bool INTEGRATED = false;
#endif

// 3: the five streams, in this order, after hist.alliances.renew.
inline const std::vector<std::string>& streams() {
    static const std::vector<std::string> v = {"hist.deep.delve", "hist.deep.wake", "hist.deep.kind", "hist.deep.gate",
                                               "hist.deep.take",
                                               // r2
                                               "hist.deep.aid", "hist.deep.warren_delve", "hist.deep.warren_wake",
                                               "hist.deep.warren_kind"};
    return v;
}
// 8.1: the kinds by id.
inline const std::vector<std::pair<std::int64_t, std::string>>& new_kinds() {
    static const std::vector<std::pair<std::int64_t, std::string>> v = {
        {93, "delving_deepened"}, {94, "deep_woken"}, {95, "deep_sealed"}, {96, "hold_lost"},
        {97, "ally_aided_hold"}, {98, "warren_delved"}, {99, "warren_sealed"}};
    return v;
}

// A history tuning field, or `fallback` when the definition or the field is absent (a 0.28.0 content set).
inline std::int64_t tuning_or(const wd::ContentSet& c, const std::string& id, const std::string& field,
                              std::int64_t fallback) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != id) continue;
            const wd::Value* v = d.record.find(field);
            return v == nullptr ? fallback : v->as_int();
        }
    }
    return fallback;
}
inline std::vector<std::int64_t> tuning_array_or(const wd::ContentSet& c, const std::string& id,
                                                 const std::string& field, std::vector<std::int64_t> fallback) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != id) continue;
            const wd::Value* v = d.record.find(field);
            if (v == nullptr) return fallback;
            std::vector<std::int64_t> out;
            for (const wd::Value& x : v->as_array()) out.push_back(x.as_int());
            return out;
        }
    }
    return fallback;
}
inline wd::Value ints(const std::vector<std::int64_t>& v) {
    std::vector<wd::Value> a;
    for (const std::int64_t x : v) a.push_back(wd::Value::integer(x));
    return wd::Value::array(std::move(a));
}

// Fields of the race definition `id` replaced (each must exist), as p503api::with_history for history definitions.
inline wd::ContentSet with_race(const wd::ContentSet& base, const std::string& id,
                                const std::map<std::string, wd::Value>& edits) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (wd::Definition& d : k.definitions) {
            if (d.id != id) continue;
            wd::Record r;
            std::size_t used = 0;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const auto it = edits.find(d.record.name_at(i));
                used += it != edits.end() ? 1U : 0U;
                r.add(d.record.name_at(i), it != edits.end() ? it->second : d.record.value_at(i));
            }
            REQUIRE_MESSAGE(used == edits.size(), "race." << id << ": an edited field is not in the data");
            d.record = std::move(r);
        }
    }
    return c;
}

// 4.2 depths-small (from monsters-small, P5-02 19.3: the stress content with every monster kind's wake_rate x 2000
// and overrun_permille 1000, living_permille 1000): deep_years 5, 10, 20, deep_delve_permille 1000, deep_wake_ppm 0,
// 200000, 600000, deep_takes_max 3; (r2) deep_warren_population_min 100, deep_warren_wake_permille 1000,
// deep_aid_permille 1000 and goblin delving_permille 1000 (appended to the race record, where the fixture roots leave
// the optional field out). One source of truth: the engineer's in-memory content of spec 6.12 item 4
// (p5_07_depths_content.hpp, depths_small_content()); integration checked it against 4.2's text.
inline const wd::ContentSet& depths_small() {
    return depths_content::depths_small_content();
}
// 14 I1 (r2): depths-small with delving_permille 0 for every race and deep_aid_permille 0.
inline const wd::ContentSet& i1_content() {
    static const wd::ContentSet c = [] {
        wd::ContentSet x = p503api::with_history(depths_small(), "monsters", {{"deep_aid_permille", V(0)}});
        for (const char* r : {"dwarf", "elf", "faerie", "goblin", "human", "orc"}) {
            x = depths_content::with_race_field(x, r, "delving_permille", V(0));
        }
        return x;
    }();
    return c;
}
// 14 I0: the shipped data with deep_delve_permille 0 and forge_depth_permille 0, or forge_depth_permille 15.
inline const wd::ContentSet& zero(std::int64_t forge_depth) {
    static std::map<std::int64_t, wd::ContentSet> cache;
    if (const auto it = cache.find(forge_depth); it != cache.end()) return it->second;
    wd::ContentSet c = p503api::with_history(conflict_test::shipped(), "monsters", {{"deep_delve_permille", V(0)}});
    c = p503api::with_history(c, "conflict", {{"forge_depth_permille", V(forge_depth)}});
    return cache.emplace(forge_depth, std::move(c)).first->second;
}

// A generated 512 world of `content` (cached per process), as p506run::generated; `until` stops the pipeline.
inline std::string generated(std::uint64_t seed, std::int64_t size, const wd::ContentSet& content, const std::string& tag,
                             std::uint32_t threads = 1, const std::string& until = "") {
    if (until.empty()) return p506run::generated(seed, size, content, "p507-" + tag, 0, threads);
    static std::map<std::string, std::string> cache;
    const std::string key = tag + "-" + std::to_string(seed) + "-" + std::to_string(size) + "-" + until;
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
    const chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("all"));
    const std::string path = chrono_test::world_dir().file("p507-" + key + ".db");
    const worldgen::WorldParams wp =
        chrono_test::world_params(seed, size, p, content, std::optional<std::string>(until), threads);
    REQUIRE(worldgen::generate_world(wp, content, path, db::Overwrite::refuse).ok());
    cache[key] = path;
    return path;
}

// --- The library names the spec adds (11.1, 7.1, 7.3, 7.5, 12 item 4) ------------------------------------------

// 11.1 Timeline::mine_depth_at(site, year): the largest depth of the site's kind 93 events of year <= Y, 0 none.
inline std::int64_t mine_depth_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t site,
                                  [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P507_API)
    return t.mine_depth_at(site, year);
#else
    return -1;
#endif
}
// 11.1 Timeline::sealed_year(monster): the year of its deep_sealed, -1 none.
inline std::int64_t sealed_year([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t monster) {
#if defined(UNMAPPED_P507_API)
    return t.sealed_year(monster);
#else
    return -2;
#endif
}
// r2 11.1 Timeline::warren_depth_at(settlement, year): the largest depth of its kind 98 events of year <= Y, 0 none.
inline std::int64_t warren_depth_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t settlement,
                                    [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P507_API)
    return t.warren_depth_at(settlement, year);
#else
    return -1;
#endif
}
// 11.1 mine_depth_word(depth) in worldgen/inspect/markers.hpp.
inline std::string mine_depth_word([[maybe_unused]] std::int64_t depth) {
#if defined(UNMAPPED_P507_API)
    return std::string(worldgen::inspect::mine_depth_word(depth));
#else
    return "?";
#endif
}

}  // namespace p507api

// Every test case that needs the 0.29.0 engine starts with this: before integration it returns (a MESSAGE, no check).
#define P507_REQUIRE_INTEGRATED()                                                          \
    do {                                                                                   \
        if (!p507api::INTEGRATED) {                                                        \
            MESSAGE("P5-07 not integrated (define UNMAPPED_P507_API): case not run");      \
            return;                                                                        \
        }                                                                                  \
    } while (false)
