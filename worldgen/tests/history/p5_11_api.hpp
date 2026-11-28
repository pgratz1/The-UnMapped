#pragma once

// The one place of the P5-11 tests' names and values that the spec (docs/specs/p5-11-artifacts.md r1) gives but that
// the 0.30.0 library does not have yet, and the switch that turns the cases that need the 0.31.0 engine on.
//
// The P5-11 tests are written from the spec before the implementation (on 0.30.0). Every library name the spec adds
// (11.1: Timeline::artifact_at, artifacts_held, artifacts_placed and the struct ArtifactPlace; inspect::
// artifact_state_word and MarkerOptions::artifacts; the browser's group `artifacts`; history_entity_tag's `ar`) is
// reached only through the functions below, and only when UNMAPPED_P511_API is defined: the signatures are the spec's,
// so at integration a compile error here names a signature that differs from the spec, which is a finding, not a test
// to adjust silently. The seams of 7 (ArtifactView, raise_maker, brief, caster_site, forge_site, force_act) are not
// called from C++: their contracts are checked by the reference's replay (worldgen/tests/reference/
// p5_11_artifacts_reference.py), which must equal the engine table for table (golden p5-11-artifacts.txt).
//
// The checks read the world file's tables by name (WorldReader::table_rows), so they need no new row type; the content
// edits name the spec's fields (history.artifacts) and fail at run time, not at compile time, on a 0.30.0 content set.
//
// Integration: define UNMAPPED_P511_API (the commented line below, or -DUNMAPPED_P511_API in
// worldgen/tests/CMakeLists.txt) once the implementation is merged. Until then INTEGRATED is false and every case that
// needs 0.31.0 starts with P511_REQUIRE_INTEGRATED(), which returns with a MESSAGE; the cases that check the spec's
// arithmetic by hand (history_artifacts_unit_test.cpp, p5_11_checks.hpp's p511_f) run either way.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "p5_08_api.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/history_report.hpp"
#include "worldgen/inspect/markers.hpp"

#define UNMAPPED_P511_API 1

namespace p511api {

namespace wd = worldgen::data;
namespace h = worldgen::history;
namespace db = worldgen::db;
using p503api::V;

inline constexpr const char* GENERATOR_SPEC = "0.31.0";                     // header
inline constexpr std::int64_t SCHEMA_SPEC = 15;                             // 9
inline constexpr std::int64_t STREAMS_SPEC = 127;                           // 3: seven after hist.deep.warren_stir
inline constexpr std::int64_t KINDS_SPEC = 116;                             // 8.1: 111 to 116
inline constexpr std::int64_t TABLES_SPEC = 40;                             // 9
inline constexpr const char* REPORT_HEADER_SPEC = "# worldgen history v12";  // 10 item 1
inline constexpr std::int64_t K_MADE = 111, K_TAKEN = 112, K_LOST = 113, K_HOARDED = 114, K_RECOVERED = 115,
                              K_BURIED = 116;
inline constexpr std::int64_t ARMS = 1, ARMOUR = 2, REGALIA = 3, RELIC = 4, STAFF = 5;   // artifact_kind.class
inline constexpr std::int64_t KEPT = 1, BORNE = 2, HOARDED = 3, LOST = 4, BURIED = 5, ENSHRINED = 6;   // 6.2 state
inline constexpr std::int64_t SMITH_MADE = 1, COMMISSION = 2, CASTER_MADE = 3, GOD_GIVEN = 4;   // artifact.maker_kind
inline constexpr std::int64_t ROLE_SMITH = 8, ROLE_CASTER = 5;   // figure.role
inline constexpr std::int64_t ENTITY_GOD = 2, ENTITY_CIV = 3, ENTITY_SETTLEMENT = 4, ENTITY_REGION = 5,
                              ENTITY_FIGURE = 6, ENTITY_MONSTER = 8, ENTITY_SITE = 9, ENTITY_ARTIFACT = 10;
inline constexpr std::int64_t SITE_LAIR = 1, SITE_MINE = 3, SITE_TOWER = 4, SITE_TEMPLE = 6, SITE_SANCTUM = 7;
inline constexpr std::int64_t GROUP_ARTIFACTS = 10, ALL_GROUPS = 0x7FF;   // 11.1
inline constexpr std::int64_t GLYPH_ARTIFACT = 24;                        // 11.1
inline constexpr std::int64_t SUMMARY_ARTIFACTS_MAX = 8;                  // 10 item 2

#if defined(UNMAPPED_P511_API)
inline constexpr bool INTEGRATED = true;
#else
inline constexpr bool INTEGRATED = false;
#endif

// 3: the seven streams, in this order, after hist.deep.warren_stir (the 120th).
inline const std::vector<std::string>& streams() {
    static const std::vector<std::string> v = {"hist.artifacts.make", "hist.artifacts.class", "hist.artifacts.kind",
                                               "hist.artifacts.take", "hist.artifacts.bury",  "hist.artifacts.raid",
                                               "hist.names.artifact"};
    return v;
}
// 8.1: the kinds by id.
inline const std::vector<std::pair<std::int64_t, std::string>>& new_kinds() {
    static const std::vector<std::pair<std::int64_t, std::string>> v = {
        {111, "artifact_made"},  {112, "artifact_taken"},     {113, "artifact_lost"},
        {114, "artifact_hoarded"}, {115, "artifact_recovered"}, {116, "artifact_buried"}};
    return v;
}
// 10 item 6, in this order (cult_returns_barred dropped by the lead, 2026-11-28; reading A-R14 of the reference).
inline const std::vector<std::string>& stats_keys() {
    static const std::vector<std::string> v = {
        "artifacts_made",      "artifacts_smiths",       "artifacts_commissioned", "artifacts_casters",
        "artifacts_gods",      "artifacts_deep",         "artifacts_arms",         "artifacts_armour",
        "artifacts_regalia",   "artifacts_relic",        "artifacts_staff",        "artifacts_taken",
        "artifacts_lost",      "artifacts_hoarded",      "artifacts_recovered",    "artifacts_buried",
        "artifact_holdings",   "artifacts_changed_hands", "artifacts_lost_at_end", "artifacts_held_by_others",
        "smiths",              "wars_citing_artifacts",  "battles_with_artifacts", "battles_under_acts"};
    return v;
}

// --- Content ---------------------------------------------------------------------------------------------------

// history.artifacts with every occasion's chance_permille set to `chance` and the scalar fields of `edits` replaced.
inline wd::ContentSet with_artifacts(const wd::ContentSet& base, const std::map<std::string, wd::Value>& edits,
                                     std::optional<std::int64_t> chance = std::nullopt,
                                     bool no_occasions = false) {
    wd::ContentSet c = p503api::with_history(base, "artifacts", edits);
    if (!chance.has_value() && !no_occasions) return c;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (wd::Definition& d : k.definitions) {
            if (d.id != "artifacts") continue;
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                if (d.record.name_at(i) != "occasions") {
                    r.add(d.record.name_at(i), d.record.value_at(i));
                    continue;
                }
                std::vector<wd::Value> occ;
                if (!no_occasions) {
                    for (const wd::Value& o : d.record.value_at(i).as_array()) {
                        wd::Record x;
                        const wd::Record& orec = o.as_record();
                        for (std::size_t j = 0; j < orec.size(); ++j) {
                            x.add(orec.name_at(j), orec.name_at(j) == "chance_permille" ? V(*chance) : orec.value_at(j));
                        }
                        occ.push_back(wd::Value::record(std::move(x)));
                    }
                }
                r.add("occasions", wd::Value::array(std::move(occ)));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

// 4.6 artifacts-small: P5-08's pantheon-small plus every shipped occasion at 1000, realm_rest_years 0, world_max 500,
// burial_permille 500, battle_take_permille 500, raid_take_permille 500.
inline const wd::ContentSet& artifacts_small() {
    static const wd::ContentSet c = with_artifacts(
        p508api::pantheon_small(),
        {{"realm_rest_years", V(0)}, {"world_max", V(500)}, {"burial_permille", V(500)},
         {"battle_take_permille", V(500)}, {"raid_take_permille", V(500)}},
        1000);
    return c;
}
// 4.3 the fixture tuning `zero` (I0, 14): the shipped values with occasions empty.
inline const wd::ContentSet& zero() {
    static const wd::ContentSet c = with_artifacts(conflict_test::shipped(), {}, std::nullopt, true);
    return c;
}
// artifacts-small with further edits (the scalar fields of history.artifacts).
inline wd::ContentSet artifacts_small_with(const std::map<std::string, wd::Value>& edits) {
    return p503api::with_history(artifacts_small(), "artifacts", edits);
}
// A history tuning field, or `fallback` when the definition or the field is absent.
inline std::int64_t tuning_or(const wd::ContentSet& c, const std::string& id, const std::string& field,
                              std::int64_t fallback) {
    return p508api::tuning_or(c, id, field, fallback);
}

// A generated world of `content` (cached per process), as p508api::generated; `intensity` the --event-intensity.
inline std::string generated(std::uint64_t seed, std::int64_t size, const wd::ContentSet& content, const std::string& tag,
                             std::uint32_t threads = 1, const std::string& until = "",
                             const std::string& intensity = "normal") {
    if (intensity == "normal") return p508api::generated(seed, size, content, "p511-" + tag, threads, until);
    static std::map<std::string, std::string> cache;
    const std::string key = tag + "-" + std::to_string(seed) + "-" + std::to_string(size) + "-" + until + "-" +
                            intensity + "-" + std::to_string(threads);
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
    chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("all"));
    p.intensity = intensity;
    const std::string path = chrono_test::world_dir().file("p511-" + key + ".db");
    const std::optional<std::string> u = until.empty() ? std::nullopt : std::optional<std::string>(until);
    const worldgen::WorldParams wp = chrono_test::world_params(seed, size, p, content, u, threads);
    REQUIRE(worldgen::generate_world(wp, content, path, db::Overwrite::refuse).ok());
    cache[key] = path;
    return path;
}

// --- The library names the spec adds (11.1) -----------------------------------------------------------------------

// Timeline::artifact_at(artifact, year) as the spec's ArtifactPlace fields, in its order.
struct Place {
    std::int64_t state = 0, holder_type = 0, holder = 0, region = 0, settlement = 0, site = 0, x = 0, y = 0, since = 0,
                 event = 0;
    friend bool operator==(const Place&, const Place&) = default;
};
inline Place artifact_at([[maybe_unused]] const h::Timeline& t, [[maybe_unused]] std::int64_t artifact,
                         [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P511_API)
    const auto p = t.artifact_at(artifact, year);
    return Place{p.state, p.holder_type, p.holder, p.region, p.settlement, p.site, p.x, p.y, p.since, p.event};
#else
    return Place{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
#endif
}
// Timeline::artifacts_held(holder_type, holder, year): ascending.
inline std::vector<std::int64_t> artifacts_held([[maybe_unused]] const h::Timeline& t,
                                                [[maybe_unused]] std::int64_t holder_type,
                                                [[maybe_unused]] std::int64_t holder, [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P511_API)
    return t.artifacts_held(holder_type, holder, year);
#else
    return {-1};
#endif
}
// Timeline::artifacts_placed(year): the artifacts in states 3 to 6 at that year, ascending.
inline std::vector<std::int64_t> artifacts_placed([[maybe_unused]] const h::Timeline& t,
                                                  [[maybe_unused]] std::int64_t year) {
#if defined(UNMAPPED_P511_API)
    return t.artifacts_placed(year);
#else
    return {-1};
#endif
}
// inspect::artifact_state_word(state): kept, borne, hoarded, lost, buried, enshrined.
inline std::string artifact_state_word([[maybe_unused]] std::int64_t state) {
#if defined(UNMAPPED_P511_API)
    return std::string(worldgen::inspect::artifact_state_word(state));
#else
    return "?";
#endif
}
// MarkerOptions::artifacts: present and on by default.
inline bool marker_option_artifacts_default() {
#if defined(UNMAPPED_P511_API)
    const worldgen::inspect::MarkerOptions o;
    return o.artifacts;
#else
    return false;
#endif
}
// The browser's group of a kind key (as its number) and ALL_GROUPS.
inline std::int64_t event_group_of(const std::string& key) {
    return static_cast<std::int64_t>(worldgen::inspect::event_group(key));
}
inline std::int64_t all_groups() { return static_cast<std::int64_t>(worldgen::inspect::ALL_GROUPS); }
// history_entity_tag(10, id): `ar<id>`.
inline std::string entity_tag(std::int64_t type, std::int64_t id) { return worldgen::inspect::history_entity_tag(type, id); }

}  // namespace p511api

// Every test case that needs the 0.31.0 engine starts with this: before integration it returns (a MESSAGE, no check).
#define P511_REQUIRE_INTEGRATED()                                                          \
    do {                                                                                   \
        if (!p511api::INTEGRATED) {                                                        \
            MESSAGE("P5-11 not integrated (define UNMAPPED_P511_API): case not run");      \
            return;                                                                        \
        }                                                                                  \
    } while (false)
