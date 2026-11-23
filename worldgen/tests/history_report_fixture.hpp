#pragma once

// Test support for spec P3-07 (the history report): the hand-made fixture worlds of 14.3, read from
// worldgen/tests/data/history-report/<name>.toml (format: README.md there) and written through the writer
// API (a synthetic pipeline of four stages through generate_world_with_stages, as P3-04 16.2 item 17), the
// golden file worldgen/tests/golden/p3-07-report.txt (written by reference/p3_report_reference.py), and
// small text helpers. The reference expands the same files the same way and writes its copy through SQL;
// the table hashes of the two copies must agree (14.3).
//
// Needs no P3-07 name of the library: what follows compiles and runs before the report exists.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <toml.hpp>

#include "db_test_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/history/params.hpp"
#include "worldgen/inspect/hashes.hpp"
#include "worldgen/stages/stage.hpp"
#include "worldgen/stages/testing.hpp"

#ifndef UNMAPPED_P3_07_FIXTURE_DIR
#error "UNMAPPED_P3_07_FIXTURE_DIR must name worldgen/tests/data/history-report"
#endif
#ifndef UNMAPPED_P3_07_GOLDEN_PATH
#error "UNMAPPED_P3_07_GOLDEN_PATH must name worldgen/tests/golden/p3-07-report.txt"
#endif

namespace hrtest {

namespace fs = std::filesystem;
namespace db = worldgen::db;

using Cell = std::variant<std::int64_t, std::string, std::monostate>;  // monostate: NULL
using Row = std::vector<Cell>;

inline const std::vector<std::string>& fixture_names() {
    static const std::vector<std::string> v = {"h1-war-and-ruin", "h2-minimal",   "h3-runaway",
                                               "h3b-runaway-1000", "h3c-share-ties", "h4-budget",
                                               "h5-saturated",    "h6-notes",       "h7-runs",
                                               "h8-frames",       "h9-realms",      "h10-floor",
                                               "h11-v2",    // r19 25.13 item 11 (the spec's "H9 v2")
                                               "h12-v3",    // r21 26.8 item 9 (P5-12)
                                               "h13-v4"};   // P5-02 19.1 item 15 (format v4, schema 9)
    return v;
}

// --- The fixture, expanded to rows in primary-key order ----------------------------------------

struct Fixture {
    std::string name;
    std::uint64_t seed = 0;
    std::int64_t map_size = 512;
    std::int64_t span = 0;
    std::vector<std::string> races;
    std::vector<db::BiomeRow> biomes;
    std::vector<db::RegionRow> regions;
    std::vector<db::RaceRow> race_rows;
    std::vector<db::CultureRow> cultures;
    std::vector<db::GodRow> gods;
    std::vector<db::AgeRow> ages;
    std::vector<db::EventKindRow> kinds;
    std::vector<db::EventKindRoleRow> roles;
    std::vector<db::EventKindValueRow> values;
    std::vector<db::PhaseRow> phases;
    std::vector<db::EventRow> events;
    std::vector<db::EventParticipantRow> participants;
    std::vector<db::EventCauseRow> causes;
    std::vector<db::EventValueRow> event_values;
    std::vector<db::CivilizationRow> civilizations;
    std::vector<db::SettlementRow> settlements;
    std::vector<db::SettlementCensusRow> census;
    std::vector<db::RegionControlRow> region_control;
    std::vector<db::RelationRow> relations;
    std::vector<db::RegionNameRow> region_names;  // schema 8 (P5-12 7.2; P3-07 r21 26.8 item 9, H12)
    // schema 9 (P5-02 11.1; H13). GUESS the row types (p5_02 api header of the history tests).
    std::vector<db::BeastRow> beasts;
    std::vector<db::MonsterKindRow> monster_kinds;
    std::vector<db::BeastPopulationRow> beast_populations;
    std::vector<db::MonsterRow> monsters;
    std::vector<db::SiteRow> sites;
};

namespace detail {

inline std::int64_t i64(const toml::node& n) {
    const auto v = n.value<std::int64_t>();
    REQUIRE_MESSAGE(v.has_value(), "fixture: expected an integer");
    return *v;
}
inline std::string str(const toml::node& n) {
    const auto v = n.value<std::string>();
    REQUIRE_MESSAGE(v.has_value(), "fixture: expected a string");
    return *v;
}
inline std::optional<std::int64_t> nullable(const toml::node& n) {
    const std::int64_t v = i64(n);
    return v == -1 ? std::nullopt : std::optional<std::int64_t>(v);
}
// The rows of [tables].<name> (empty when absent), each as its array of nodes.
inline std::vector<const toml::array*> rows(const toml::table& doc, std::string_view name) {
    std::vector<const toml::array*> out;
    const toml::array* a = doc["tables"][name].as_array();
    if (a == nullptr) {
        return out;
    }
    for (const toml::node& r : *a) {
        const toml::array* row = r.as_array();
        REQUIRE_MESSAGE(row != nullptr, "fixture: a row of " << name << " is not an array");
        out.push_back(row);
    }
    return out;
}
inline std::string text_of_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE_MESSAGE(f.good(), "cannot read " << p.string());
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

}  // namespace detail

inline Fixture load_fixture(const std::string& name) {
    using detail::i64;
    using detail::nullable;
    using detail::str;
    const fs::path path = fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (name + ".toml");
    const toml::table doc = toml::parse(detail::text_of_file(path), path.string());
    Fixture fx;
    fx.name = name;
    const toml::node_view<const toml::node> m = doc["meta"];
    fx.seed = static_cast<std::uint64_t>(i64(*m["seed"].node()));
    fx.map_size = i64(*m["map_size"].node());
    fx.span = i64(*m["history_span"].node());
    REQUIRE(str(*m["seed_phrase"].node()).empty());
    REQUIRE(str(*m["history_detail"].node()) == "normal");
    REQUIRE(str(*m["civs_per_race"].node()) == "auto");
    REQUIRE(str(*m["event_intensity"].node()) == "normal");
    REQUIRE(str(*m["monster_pressure"].node()) == "normal");
    REQUIRE(str(*m["divine_activity"].node()) == "normal");
    REQUIRE(str(*m["divine_script"].node()).empty());
    {
        const std::string races = str(*m["races"].node());
        std::string cur;
        for (const char c : races) {
            if (c == ',') {
                fx.races.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        fx.races.push_back(cur);
    }
    for (const toml::array* r : detail::rows(doc, "biome")) {
        fx.biomes.push_back({i64(*r->get(0)), str(*r->get(1)), str(*r->get(2)), i64(*r->get(3)), i64(*r->get(4))});
    }
    for (const toml::array* r : detail::rows(doc, "region")) {
        db::RegionRow row;
        row.id = i64(*r->get(0));
        row.name = str(*r->get(1));
        row.biome = i64(*r->get(2));
        row.cells = i64(*r->get(3));
        row.lake_cells = i64(*r->get(4));
        row.walk_cells = i64(*r->get(5));
        fx.regions.push_back(row);
    }
    for (const toml::array* r : detail::rows(doc, "race")) {
        fx.race_rows.push_back({i64(*r->get(0)), str(*r->get(1)), str(*r->get(2)), str(*r->get(3)), str(*r->get(4)),
                                i64(*r->get(5)), i64(*r->get(6)), i64(*r->get(7)), i64(*r->get(8)), i64(*r->get(9)),
                                nullable(*r->get(10)), i64(*r->get(11))});
    }
    for (const toml::array* r : detail::rows(doc, "culture")) {
        fx.cultures.push_back({i64(*r->get(0)), str(*r->get(1)), str(*r->get(2)), i64(*r->get(3))});
    }
    for (const toml::array* r : detail::rows(doc, "god")) {
        fx.gods.push_back({i64(*r->get(0)), str(*r->get(1)), i64(*r->get(2))});
    }
    for (const toml::array* r : detail::rows(doc, "age")) {
        fx.ages.push_back({i64(*r->get(0)), i64(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)), str(*r->get(4))});
    }
    for (const toml::array* r : detail::rows(doc, "phase")) {
        fx.phases.push_back({i64(*r->get(0)), i64(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)), i64(*r->get(4)),
                             i64(*r->get(5))});
    }
    for (const toml::array* r : detail::rows(doc, "civilization")) {
        fx.civilizations.push_back({i64(*r->get(0)), str(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)),
                                    i64(*r->get(4)), i64(*r->get(5)), i64(*r->get(6)), i64(*r->get(7)),
                                    nullable(*r->get(8)), i64(*r->get(9))});
    }
    std::vector<bool> derive_ending;   // P5-02 8.3: a row of fourteen columns has its ending derived below
    for (const toml::array* r : detail::rows(doc, "settlement")) {
        fx.settlements.push_back({i64(*r->get(0)), str(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)),
                                  i64(*r->get(4)), i64(*r->get(5)), i64(*r->get(6)), i64(*r->get(7)),
                                  i64(*r->get(8)), i64(*r->get(9)), i64(*r->get(10)), i64(*r->get(11)),
                                  nullable(*r->get(12)), i64(*r->get(13)), r->size() > 14U ? i64(*r->get(14)) : 0});
        derive_ending.push_back(r->size() <= 14U);
    }
    for (const toml::array* r : detail::rows(doc, "settlement_census")) {
        fx.census.push_back({i64(*r->get(0)), i64(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)), i64(*r->get(4)),
                             i64(*r->get(5))});
    }
    for (const toml::array* r : detail::rows(doc, "region_control")) {
        fx.region_control.push_back(
            {i64(*r->get(0)), i64(*r->get(1)), nullable(*r->get(2)), i64(*r->get(3)), i64(*r->get(4))});
    }
    for (const toml::array* r : detail::rows(doc, "relation")) {
        db::RelationRow row;
        row.a_type = i64(*r->get(0));
        row.a_id = i64(*r->get(1));
        row.b_type = i64(*r->get(2));
        row.b_id = i64(*r->get(3));
        row.kind = str(*r->get(4));
        row.since = i64(*r->get(5));
        row.until = nullable(*r->get(6));
        row.since_event = i64(*r->get(7));
        row.until_event = i64(*r->get(8));
        fx.relations.push_back(row);
    }
    // Schema 8: [region, from_year, name, culture, event] (P5-12 7.2).
    for (const toml::array* r : detail::rows(doc, "region_name")) {
        fx.region_names.push_back({i64(*r->get(0)), i64(*r->get(1)), str(*r->get(2)), i64(*r->get(3)), i64(*r->get(4))});
    }
    // Schema 9 (P5-02 11.1, 11.2): the five tables in column order; -1 is NULL in monster.died and site.ended_year.
    for (const toml::array* r : detail::rows(doc, "beast")) {
        fx.beasts.push_back({i64(*r->get(0)), str(*r->get(1)), str(*r->get(2)), str(*r->get(3)), i64(*r->get(4)),
                             i64(*r->get(5))});
    }
    for (const toml::array* r : detail::rows(doc, "monster_kind")) {
        fx.monster_kinds.push_back({i64(*r->get(0)), str(*r->get(1)), str(*r->get(2)), str(*r->get(3)),
                                    i64(*r->get(4)), i64(*r->get(5)), i64(*r->get(6))});
    }
    for (const toml::array* r : detail::rows(doc, "beast_population")) {
        fx.beast_populations.push_back({i64(*r->get(0)), i64(*r->get(1)), i64(*r->get(2)), i64(*r->get(3))});
    }
    for (const toml::array* r : detail::rows(doc, "monster")) {
        fx.monsters.push_back({i64(*r->get(0)), str(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)), i64(*r->get(4)),
                               i64(*r->get(5)), nullable(*r->get(6)), i64(*r->get(7)), i64(*r->get(8)),
                               i64(*r->get(9)), i64(*r->get(10))});
    }
    for (const toml::array* r : detail::rows(doc, "site")) {
        fx.sites.push_back({i64(*r->get(0)), str(*r->get(1)), i64(*r->get(2)), i64(*r->get(3)), i64(*r->get(4)),
                            i64(*r->get(5)), i64(*r->get(6)), i64(*r->get(7)), i64(*r->get(8)), i64(*r->get(9)),
                            i64(*r->get(10)), nullable(*r->get(11)), i64(*r->get(12)), i64(*r->get(13))});
    }
    // The catalogue: id = position; roles and values seq = position.
    struct KindInfo {
        std::int64_t id;
        std::int64_t salience;
        std::map<std::string, std::pair<std::int64_t, std::int64_t>> roles;  // role -> (seq, entity type)
    };
    std::map<std::string, KindInfo> by_key;
    if (const toml::array* ks = doc["kind"].as_array()) {
        std::int64_t id = 0;
        for (const toml::node& kn : *ks) {
            const toml::table& k = *kn.as_table();
            ++id;
            db::EventKindRow row{id,
                                 str(*k.get("key")),
                                 i64(*k.get("caused")),
                                 i64(*k.get("region_use")),
                                 i64(*k.get("settlement_use")),
                                 i64(*k.get("salience")),
                                 str(*k.get("template"))};
            KindInfo info{id, row.salience, {}};
            if (const toml::array* rs = k.get_as<toml::array>("roles")) {
                std::int64_t seq = 0;
                for (const toml::node& rn : *rs) {
                    const toml::array& r = *rn.as_array();
                    ++seq;
                    fx.roles.push_back({id, seq, str(*r.get(0)), i64(*r.get(1)), i64(*r.get(2)), i64(*r.get(3))});
                    info.roles[str(*r.get(0))] = {seq, i64(*r.get(1))};
                }
            }
            if (const toml::array* vs = k.get_as<toml::array>("values")) {
                std::int64_t seq = 0;
                for (const toml::node& vn : *vs) {
                    fx.values.push_back({id, ++seq, str(vn)});
                }
            }
            by_key[row.key] = info;
            fx.kinds.push_back(row);
        }
    }
    auto add_event = [&](std::int64_t id, std::int64_t year, std::int64_t end, const std::string& key,
                         std::int64_t phase, std::int64_t region, std::int64_t settlement,
                         std::optional<std::int64_t> salience, const std::vector<std::pair<std::string, std::int64_t>>& ps,
                         const std::vector<std::int64_t>& vs, std::vector<std::int64_t> cs) {
        const auto it = by_key.find(key);
        REQUIRE_MESSAGE(it != by_key.end(), "fixture " << name << ": event " << id << " has kind " << key);
        fx.events.push_back(
            {id, year, end, it->second.id, phase, region, settlement, salience.value_or(it->second.salience)});
        std::int64_t seq = 0;
        for (const auto& [role, x] : ps) {
            const auto r = it->second.roles.find(role);
            REQUIRE_MESSAGE(r != it->second.roles.end(), "fixture " << name << ": event " << id << " role " << role);
            fx.participants.push_back({id, ++seq, r->second.first, r->second.second, x});
        }
        seq = 0;
        for (const std::int64_t v : vs) {
            fx.event_values.push_back({id, ++seq, v});
        }
        std::sort(cs.begin(), cs.end());
        for (const std::int64_t c : cs) {
            fx.causes.push_back({id, c});
        }
    };
    if (const toml::array* es = doc["event"].as_array()) {
        std::int64_t pos = 0;
        for (const toml::node& en : *es) {
            const toml::table& e = *en.as_table();
            ++pos;
            REQUIRE_MESSAGE(i64(*e.get("id")) == pos, "fixture " << name << ": event " << pos << " has another id");
            const std::int64_t year = i64(*e.get("year"));
            std::vector<std::pair<std::string, std::int64_t>> ps;
            if (const toml::array* a = e.get_as<toml::array>("parts")) {
                for (const toml::node& p : *a) {
                    ps.emplace_back(str(*p.as_array()->get(0)), i64(*p.as_array()->get(1)));
                }
            }
            std::vector<std::int64_t> vs;
            if (const toml::array* a = e.get_as<toml::array>("values")) {
                for (const toml::node& v : *a) vs.push_back(i64(v));
            }
            std::vector<std::int64_t> cs;
            if (const toml::array* a = e.get_as<toml::array>("causes")) {
                for (const toml::node& v : *a) cs.push_back(i64(v));
            }
            add_event(pos, year, e.contains("end") ? i64(*e.get("end")) : year, str(*e.get("kind")),
                      i64(*e.get("phase")), e.contains("region") ? i64(*e.get("region")) : 0,
                      e.contains("settlement") ? i64(*e.get("settlement")) : 0,
                      e.contains("salience") ? std::optional<std::int64_t>(i64(*e.get("salience"))) : std::nullopt, ps,
                      vs, cs);
        }
    }
    if (const toml::array* bs = doc["bulk"].as_array()) {
        for (const toml::node& bn : *bs) {
            const toml::table& b = *bn.as_table();
            const std::int64_t count = i64(*b.get("count"));
            for (std::int64_t i = 1; i <= count; ++i) {
                const std::int64_t sal =
                    b.contains("salience") ? i64(*b.get("salience"))
                                           : (i64(*b.get("salience_mul")) * i) % i64(*b.get("salience_mod"));
                std::vector<std::int64_t> vs;
                if (b.contains("value") && str(*b.get("value")) == "index") {
                    vs.push_back(i);
                }
                const std::int64_t year = i64(*b.get("year"));
                add_event(static_cast<std::int64_t>(fx.events.size()) + 1, year, year, str(*b.get("kind")),
                          i64(*b.get("phase")), 0, 0, sal, {}, vs, {});
            }
        }
    }
    // P5-02 8.3: the ending of a fourteen-column settlement row, from the log (status 3: its ruin_event; status 4:
    // the cause of its ruin_faded event), as the reference's expand_fixture derives it.
    {
        std::map<std::int64_t, std::string> key_of_kind;
        for (const auto& k : fx.kinds) key_of_kind[k.id] = k.key;
        std::map<std::int64_t, std::string> key_of_event;
        std::map<std::int64_t, std::int64_t> settlement_of;
        for (const auto& e : fx.events) {
            key_of_event[e.id] = key_of_kind[e.kind];
            settlement_of[e.id] = e.settlement;
        }
        std::map<std::int64_t, std::vector<std::int64_t>> causes_of;
        for (const auto& c : fx.causes) causes_of[c.event].push_back(c.cause);
        std::map<std::int64_t, std::int64_t> faded_ruin;
        for (const auto& [e, k] : key_of_event) {
            if (k == "ruin_faded" && !causes_of[e].empty()) faded_ruin[settlement_of[e]] = causes_of[e].front();
        }
        for (std::size_t i = 0; i < fx.settlements.size(); ++i) {
            if (!derive_ending[i]) continue;
            db::SettlementRow& s = fx.settlements[i];
            if (s.status == 1 || s.status == 2) {
                s.ending = 0;
                continue;
            }
            const std::int64_t ruin = s.status == 3 ? s.ruin_event : faded_ruin[s.id];
            const std::string& k = key_of_event[ruin];
            std::int64_t ending = 5;
            if (k == "sacked") {
                ending = 1;
            } else if (k == "settlement_overrun") {
                ending = 2;
            } else if (k == "settlement_abandoned" || k == "settlement_abandoned_to") {
                bool plague = false, famine = false;
                for (const std::int64_t c : causes_of[ruin]) {
                    const std::string& ck = key_of_event[c];
                    plague = plague || ck == "plague" || ck == "plague_spread" || ck == "plague_outbreak";
                    famine = famine || ck == "famine";
                }
                ending = plague ? 3 : famine ? 4 : 5;
            }
            s.ending = ending;
        }
    }
    // Primary-key order (the writer requires ascending keys).
    auto by = [](auto key) { return [key](const auto& a, const auto& b) { return key(a) < key(b); }; };
    std::sort(fx.biomes.begin(), fx.biomes.end(), by([](const db::BiomeRow& r) { return r.id; }));
    std::sort(fx.regions.begin(), fx.regions.end(), by([](const db::RegionRow& r) { return r.id; }));
    std::sort(fx.race_rows.begin(), fx.race_rows.end(), by([](const db::RaceRow& r) { return r.id; }));
    std::sort(fx.cultures.begin(), fx.cultures.end(), by([](const db::CultureRow& r) { return r.id; }));
    std::sort(fx.gods.begin(), fx.gods.end(), by([](const db::GodRow& r) { return r.id; }));
    std::sort(fx.ages.begin(), fx.ages.end(), by([](const db::AgeRow& r) { return r.id; }));
    std::sort(fx.phases.begin(), fx.phases.end(), by([](const db::PhaseRow& r) { return r.id; }));
    std::sort(fx.civilizations.begin(), fx.civilizations.end(), by([](const db::CivilizationRow& r) { return r.id; }));
    std::sort(fx.settlements.begin(), fx.settlements.end(), by([](const db::SettlementRow& r) { return r.id; }));
    std::sort(fx.census.begin(), fx.census.end(),
              by([](const db::SettlementCensusRow& r) { return std::make_pair(r.settlement, r.year); }));
    std::sort(fx.region_control.begin(), fx.region_control.end(),
              by([](const db::RegionControlRow& r) { return std::make_pair(r.region, r.from_year); }));
    std::sort(fx.relations.begin(), fx.relations.end(), by([](const db::RelationRow& r) {
                  return std::make_tuple(r.a_type, r.a_id, r.b_type, r.b_id, r.kind, r.since);
              }));
    std::sort(fx.region_names.begin(), fx.region_names.end(),
              by([](const db::RegionNameRow& r) { return std::make_pair(r.region, r.from_year); }));
    return fx;
}

// --- Writing a fixture world through the pipeline ----------------------------------------------

namespace detail {

inline const Fixture*& current() {
    static const Fixture* f = nullptr;
    return f;
}

inline void stage_biomes(worldgen::stages::StageContext& ctx) {
    for (const auto& r : current()->biomes) ctx.write_biome(r);
}
inline void stage_regions(worldgen::stages::StageContext& ctx) {
    for (const auto& r : current()->regions) ctx.write_region(r);
}
inline void stage_chronology(worldgen::stages::StageContext& ctx) {
    const Fixture& f = *current();
    for (const auto& r : f.race_rows) ctx.write_race(r);
    for (const auto& r : f.cultures) ctx.write_culture(r);
    for (const auto& r : f.gods) ctx.write_god(r);
    for (const auto& r : f.ages) ctx.write_age(r);
}
inline void stage_history(worldgen::stages::StageContext& ctx) {
    const Fixture& f = *current();
    for (const auto& r : f.kinds) ctx.write_event_kind(r);
    for (const auto& r : f.roles) ctx.write_event_kind_role(r);
    for (const auto& r : f.values) ctx.write_event_kind_value(r);
    for (const auto& r : f.phases) ctx.write_phase(r);
    for (const auto& r : f.events) ctx.write_event(r);
    for (const auto& r : f.participants) ctx.write_event_participant(r);
    for (const auto& r : f.causes) ctx.write_event_cause(r);
    for (const auto& r : f.event_values) ctx.write_event_value(r);
    for (const auto& r : f.civilizations) ctx.write_civilization(r);
    for (const auto& r : f.settlements) ctx.write_settlement(r);
    for (const auto& r : f.census) ctx.write_settlement_census(r);
    for (const auto& r : f.region_control) ctx.write_region_control(r);
    for (const auto& r : f.relations) ctx.write_relation(r);
    for (const auto& r : f.region_names) ctx.write_region_name(r);
    // P5-02 11.1: GUESS the StageContext writers, as write_region_name
    for (const auto& r : f.beasts) ctx.write_beast(r);
    for (const auto& r : f.monster_kinds) ctx.write_monster_kind(r);
    for (const auto& r : f.beast_populations) ctx.write_beast_population(r);
    for (const auto& r : f.monsters) ctx.write_monster(r);
    for (const auto& r : f.sites) ctx.write_site(r);
}

inline const dbtest::TempDir& fixture_dir() {
    static const dbtest::TempDir d("history-report");
    return d;
}

}  // namespace detail

// Writes fixture `fx` to `path` (replacing it).
inline void write_fixture_world(const Fixture& fx, const std::string& path) {
    static const worldgen::data::ContentSet empty;
    worldgen::WorldParams p;
    p.seed = fx.seed;
    p.map_size = fx.map_size;
    p.content_hash = worldgen::data::content_hash(empty);
    p.history.span = fx.span;
    p.history.races = fx.races;
    p.threads = 1;
    const worldgen::stages::StageInfo stages[] = {{"biomes", &detail::stage_biomes},
                                                  {"regions", &detail::stage_regions},
                                                  {"chronology", &detail::stage_chronology},
                                                  {"history", &detail::stage_history}};
    detail::current() = &fx;
    const db::Status s =
        worldgen::stages::testing::generate_world_with_stages(p, empty, stages, path, db::Overwrite::replace);
    detail::current() = nullptr;
    REQUIRE_MESSAGE(s.ok(), "fixture " << fx.name << ": " << (s.ok() ? "" : s.error().message));
}

// The world of fixture `name`, written once per process; returns its path.
inline std::string fixture_world(const std::string& name) {
    static std::map<std::string, std::string> cache;
    const auto it = cache.find(name);
    if (it != cache.end()) {
        return it->second;
    }
    const Fixture fx = load_fixture(name);
    const std::string path = detail::fixture_dir().file(name + ".db");
    write_fixture_world(fx, path);
    return cache[name] = path;
}

// A fresh copy of fixture `name` at a new path (to corrupt it).
inline std::string fixture_copy(const std::string& name, const std::string& label) {
    static int counter = 0;
    const std::string path = detail::fixture_dir().file(label + "-" + std::to_string(++counter) + ".db");
    std::error_code ec;
    fs::copy_file(worldgen::db::path_from_utf8(fixture_world(name)), worldgen::db::path_from_utf8(path),
                  fs::copy_options::overwrite_existing, ec);
    REQUIRE_MESSAGE(!ec, "cannot copy fixture " << name);
    return path;
}

inline db::WorldReader open_world(const std::string& path) {
#ifdef UNMAPPED_P3_07_STUB_PATH_HOOK
    UNMAPPED_P3_07_STUB_PATH_HOOK(path);  // the test writer's harness that runs the reference in place of the C++
#endif
    auto r = db::WorldReader::open(path);
    REQUIRE_MESSAGE(r.ok(), "cannot open " << path << ": " << (r.ok() ? "" : r.error().message));
    return std::move(r).value();
}

// hash --detail's table lines: name -> hex.
inline std::map<std::string, std::string> table_hashes(const std::string& path) {
    const db::WorldReader r = open_world(path);
    auto d = worldgen::inspect::detail_hashes(r);
    REQUIRE_MESSAGE(d.ok(), "detail_hashes failed for " << path);
    std::map<std::string, std::string> out;
    for (const auto& t : d.value().tables) {
        out[t.name] = worldgen::to_hex(t.digest);
    }
    return out;
}

// --- The golden file ---------------------------------------------------------------------------

struct GoldenCase {
    std::map<std::string, std::string> report;                  // level -> sha256 hex
    std::vector<std::pair<std::string, std::int64_t>> stats;    // in block order (world cases)
    std::map<std::string, std::string> tables;                  // fixture table hashes
};

inline const std::map<std::string, GoldenCase>& golden() {
    static const std::map<std::string, GoldenCase> g = [] {
        std::map<std::string, GoldenCase> out;
        std::istringstream in(detail::text_of_file(fs::path(UNMAPPED_P3_07_GOLDEN_PATH)));
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream ls(line);
            std::string name, what, a, b;
            ls >> name >> what >> a;
            if (what == "stats") {
                ls >> b;
                out[name].stats.emplace_back(a, std::stoll(b));
            } else if (what == "table") {
                ls >> b;
                out[name].tables[a] = b;
            } else {
                out[name].report[what] = a;
            }
        }
        return out;
    }();
    return g;
}

inline const GoldenCase& golden_case(const std::string& name) {
    const auto it = golden().find(name);
    REQUIRE_MESSAGE(it != golden().end(), "no golden lines for " << name);
    return it->second;
}

// --- Text helpers ------------------------------------------------------------------------------

inline std::vector<std::string> lines_of(std::string_view text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t nl = text.find('\n', at);
        const std::size_t end = nl == std::string_view::npos ? text.size() : nl;
        out.emplace_back(text.substr(at, end - at));
        at = end + 1;
    }
    return out;
}

// The lines of block `head` (e.g. "[present]"), without the heading, up to the next block heading.
inline std::vector<std::string> block(std::string_view text, std::string_view head) {
    const std::vector<std::string> all = lines_of(text);
    std::vector<std::string> out;
    bool in = false;
    for (const std::string& l : all) {
        const bool heading = !l.empty() && l.front() == '[' && l.back() == ']' && l.find(' ') == std::string::npos;
        if (heading) {
            if (in) break;
            in = l == head;
            continue;
        }
        if (in) out.push_back(l);
    }
    return out;
}

// The lines of civilization c's block in [civilizations], heading included.
inline std::vector<std::string> civ_block(std::string_view text, std::int64_t c) {
    const std::vector<std::string> civs = block(text, "[civilizations]");
    std::vector<std::string> out;
    const std::string head = "c" + std::to_string(c) + " ";
    bool in = false;
    for (const std::string& l : civs) {
        if (!l.empty() && l[0] != ' ') {
            in = l.rfind(head, 0) == 0;
        }
        if (in) out.push_back(l);
    }
    return out;
}

inline std::map<std::string, std::int64_t> stats_of(std::string_view text) {
    std::map<std::string, std::int64_t> out;
    for (const std::string& l : block(text, "[stats]")) {
        const std::size_t eq = l.find(" = ");
        REQUIRE_MESSAGE(eq != std::string::npos, "not a stats line: " << l);
        out[l.substr(0, eq)] = std::stoll(l.substr(eq + 3));
    }
    return out;
}

inline std::vector<std::pair<std::string, std::int64_t>> stats_list(std::string_view text) {
    std::vector<std::pair<std::string, std::int64_t>> out;
    for (const std::string& l : block(text, "[stats]")) {
        const std::size_t eq = l.find(" = ");
        REQUIRE_MESSAGE(eq != std::string::npos, "not a stats line: " << l);
        out.emplace_back(l.substr(0, eq), std::stoll(l.substr(eq + 3)));
    }
    return out;
}

inline bool contains_line(const std::vector<std::string>& lines, std::string_view line) {
    return std::find(lines.begin(), lines.end(), line) != lines.end();
}

}  // namespace hrtest
