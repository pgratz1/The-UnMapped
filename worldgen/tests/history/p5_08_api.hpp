#pragma once

// The one place of the P5-08 tests' names and values that the spec (docs/specs/p5-08-pantheon-cults.md r1) gives but
// that the 0.29.0 library does not have yet, and the switch that turns the cases that need the 0.30.0 engine on.
//
// The P5-08 tests are written from the spec before the implementation (on 0.29.0). Every library name the spec adds
// (11.1: Timeline::faith_at, cults_in_at, cult_status_at, divine_effect_at; inspect::shrine_word; the browser's group
// `gods`; the marker glyphs 21 to 23) is reached only through the functions below, and only when UNMAPPED_P508_API is
// defined: the signatures are the spec's, so at integration a compile error here names a signature that differs from
// the spec, which is a finding, not a test to adjust silently. The seams of 7 (DivineView, prepare_sent, create_sent,
// raise_divine, partners, InterventionApplier) are not called from C++: their contracts are checked by the reference's
// replay (worldgen/tests/reference/p5_08_pantheon_reference.py), which must equal the engine table for table.
//
// The checks read the world file's tables by name (WorldReader::table_rows), so they need no new row type; the content
// edits name the spec's fields (history.pantheon, the god kind) and fail at run time, not at compile time, on a 0.29.0
// content set.
//
// Integration: define UNMAPPED_P508_API (the commented line below, or -DUNMAPPED_P508_API in
// worldgen/tests/CMakeLists.txt) once the implementation is merged. Until then INTEGRATED is false and every case that
// needs 0.30.0 starts with P508_REQUIRE_INTEGRATED(), which returns with a MESSAGE; the cases that check the spec's
// arithmetic by hand (history_pantheon_unit_test.cpp, p5_08_checks.hpp's p508_f) run either way.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "p5_02_monsters_content.hpp"
#include "p5_03_relations_api.hpp"
#include "p5_06_figures_runs.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"

#define UNMAPPED_P508_API 1

namespace p508api {

namespace wd = worldgen::data;
namespace h = worldgen::history;
namespace db = worldgen::db;
using p503api::V;

inline constexpr const char* GENERATOR_SPEC = "0.30.0";                     // header
inline constexpr std::int64_t SCHEMA_SPEC = 14;                             // 9
inline constexpr std::int64_t STREAMS_SPEC = 120;                           // 3: thirteen after hist.deep.warren_kind
inline constexpr std::int64_t KINDS_SPEC = 110;                             // 8.1: 100 to 110
inline constexpr const char* REPORT_HEADER_SPEC = "# worldgen history v11";  // 10 item 1
inline constexpr std::int64_t K_BLESS = 100, K_CURSE = 101, K_CHAMPION = 102, K_MONSTER = 103, K_MARK = 104;
inline constexpr std::int64_t K_FOUNDED = 105, K_SPREAD = 106, K_ADOPTED = 107, K_SUPPRESSED = 108;
inline constexpr std::int64_t K_TAKEN_UP = 109, K_ABANDONED = 110;
inline constexpr std::int64_t SITE_LAIR = 1, SITE_TOWER = 4, SITE_SHRINE = 5, SITE_TEMPLE = 6, SITE_SANCTUM = 7;
inline constexpr std::int64_t ENDING_LOST = 4, ENDING_ABANDONED = 7, ENDING_SUPPRESSED = 8;   // site.ending
inline constexpr std::int64_t ROLE_CHAMPION = 6, ROLE_PROPHET = 7, ROLE_CASTER = 5, ROLE_RULER = 1;   // figure.role
inline constexpr std::int64_t STATUS_PRESENT = 1, STATUS_HIDDEN = 2, STATUS_ENDED = 3;   // faction.status
inline constexpr std::int64_t CULT_SUPPRESSED = 1, CULT_FELL = 2;                         // faction.ending
inline constexpr std::int64_t ENTITY_GOD = 2, ENTITY_CIV = 3, ENTITY_FIGURE = 6, ENTITY_FACTION = 7;
inline constexpr std::int64_t GROUP_GODS = 9, ALL_GROUPS = 0x3FF;                         // 11.1
inline constexpr std::int64_t GLYPH_SHRINE = 21, GLYPH_TEMPLE = 22, GLYPH_SANCTUM = 23;   // 11.1

#if defined(UNMAPPED_P508_API)
inline constexpr bool INTEGRATED = true;
#else
inline constexpr bool INTEGRATED = false;
#endif

// 3: the thirteen streams, in this order, after hist.deep.warren_kind (the 107th).
inline const std::vector<std::string>& streams() {
    static const std::vector<std::string> v = {
        "hist.pantheon.act",         "hist.pantheon.kind",      "hist.pantheon.flare", "hist.pantheon.monster_kind",
        "hist.pantheon.site_name",   "hist.pantheon.cult_found", "hist.pantheon.cult_name",
        "hist.pantheon.cult_spread", "hist.pantheon.adopt",     "hist.pantheon.suppress", "hist.pantheon.hide",
        "hist.deep.stir",            "hist.deep.warren_stir"};
    return v;
}
// 8.1: the kinds by id.
inline const std::vector<std::pair<std::int64_t, std::string>>& new_kinds() {
    static const std::vector<std::pair<std::int64_t, std::string>> v = {
        {100, "divine_bless"},  {101, "divine_curse"},  {102, "divine_champion"}, {103, "divine_monster"},
        {104, "divine_mark"},   {105, "cult_founded"},  {106, "cult_spread"},     {107, "cult_adopted"},
        {108, "cult_suppressed"}, {109, "tower_taken_up"}, {110, "tower_abandoned"}};
    return v;
}

// --- Content ---------------------------------------------------------------------------------------------------

// A history tuning field, or `fallback` when the definition or the field is absent (a 0.29.0 content set).
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

inline wd::Value rec3(const char* a, std::int64_t x, const char* b, std::int64_t y, const char* c, std::int64_t z) {
    wd::Record r;
    r.add(a, V(x));
    r.add(b, V(y));
    r.add(c, V(z));
    return wd::Value::record(std::move(r));
}

// 4.1 pantheon-small: monsters-small (P5-02 19.3) with the ten pantheon edits, so that every path of 6 occurs at 512.
inline const wd::ContentSet& pantheon_small() {
    static const wd::ContentSet c = p503api::with_history(
        monsters_content::monsters_small_content(), "pantheon",
        {{"act_threshold", V(30)}, {"act_permille", V(1000)}, {"god_rest_years", V(5)}, {"pair_rest_years", V(10)},
         {"cult_found_permille", V(1000)}, {"cult_spread_permille", V(300)}, {"adopt_permille", V(300)},
         {"suppress_permille", V(200)}, {"hide_permille", V(500)}, {"flare_permille", V(1000)}});
    return c;
}
// 4.2 the fixture tuning `zero` (I0, 14): the shipped values with act_permille 0 and flare_permille 0.
inline const wd::ContentSet& zero() {
    static const wd::ContentSet c = p503api::with_history(conflict_test::shipped(), "pantheon",
                                                          {{"act_permille", V(0)}, {"flare_permille", V(0)}});
    return c;
}
// pantheon-small with further history edits (a fixture file's [pantheon] table, in memory).
inline wd::ContentSet pantheon_small_with(const std::map<std::string, wd::Value>& edits) {
    return p503api::with_history(pantheon_small(), "pantheon", edits);
}

// A generated world of `content` (cached per process), as p506run::generated; `until` stops the pipeline and
// `activity` is the --divine-activity setting.
inline std::string generated(std::uint64_t seed, std::int64_t size, const wd::ContentSet& content, const std::string& tag,
                             std::uint32_t threads = 1, const std::string& until = "",
                             const std::string& activity = "normal") {
    if (until.empty() && activity == "normal") return p506run::generated(seed, size, content, "p508-" + tag, 0, threads);
    static std::map<std::string, std::string> cache;
    const std::string key = tag + "-" + std::to_string(seed) + "-" + std::to_string(size) + "-" + until + "-" +
                            activity + "-" + std::to_string(threads);
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
    chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("all"));
    p.activity = activity;
    const std::string path = chrono_test::world_dir().file("p508-" + key + ".db");
    const std::optional<std::string> u = until.empty() ? std::nullopt : std::optional<std::string>(until);
    const worldgen::WorldParams wp = chrono_test::world_params(seed, size, p, content, u, threads);
    REQUIRE(worldgen::generate_world(wp, content, path, db::Overwrite::refuse).ok());
    cache[key] = path;
    return path;
}

// --- The library names the spec adds (11.1) -----------------------------------------------------------------------

// Timeline::faith_at(civ, year): the faction of the faith_of row in force, 0 none.
inline std::int64_t faith_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t civ,
                             [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P508_API)
    return t.faith_at(civ, year);
#else
    return -1;
#endif
}
// Timeline::cults_in_at(civ, year): ascending.
inline std::vector<std::int64_t> cults_in_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t civ,
                                             [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P508_API)
    return t.cults_in_at(civ, year);
#else
    return {-1};
#endif
}
// Timeline::cult_status_at(faction, year): 0 not founded, 1 present, 2 hidden, 3 ended.
inline std::int64_t cult_status_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t faction,
                                   [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P508_API)
    return t.cult_status_at(faction, year);
#else
    return -1;
#endif
}
// Timeline::divine_effect_at(civ, year): bit 1 plenty, 2 withering, 4 valour, 8 defeat.
inline std::int64_t divine_effect_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t civ,
                                     [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P508_API)
    return t.divine_effect_at(civ, year);
#else
    return -1;
#endif
}
// inspect::shrine_word(profane) in worldgen/inspect/markers.hpp: `holy to`, `cursed by`.
inline std::string shrine_word([[maybe_unused]] bool profane) {
#if defined(UNMAPPED_P508_API)
    return std::string(worldgen::inspect::shrine_word(profane));
#else
    return "?";
#endif
}

// --- The fixture files of worldgen/tests/data/pantheon/ (read for their cases; the content edits are the reference's) ---

struct FixtureCase {
    std::uint64_t seed = 0;
    std::int64_t span = 6000;
    std::string races = "all", civs = "auto", intensity = "normal", activity = "normal";
};
// The [[case]] tables of a fixture file (a minimal reader of the file's own layout: `key = value` lines).
inline std::vector<FixtureCase> fixture_cases(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot read " << file.string());
    std::vector<FixtureCase> out;
    std::string line;
    bool in_case = false;
    while (std::getline(in, line)) {
        if (line.rfind("[[case]]", 0) == 0) {
            out.emplace_back();
            in_case = true;
            continue;
        }
        if (!line.empty() && line[0] == '[') in_case = false;
        if (!in_case || line.empty() || line[0] == '#') continue;
        const std::size_t eq = line.find(" = ");
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 3);
        if (v.size() >= 2U && v.front() == '"') v = v.substr(1, v.size() - 2U);
        FixtureCase& c = out.back();
        if (k == "seed") c.seed = static_cast<std::uint64_t>(std::stoll(v));
        if (k == "span") c.span = std::stoll(v);
        if (k == "races") c.races = v;
        if (k == "civs") c.civs = v;
        if (k == "intensity") c.intensity = v;
        if (k == "activity") c.activity = v;
    }
    return out;
}

}  // namespace p508api

// Every test case that needs the 0.30.0 engine starts with this: before integration it returns (a MESSAGE, no check).
#define P508_REQUIRE_INTEGRATED()                                                          \
    do {                                                                                   \
        if (!p508api::INTEGRATED) {                                                        \
            MESSAGE("P5-08 not integrated (define UNMAPPED_P508_API): case not run");      \
            return;                                                                        \
        }                                                                                  \
    } while (false)
