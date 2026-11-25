#pragma once

// Test support for spec P4-01 (the timeline in the library): fixture T1 (worldgen/tests/data/timeline/,
// the P3-07 fixture format with [[layer]] entries) written through the writer API, the P3-07 fixtures,
// generated worlds cached per process, the canonical dumps of 13.1 computed from the library's answers,
// the territory hash of a world, and the golden file worldgen/tests/golden/p4-01-timeline.txt (written by
// reference/p4_timeline_reference.py).
//
// Written from the spec only (docs/specs/p4-01-timeline.md); the library's API is that of section 5.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <toml.hpp>

#include "db_test_support.hpp"
#include "history_report_fixture.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/territory.hpp"
#include "worldgen/stages/stage.hpp"
#include "worldgen/stages/testing.hpp"

#ifndef UNMAPPED_P4_01_FIXTURE_DIR
#error "UNMAPPED_P4_01_FIXTURE_DIR must name worldgen/tests/data/timeline"
#endif
#ifndef UNMAPPED_P4_01_GOLDEN_PATH
#error "UNMAPPED_P4_01_GOLDEN_PATH must name worldgen/tests/golden/p4-01-timeline.txt"
#endif
#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace tltest {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace wh = worldgen::history;
namespace wi = worldgen::inspect;
namespace wd = worldgen::data;

inline const std::string T1 = "t1-edges";

// --- Fixture T1: the P3-07 rows plus layers --------------------------------------------------------

struct FixtureLayer {
    std::string name;
    db::LayerGrid grid;
};

namespace detail {

inline std::string text_of(const fs::path& p) { return hrtest::detail::text_of_file(p); }

inline std::int64_t node_i64(const toml::node& n) {
    const auto v = n.value<std::int64_t>();
    REQUIRE_MESSAGE(v.has_value(), "T1: expected an integer");
    return *v;
}

// A grid of the type named by `type` ("u8", "u16", "i16") with the cells given (row-major).
inline db::LayerGrid make_grid(const std::string& type, std::uint32_t w, std::uint32_t h,
                               const std::vector<std::int64_t>& cells) {
    REQUIRE(cells.size() == std::size_t{w} * h);
    db::LayerGrid g;
    g.width = w;
    g.height = h;
    if (type == "u8") {
        std::vector<std::uint8_t> v;
        for (const std::int64_t c : cells) v.push_back(static_cast<std::uint8_t>(c));
        g.values = std::move(v);
    } else if (type == "u16") {
        std::vector<std::uint16_t> v;
        for (const std::int64_t c : cells) v.push_back(static_cast<std::uint16_t>(c));
        g.values = std::move(v);
    } else if (type == "i16") {
        std::vector<std::int16_t> v;
        for (const std::int64_t c : cells) v.push_back(static_cast<std::int16_t>(c));
        g.values = std::move(v);
    } else {
        FAIL("T1: layer type " << type << " is not used by the fixture");
    }
    return g;
}

inline const hrtest::Fixture*& t1_rows() {
    static const hrtest::Fixture* f = nullptr;
    return f;
}
inline const std::vector<FixtureLayer>*& t1_layers() {
    static const std::vector<FixtureLayer>* l = nullptr;
    return l;
}
// The stage "biomes" of the T1 world: the biome rows, then the six layers in file order (ids 1 to 6).
inline void stage_biomes_and_layers(worldgen::stages::StageContext& ctx) {
    hrtest::detail::stage_biomes(ctx);
    for (const FixtureLayer& l : *t1_layers()) ctx.write_layer(l.name, l.grid);
}

inline const dbtest::TempDir& scratch() {
    static const dbtest::TempDir d("timeline");
    return d;
}

}  // namespace detail

inline fs::path t1_toml() { return fs::path(UNMAPPED_P4_01_FIXTURE_DIR) / "t1-edges.toml"; }

// The rows of T1 (hrtest's expansion of the P3-07 format; the path given is absolute, so it replaces the
// P3-07 fixture directory).
inline const hrtest::Fixture& t1_fixture() {
    static const hrtest::Fixture fx = hrtest::load_fixture(db::path_to_utf8(t1_toml().replace_extension()));
    return fx;
}

// T1's [[layer]] entries, in file order.
inline const std::vector<FixtureLayer>& t1_layers() {
    static const std::vector<FixtureLayer> layers = [] {
        const fs::path path = t1_toml();
        const toml::table doc = toml::parse(detail::text_of(path), path.string());
        std::vector<FixtureLayer> out;
        const toml::array* ls = doc["layer"].as_array();
        REQUIRE(ls != nullptr);
        for (const toml::node& n : *ls) {
            const toml::table& t = *n.as_table();
            std::vector<std::int64_t> cells;
            for (const toml::node& c : *t.get_as<toml::array>("cells")) cells.push_back(detail::node_i64(c));
            const auto w = static_cast<std::uint32_t>(detail::node_i64(*t.get("width")));
            const auto h = static_cast<std::uint32_t>(detail::node_i64(*t.get("height")));
            out.push_back({*t.get("name")->value<std::string>(),
                           detail::make_grid(*t.get("type")->value<std::string>(), w, h, cells)});
        }
        return out;
    }();
    return layers;
}

inline const db::LayerGrid& t1_layer(std::string_view name) {
    for (const FixtureLayer& l : t1_layers()) {
        if (l.name == name) return l.grid;
    }
    FAIL("T1 has no layer " << name);
    return t1_layers().front().grid;  // not reached
}

// T1's biome table as the composites take it.
inline std::vector<wi::BiomeColour> t1_biome_colours() {
    std::vector<wi::BiomeColour> out;
    for (const db::BiomeRow& b : t1_fixture().biomes) out.push_back({b.id, b.colour});
    return out;
}

// Writes T1 to `path` (replacing it) through the pipeline: biomes (rows and layers), regions,
// chronology, history.
inline void write_t1_world(const std::string& path) {
    static const wd::ContentSet empty;
    const hrtest::Fixture& fx = t1_fixture();
    worldgen::WorldParams p;
    p.seed = fx.seed;
    p.map_size = fx.map_size;
    p.content_hash = wd::content_hash(empty);
    p.history.span = fx.span;
    p.history.races = fx.races;
    p.threads = 1;
    const worldgen::stages::StageInfo stages[] = {{"biomes", &detail::stage_biomes_and_layers},
                                                  {"regions", &hrtest::detail::stage_regions},
                                                  {"chronology", &hrtest::detail::stage_chronology},
                                                  {"history", &hrtest::detail::stage_history}};
    hrtest::detail::current() = &fx;
    detail::t1_layers() = &t1_layers();
    const db::Status s =
        worldgen::stages::testing::generate_world_with_stages(p, empty, stages, path, db::Overwrite::replace);
    hrtest::detail::current() = nullptr;
    detail::t1_layers() = nullptr;
    REQUIRE_MESSAGE(s.ok(), "T1: " << (s.ok() ? "" : s.error().message));
}

// The T1 world, written once per process.
inline const std::string& t1_world() {
    static const std::string path = [] {
        const std::string p = detail::scratch().file("t1-edges.db");
        write_t1_world(p);
        return p;
    }();
    return path;
}

// The world of a fixture case: T1 or one of the P3-07 fixtures.
inline std::string fixture_world(const std::string& name) {
    return name == T1 ? t1_world() : hrtest::fixture_world(name);
}

// A fresh copy of T1 at a new path (to corrupt it).
inline std::string t1_copy(const std::string& label) {
    static int counter = 0;
    const std::string path = detail::scratch().file(label + "-" + std::to_string(++counter) + ".db");
    std::error_code ec;
    fs::copy_file(db::path_from_utf8(t1_world()), db::path_from_utf8(path), fs::copy_options::overwrite_existing,
                  ec);
    REQUIRE_MESSAGE(!ec, "cannot copy T1");
    return path;
}

// --- Generated worlds ----------------------------------------------------------------------------

inline const wd::ContentSet& shipped() {
    static const wd::ContentSet c = [] {
        auto r = wd::load_content(wd::builtin_kinds(), fs::path(UNMAPPED_TEST_SOURCE_ROOT) / "game" / "data");
        REQUIRE_MESSAGE(r.ok(), "game/data does not load");
        return std::move(*r.value);
    }();
    return c;
}

// A world of the shipped data: the seed (or a phrase), map size, span, races (empty: the default six),
// detail and stage limit; written once per process and key.
struct WorldSpec {
    std::string name;  // unique key and file stem
    std::uint64_t seed = 0;
    std::string phrase;
    std::int64_t size = 512;
    std::int64_t span = 6000;
    std::vector<std::string> races;
    wd::HistoryDetail detail = wd::HistoryDetail::normal;
    std::optional<std::string> until = std::nullopt;  // --until; nullopt: every stage
};

inline const std::vector<std::string>& six_races() {
    static const std::vector<std::string> v = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};
    return v;
}

inline std::string world(const WorldSpec& c) {
    static std::map<std::string, std::string> cache;
    const auto it = cache.find(c.name);
    if (it != cache.end()) return it->second;
    worldgen::WorldParams p;
    p.seed_phrase = c.phrase;
    p.seed = p.seed_phrase.empty() ? c.seed : worldgen::phrase_seed(p.seed_phrase);
    p.map_size = c.size;
    p.content_hash = wd::content_hash(shipped());
    p.history.span = c.span;
    p.history.races = c.races.empty() ? six_races() : c.races;
    p.history.detail = c.detail;
    p.until = c.until;
    p.threads = 1;
    const std::string path = detail::scratch().file(c.name + ".db");
    const db::Status s = worldgen::generate_world(p, shipped(), path, db::Overwrite::replace);
    REQUIRE_MESSAGE(s.ok(), "generate_world " << c.name << ": " << (s.ok() ? "" : s.error().message));
    return cache[c.name] = path;
}

// The world cases of 13.4 (the cases of tools/ci/world-hashes.sh with history, and sample-42-1024).
inline const std::vector<WorldSpec>& world_cases_512() {
    static const std::vector<WorldSpec> v = {
        {"geo-phrase-512", 0, "the unmapped", 512, 6000, {}, wd::HistoryDetail::normal},
        {"geo-seed-0-512", 0, "", 512, 6000, {}, wd::HistoryDetail::normal},
        {"geo-seed-42-512", 42, "", 512, 6000, {}, wd::HistoryDetail::normal},
        {"hist-span-2000-512", 7, "", 512, 2000, {}, wd::HistoryDetail::normal},
    };
    return v;
}
inline const std::vector<WorldSpec>& world_cases_1024() {
    static const std::vector<WorldSpec> v = {
        {"geo-seed-1-1024", 1, "", 1024, 6000, {}, wd::HistoryDetail::normal},
        {"hist-dense-1024", 11, "", 1024, 6000, {}, wd::HistoryDetail::dense},
        {"hist-races-elf-orc-1024", 3, "", 1024, 6000, {"elf", "orc"}, wd::HistoryDetail::normal},
        {"sample-42-1024", 42, "", 1024, 6000, {}, wd::HistoryDetail::normal},
    };
    return v;
}
// Property worlds of 13.5 item 8: 512 seeds 0 to 19, 1024 seeds 0 to 9, default settings. The keys equal
// the world cases' where the settings are the same, so that each is generated once per process.
inline WorldSpec property_world(std::uint64_t seed, std::int64_t size) {
    WorldSpec s;
    s.seed = seed;
    s.size = size;
    if (size == 512 && seed == 0) {
        s.name = "geo-seed-0-512";
    } else if (size == 512 && seed == 42) {
        s.name = "geo-seed-42-512";
    } else if (size == 1024 && seed == 1) {
        s.name = "geo-seed-1-1024";
    } else {
        s.name = "seed-" + std::to_string(seed) + "-" + std::to_string(size);
    }
    return s;
}

// --- Opening a world --------------------------------------------------------------------------------

// A world's reader and its loaded history (load_history REQUIREd to succeed).
struct Opened {
    db::WorldReader reader;
    wh::LoadedHistory h;
};

inline db::WorldReader open_reader(const std::string& path) {
    auto r = db::WorldReader::open(path);
    REQUIRE_MESSAGE(r.ok(), "cannot open " << path << ": " << (r.ok() ? "" : r.error().message));
    return std::move(r).value();
}

inline wh::LoadedHistory load(const db::WorldReader& r) {
    auto h = wh::load_history(r);
    REQUIRE_MESSAGE(h.ok(), "load_history failed: " << (h.ok() ? "" : h.error().message));
    return std::move(h).value();
}

// --- The canonical dumps of 13.1 ------------------------------------------------------------------

inline std::string num(std::int64_t v) { return std::to_string(v); }

inline std::string list_text(std::span<const std::int64_t> ids) {
    if (ids.empty()) return "-";
    std::string s;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) s += ' ';
        s += num(ids[i]);
    }
    return s;
}

// D(Y) of a YearState. P5-02 13 (P4-01 r4; the test writer's line formats, reading P5 of the reference: 13 gives
// none): the settlement lines gain the ending at Y (GUESS SettlementState::ending), and with the timeline D(Y)
// ends with `site <id>` per site open at Y and `monster <id> <awake> <lair> <region>` per monster alive at Y.
// P5-03 18 (rule set p5-03): with the timeline and the world's race ids (ascending), D(Y) ends with `races <a> <b>
// <met> <disposition> <band>` per pair a < b, then `mix <s> <race> <share> <standing>` per settlement standing at Y
// with more than one race, ascending.
inline std::string year_dump(const wh::YearState& st, const wh::Timeline* t = nullptr,
                             const std::vector<std::int64_t>* races = nullptr) {
    std::string out = "year " + num(st.year) + " phase " + num(st.phase) + " age " + num(st.age) + "\n";
    for (const wh::RegionHolding& r : st.regions) {
        out += "hold " + num(r.region) + " " + num(r.civ) + " " + num(r.since) + " " + num(r.event) + "\n";
    }
    for (const wh::SettlementState& s : st.settlements) {
        out += "settlement " + num(s.settlement) + " " + (s.exists ? "1" : "0") + " " + num(s.owner) + " " +
               num(s.kind) + " " + num(s.status) + " " + num(s.population) + " " + num(s.since) + " " +
               num(s.ending) + "\n";
    }
    for (const std::size_t i : st.relations) out += "relation " + std::to_string(i) + "\n";
    for (const std::int64_t c : st.civilizations) out += "alive " + num(c) + "\n";
    for (const std::int64_t e : st.events) out += "event " + num(e) + "\n";
    for (const std::int64_t e : st.events_starting) out += "start " + num(e) + "\n";
    if (t != nullptr) {
        for (const std::int64_t site : t->sites_existing(st.year)) {
            if (t->site_at(site, st.year).open) out += "site " + num(site) + "\n";
        }
        for (std::int64_t m = 1; m <= static_cast<std::int64_t>(t->monster_count()); ++m) {
            const wh::MonsterState ms = t->monster_at(m, st.year);
            if (ms.alive) {
                out += "monster " + num(m) + " " + (ms.awake ? "1" : "0") + " " + num(ms.lair_site) + " " +
                       num(ms.region) + "\n";
            }
        }
        if (races != nullptr) {
            for (std::size_t i = 0; i < races->size(); ++i) {
                for (std::size_t j = i + 1; j < races->size(); ++j) {
                    const wh::RaceRelationState r = t->race_relation_at((*races)[i], (*races)[j], st.year);
                    out += "races " + num(r.race_a) + " " + num(r.race_b) + " " + (r.met ? "1" : "0") + " " +
                           num(r.disposition) + " " + num(r.band) + "\n";
                }
            }
            for (const wh::SettlementState& s : st.settlements) {
                const std::vector<wh::RaceShare> mx = t->settlement_races(s.settlement, st.year);
                if (mx.size() > 1U) {
                    for (const wh::RaceShare& x : mx) {
                        out += "mix " + num(s.settlement) + " " + num(x.race) + " " + num(x.share) + " " +
                               num(x.standing) + "\n";
                    }
                }
            }
        }
        // P5-04 12 (P4-01 r6; reading T1 of the reference: after every earlier line of D(Y)): `alliance <a> <b>
        // <since>` per alliance row in force at Y, then `trade <a> <b> <since>` per trade row, ascending (a, b)
        for (const wh::AllianceState& a : t->alliances_at(st.year)) {
            out += "alliance " + num(a.a) + " " + num(a.b) + " " + num(a.since) + "\n";
        }
        std::vector<std::pair<std::pair<std::int64_t, std::int64_t>, std::int64_t>> trades;
        for (const db::RelationRow& r : t->relations()) {
            if (r.kind == "trade" && r.since <= st.year && (!r.until || st.year < *r.until)) {
                trades.push_back({{r.a_id, r.b_id}, r.since});
            }
        }
        std::sort(trades.begin(), trades.end());
        for (const auto& [ab, since] : trades) {
            out += "trade " + num(ab.first) + " " + num(ab.second) + " " + num(since) + "\n";
        }
    }
    return out;
}

inline std::vector<std::int64_t> race_ids(const wh::LoadedHistory& h) {
    std::vector<std::int64_t> v;
    for (const db::RaceRow& r : h.tables.races) v.push_back(r.id);
    std::sort(v.begin(), v.end());
    return v;
}

// S of a loaded history.
inline std::string static_dump(const wh::LoadedHistory& h) {
    const wh::Timeline& t = h.timeline;
    std::string out = "end " + num(t.end_year()) + "\n";
    for (const std::int64_t y : t.holder_change_years()) out += "change " + num(y) + "\n";
    const std::vector<wi::Rgb> colours = wi::civilization_colours(h.tables);
    REQUIRE(colours.size() == h.tables.civilizations.size());
    for (std::size_t i = 0; i < colours.size(); ++i) {
        out += "colour " + num(h.tables.civilizations[i].id) + " " + std::to_string(colours[i].r) + " " +
               std::to_string(colours[i].g) + " " + std::to_string(colours[i].b) + "\n";
    }
    for (const db::CivilizationRow& c : h.tables.civilizations) {
        out += "civ " + num(c.id) + " " + list_text(t.events_of_civilization(c.id)) + "\n";
    }
    for (const wh::RegionInfo& r : h.tables.regions) {
        out += "region " + num(r.id) + " " + list_text(t.events_of_region(r.id)) + "\n";
    }
    // P4-01 r3 16 item 4: one line per region_name row in primary-key order. The name is read through
    // Timeline::region_name at the row's from_year (item 1: a region renamed in Y bears its new name at Y),
    // so the golden hash also pins the API's answer at every renaming year.
    for (const db::RegionNameRow& n : h.tables.region_names) {
        out += "name " + num(n.region) + " " + num(n.from_year) + " " +
               std::string(t.region_name(n.region, n.from_year)) + "\n";
    }
    for (const db::SettlementRow& s : h.tables.settlements) {
        out += "settlement " + num(s.id) + " " + list_text(t.events_of_settlement(s.id)) + "\n";
    }
    // P5-02 13 (the reference's reading P5): site, monster and beast lines after the settlement lines.
    // GUESS HistoryTables::sites, monsters and beast_populations (db rows of schema 9).
    for (const db::SiteRow& x : h.tables.sites) {
        out += "site " + num(x.id) + " " + num(x.kind) + " " + num(x.region) + " " + num(x.x) + " " + num(x.y) + " " +
               num(x.settlement) + " " + num(x.founder_civ) + " " + num(x.monster) + " " + num(x.origin_year) + " " +
               (x.ended_year ? num(*x.ended_year) : std::string("-")) + " " + num(x.ending) + "\n";
    }
    for (const db::MonsterRow& m : h.tables.monsters) {
        out += "monster " + num(m.id) + " " + num(m.kind) + " " + num(m.born) + " " +
               (m.died ? num(*m.died) : std::string("-")) + " " + num(m.status) + " " + num(m.lair_site) + "\n";
    }
    for (const db::BeastPopulationRow& b : h.tables.beast_populations) {
        out += "beast " + num(b.region) + " " + num(b.beast) + " " + num(b.year) + " " + num(b.count) + "\n";
    }
    // P5-03 18: after the beast lines, every race_relation and settlement_race row in key order
    for (const db::RaceRelationRow& r : h.tables.race_relations) {
        out += "relation " + num(r.race_a) + " " + num(r.race_b) + " " + num(r.year) + " " + num(r.disposition) + " " +
               num(r.band) + "\n";
    }
    for (const db::SettlementRaceRow& r : h.tables.settlement_races) {
        out += "share " + num(r.settlement) + " " + num(r.race) + " " + num(r.year) + " " + num(r.share) + " " +
               num(r.standing) + "\n";
    }
    for (const wh::HistoryView::Event& e : h.view.events()) {
        out += "effects " + num(e.row.id) + " " + list_text(t.effects(e.row.id)) + "\n";
    }
    return out;
}

inline std::string sha(std::string_view text) { return worldgen::sha256_hex(text); }

// The golden years of 13.4 item 2: {0, E - 1, E, floor(E / 2)}, the age start years and the holder
// change years, ascending, each once.
inline std::vector<std::int64_t> golden_years(const wh::LoadedHistory& h) {
    std::vector<std::int64_t> v = {0, h.tables.end_year - 1, h.tables.end_year, h.tables.end_year / 2};
    for (const db::AgeRow& a : h.tables.ages) v.push_back(a.start_year);
    for (const std::int64_t y : h.timeline.holder_change_years()) v.push_back(y);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

// SHA-256 of D(0) ... D(E), through the reusing overload of year_state.
inline std::string years_hash(const wh::Timeline& t, const std::vector<std::int64_t>& races) {
    worldgen::Sha256 m;
    wh::YearState st;
    for (std::int64_t y = 0; y <= t.end_year(); ++y) {
        t.year_state(y, st);
        m.update(std::string_view(year_dump(st, &t, &races)));
    }
    return worldgen::to_hex(m.final());
}

// --- Territory of a world -------------------------------------------------------------------------

struct WorldLayers {
    db::LayerGrid elevation, biome, region;
    std::optional<db::LayerGrid> water, river, flow_dir;
    std::vector<wi::BiomeColour> biome_table;
};

inline WorldLayers read_layers(const db::WorldReader& r) {
    WorldLayers out;
    auto need = [&](const char* name) {
        auto l = r.read_layer(name);
        REQUIRE_MESSAGE(l.ok(), "layer " << name << ": " << (l.ok() ? "" : l.error().message));
        return std::move(l.value().grid);
    };
    auto maybe = [&](const char* name) -> std::optional<db::LayerGrid> {
        auto l = r.read_layer(name);
        if (!l.ok()) {
            REQUIRE(l.code() == db::ErrorCode::layer_not_found);
            return std::nullopt;
        }
        return std::move(l.value().grid);
    };
    out.elevation = need("elevation");
    out.biome = need("biome");
    out.region = need("region");
    out.water = maybe("water");
    out.river = maybe("river");
    out.flow_dir = maybe("flow_dir");
    auto rows = db::read_biome_rows(r);
    REQUIRE(rows.ok());
    for (const db::BiomeRow& b : rows.value()) out.biome_table.push_back({b.id, b.colour});
    return out;
}

inline wi::TerritoryBase territory_base(const WorldLayers& l) {
    return wi::TerritoryBase::build(l.elevation, l.water ? &*l.water : nullptr, l.river ? &*l.river : nullptr,
                                    l.flow_dir ? &*l.flow_dir : nullptr, l.biome, l.region, l.biome_table);
}

inline std::vector<std::uint8_t> regions_composite(const WorldLayers& l, std::uint32_t scale) {
    return wi::render_composite_regions(l.elevation, l.water ? &*l.water : nullptr, l.river ? &*l.river : nullptr,
                                        l.flow_dir ? &*l.flow_dir : nullptr, l.biome, l.region, scale,
                                        l.biome_table);
}

// The territory years of 13.4 item 4: {the smallest holder change year, floor(E / 2), E}, ascending, each
// once (the first omitted when no region ever changed hands).
inline std::vector<std::int64_t> territory_years(const wh::LoadedHistory& h) {
    std::vector<std::int64_t> v;
    v.reserve(3);   // (GCC 15's -Warray-bounds false positive on the initializer list then push_back, -O2)
    v.push_back(h.tables.end_year / 2);
    v.push_back(h.tables.end_year);
    const auto ch = h.timeline.holder_change_years();
    if (!ch.empty()) v.push_back(ch.front());
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

// --- The golden file --------------------------------------------------------------------------------

struct GoldenCase {
    std::string static_hash;
    std::vector<std::pair<std::int64_t, std::string>> years;      // item 2, file order
    std::string years_hash;                                        // item 3
    std::vector<std::pair<std::int64_t, std::string>> territory;  // item 4, file order
    std::map<std::string, std::string> tables;                    // item 5
};

inline const std::map<std::string, GoldenCase>& golden() {
    static const std::map<std::string, GoldenCase> g = [] {
        std::map<std::string, GoldenCase> out;
        std::istringstream in(detail::text_of(fs::path(UNMAPPED_P4_01_GOLDEN_PATH)));
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string name, what, a, b;
            ls >> name >> what >> a;
            GoldenCase& c = out[name];
            if (what == "static") {
                c.static_hash = a;
            } else if (what == "years") {
                c.years_hash = a;
            } else if (what == "year") {
                ls >> b;
                c.years.emplace_back(std::stoll(a), b);
            } else if (what == "territory") {
                ls >> b;
                c.territory.emplace_back(std::stoll(a), b);
            } else if (what == "table") {
                ls >> b;
                c.tables[a] = b;
            } else {
                FAIL("p4-01-timeline.txt: unknown line " << line);
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

// Checks items 1 to 3 of 13.4 for a loaded history against the golden case; on a difference the C++ dump
// is printed (the first 4000 bytes), to diff against `p4_timeline_reference.py dump`.
inline void check_dumps(const std::string& name, const wh::LoadedHistory& h) {
    CAPTURE(name);
    const GoldenCase& g = golden_case(name);
    const std::string s = static_dump(h);
    const std::vector<std::int64_t> races = race_ids(h);
    CHECK_MESSAGE(sha(s) == g.static_hash, "S of " << name << ":\n" << s.substr(0, 4000));
    std::vector<std::pair<std::int64_t, std::string>> got;
    for (const std::int64_t y : golden_years(h)) {
        const std::string d = year_dump(h.timeline.year_state(y), &h.timeline, &races);
        got.emplace_back(y, sha(d));
        const auto it = std::find_if(g.years.begin(), g.years.end(), [&](const auto& p) { return p.first == y; });
        if (it != g.years.end() && it->second != got.back().second) {
            FAIL_CHECK("D(" << y << ") of " << name << " differs:\n" << d.substr(0, 4000));
        }
    }
    CHECK(got == g.years);
    CHECK(years_hash(h.timeline, races) == g.years_hash);
}

}  // namespace tltest
