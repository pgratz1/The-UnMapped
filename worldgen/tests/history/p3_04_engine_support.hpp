#pragma once

// Test-owned helpers of the P3-04 engine tests: the golden file reader of
// worldgen/tests/golden/p3-04-engine.txt, edited content (history.engine
// and history.report rebuilt past the loader), hand-made HistoryInputs,
// a test awakening handler and test catalogue kind, engine configurations,
// an independent SC1-SC4 (own_base) for the property tests, the P0-04 8
// hashes and the [history] lines of 11.1 computed from rows, and worlds
// cached per process. Library API of P3-04 only through p3_04_engine_api.hpp.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_03_chronology_support.hpp"
#include "p3_04_engine_api.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/stages/testing.hpp"
#include "worldgen/version.hpp"

#ifndef UNMAPPED_P3_04_GOLDEN_PATH
#error "UNMAPPED_P3_04_GOLDEN_PATH must name worldgen/tests/golden/p3-04-engine.txt"
#endif

namespace engine_test {

namespace fs = std::filesystem;
namespace wd = worldgen::data;
namespace db = worldgen::db;
using chrono_test::open_world;
using chrono_test::rows_of;
using chrono_test::shipped;
using eng::Cell;
using eng::Row;
using eng::Tables;

// The empty world of 9.6 at schema 7. Spec 9.6 and 16.5: 34 pages (139264
// bytes) is the spec writer's measurement with Python's SQLite 3.46.1 and
// is UNCONFIRMED with the vendored SQLite; the engineer records the
// confirmed figure in 16.5, and this is the one constant to change.
inline constexpr std::int64_t EMPTY_WORLD_PAGES = 34;
inline constexpr std::int64_t EMPTY_WORLD_BYTES = EMPTY_WORLD_PAGES * 4096;

// The thirteen tables of 9.1 in DDL order, and in hash --detail order.
inline const std::vector<std::string>& ddl_tables() {
    static const std::vector<std::string> v = {
        "event_kind", "event_kind_role", "event_kind_value", "phase",      "event",
        "event_participant", "event_cause", "event_value", "civilization", "settlement",
        "settlement_census", "region_control", "relation"};
    return v;
}
inline const std::vector<std::string>& hash_tables() {
    static const std::vector<std::string> v = [] {
        std::vector<std::string> t = ddl_tables();
        std::sort(t.begin(), t.end());
        return t;
    }();
    return v;
}
inline std::size_t columns_of(std::string_view t) {
    static const std::map<std::string, std::size_t, std::less<>> m = {
        {"event_kind", 7},   {"event_kind_role", 6}, {"event_kind_value", 3}, {"phase", 6},
        {"event", 8},        {"event_participant", 5}, {"event_cause", 2},    {"event_value", 3},
        {"civilization", 10}, {"settlement", 14},    {"settlement_census", 6}, {"region_control", 5},
        {"relation", 9}};
    return m.find(t)->second;
}

// --- Text helpers -------------------------------------------------------------------------------

inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}
inline std::vector<std::int64_t> ints(const std::string& s, char sep = ',') {
    std::vector<std::int64_t> out;
    for (const std::string& t : split(s, sep)) {
        out.push_back(std::stoll(t));
    }
    return out;
}
inline std::string from_hex(const std::string& h) {
    if (h == "-") {
        return {};
    }
    std::string out;
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) {
        out += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}
inline std::string str(const Cell& c) {
    switch (c.type) {
        case db::ValueType::null: return "NULL";
        case db::ValueType::integer: return std::to_string(c.integer);
        default: return "'" + c.bytes + "'";
    }
}
inline std::string str(const Row& r) {
    std::string s = "(";
    for (std::size_t i = 0; i < r.size(); ++i) {
        s += (i ? ", " : "") + str(r[i]);
    }
    return s + ")";
}
inline std::string str(const std::vector<Row>& rows) {
    std::string s;
    for (const Row& r : rows) {
        s += str(r) + "\n";
    }
    return s;
}
// Rows from integers (NULL as std::nullopt), for expected tables written in tests.
using IRow = std::vector<std::optional<std::int64_t>>;
inline std::vector<Row> irows(const std::vector<IRow>& v) {
    std::vector<Row> out;
    for (const IRow& r : v) {
        Row row;
        for (const auto& x : r) {
            row.push_back(eng::O(x));
        }
        out.push_back(row);
    }
    return out;
}
inline constexpr std::nullopt_t NUL = std::nullopt;

// --- Golden file --------------------------------------------------------------------------------

struct GoldenSched {
    std::string name;
    std::string header;        // the line after "sched " or "edge "
    std::uint64_t seed = 0;    // sched cases
    std::int64_t span = 0;
    std::string detail;
    std::string races;         // sched cases
    std::string tension;       // "none" or ForcedTension(p,v,t)
    std::vector<std::int64_t> starts;
    std::vector<std::int64_t> lengths;  // A F L Q floor cap
    std::string phases;
    std::int64_t simulated = 0;
};
struct GoldenWorld {
    std::string name;
    std::uint64_t seed = 0;
    std::int64_t map_size = 0;
    std::int64_t span = 0;
    std::string races, detail, tension;
    std::map<std::string, std::string> tables;  // table -> hex
    std::vector<std::string> report;            // [history] lines after the header
    std::string phases;                         // with triggers
    std::vector<std::string> sentences;         // e1 ... en
    std::int64_t regions = 0;
};
struct GoldenTemplate {
    std::string kind;
    std::string text;
    std::string result;  // "ok" or the problem
};
struct Golden {
    std::vector<GoldenSched> scheds;  // sched and edge cases in file order
    std::vector<GoldenWorld> worlds;
    std::vector<GoldenTemplate> templates;
    std::vector<std::string> report_events;
    [[nodiscard]] const GoldenWorld& world(std::string_view name) const {
        for (const GoldenWorld& w : worlds) {
            if (w.name == name) {
                return w;
            }
        }
        FAIL("no golden world " << name);
        return worlds.front();
    }
    [[nodiscard]] const GoldenSched& sched(std::string_view name) const {
        for (const GoldenSched& s : scheds) {
            if (s.name == name) {
                return s;
            }
        }
        FAIL("no golden schedule " << name);
        return scheds.front();
    }
};

inline const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        const std::string text = chrono_test::read_text(fs::path(UNMAPPED_P3_04_GOLDEN_PATH));
        std::map<std::string, std::size_t> sched_at;
        std::map<std::string, std::size_t> world_at;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::size_t sp = line.find(' ');
            const std::string head = line.substr(0, sp);
            const std::string rest = line.substr(sp + 1);
            if (head == "sched" || head == "edge") {
                const std::vector<std::string> t = split(rest, ' ');
                GoldenSched s;
                s.name = t[0];
                s.header = rest;
                if (head == "sched") {
                    s.seed = std::stoull(t[1]);
                    s.span = std::stoll(t[2]);
                    s.detail = t[3];
                    s.races = t[4];
                    s.tension = t[5];
                } else {
                    s.span = std::stoll(t[1]);
                    s.detail = t[2];
                    s.tension = t[4];
                }
                sched_at[s.name] = out.scheds.size();
                out.scheds.push_back(s);
                continue;
            }
            if (head == "world") {
                const std::vector<std::string> t = split(rest, ' ');
                GoldenWorld w;
                w.name = t[0];
                w.seed = std::stoull(t[1]);
                w.map_size = std::stoll(t[2]);
                w.span = std::stoll(t[3]);
                w.races = t[4];
                w.detail = t[5];
                w.tension = t[6];
                world_at[w.name] = out.worlds.size();
                out.worlds.push_back(w);
                continue;
            }
            if (head == "template") {
                const std::vector<std::string> t = split(rest, ' ');
                GoldenTemplate tc{t[0], from_hex(t[1]), rest.substr(t[0].size() + t[1].size() + 2)};
                out.templates.push_back(tc);
                continue;
            }
            if (head == "report_events") {
                out.report_events.push_back(rest);
                continue;
            }
            const std::size_t sp2 = rest.find(' ');
            const std::string item = rest.substr(0, sp2);
            const std::string value = sp2 == std::string::npos ? std::string() : rest.substr(sp2 + 1);
            if (const auto it = sched_at.find(head); it != sched_at.end()) {
                GoldenSched& s = out.scheds[it->second];
                if (item == "starts") s.starts = ints(value);
                if (item == "lengths") s.lengths = ints(value, ' ');
                if (item == "phases") s.phases = value;
                if (item == "simulated") s.simulated = std::stoll(value);
                continue;
            }
            if (const auto it = world_at.find(head); it != world_at.end()) {
                GoldenWorld& w = out.worlds[it->second];
                if (item == "table") {
                    const std::size_t s2 = value.find(' ');
                    w.tables[value.substr(0, s2)] = value.substr(s2 + 1);
                } else if (item == "report") {
                    w.report.push_back(value);
                } else if (item == "phases") {
                    w.phases = value;
                } else if (item == "regions") {
                    w.regions = std::stoll(value);
                } else if (item.size() > 1 && item[0] == 'e' && item[1] >= '0' && item[1] <= '9') {
                    REQUIRE(std::stoll(item.substr(1)) == static_cast<std::int64_t>(w.sentences.size()) + 1);
                    w.sentences.push_back(value);
                }
                continue;
            }
            FAIL("unknown golden line: " << line);
        }
        REQUIRE(!out.scheds.empty());
        REQUIRE(!out.worlds.empty());
        return out;
    }();
    return g;
}

// --- P0-04 8 hashes and the [history] lines from rows ---------------------------------------------

inline std::string table_hash(std::size_t columns, const std::vector<Row>& rows) {
    chrono_test::TableHasher h(columns);
    for (const Row& r : rows) {
        REQUIRE(r.size() == columns);
        for (const Cell& c : r) {
            if (c.type == db::ValueType::null) {
                h.null();
            } else if (c.type == db::ValueType::integer) {
                h.integer(c.integer);
            } else {
                h.text(c.bytes);
            }
        }
    }
    return h.hex();
}
inline std::map<std::string, std::string> table_hashes(const Tables& t) {
    std::map<std::string, std::string> out;
    for (const std::string& n : ddl_tables()) {
        out[n] = table_hash(columns_of(n), t.of(n));
    }
    return out;
}

inline std::int64_t at(const Row& r, std::size_t i) { return r.at(i).integer; }

// 11.1 after the header line, from rows; [] when `phase` is empty.
inline std::vector<std::string> report_lines(const Tables& t, std::int64_t end_year) {
    std::vector<std::string> o;
    if (t.phase.empty()) {
        return o;
    }
    std::int64_t sim = 0;
    std::int64_t by_kind[5] = {0, 0, 0, 0, 0};
    for (const Row& r : t.phase) {
        by_kind[at(r, 2)] += 1;
        if (at(r, 2) != 2) {
            sim += at(r, 4) - at(r, 3) + 1;
        }
    }
    auto line = [&](const std::string& k, std::int64_t v) { o.push_back(k + " = " + std::to_string(v)); };
    line("end_year", end_year);
    line("phases_opening", by_kind[1]);
    line("phases_quiet", by_kind[2]);
    line("phases_flare", by_kind[3]);
    line("phases_final", by_kind[4]);
    line("simulated_years", sim);
    line("simulated_permille", sim * 1000 / end_year);
    line("events", static_cast<std::int64_t>(t.event.size()));
    for (const Row& k : t.event_kind) {
        std::int64_t n = 0;
        for (const Row& e : t.event) {
            n += at(e, 3) == at(k, 0) ? 1 : 0;
        }
        line("events_" + k[1].bytes, n);
    }
    std::int64_t alive = 0;
    for (const Row& c : t.civilization) {
        alive += c[8].type == db::ValueType::null ? 1 : 0;
    }
    std::int64_t standing = 0;
    for (const Row& s : t.settlement) {
        standing += (at(s, 10) == 1 || at(s, 10) == 2) ? 1 : 0;
    }
    std::int64_t held = 0;
    for (const Row& r : t.region_control) {
        held += r[2].type == db::ValueType::null ? 1 : 0;
    }
    line("civilizations", static_cast<std::int64_t>(t.civilization.size()));
    line("civilizations_alive", alive);
    line("settlements", static_cast<std::int64_t>(t.settlement.size()));
    line("settlements_standing", standing);
    line("regions_held", held);
    line("census_rows", static_cast<std::int64_t>(t.settlement_census.size()));
    line("relations", static_cast<std::int64_t>(t.relation.size()));
    return o;
}

// "id:age:kind:start:end|..." (golden format), with ":trigger" when asked.
inline std::string phases_text(const Tables& t, bool trigger = false) {
    std::string s;
    for (const Row& r : t.phase) {
        s += (s.empty() ? "" : "|") + std::to_string(at(r, 0)) + ":" + std::to_string(at(r, 1)) + ":" +
             std::to_string(at(r, 2)) + ":" + std::to_string(at(r, 3)) + ":" + std::to_string(at(r, 4));
        if (trigger) {
            s += ":" + std::to_string(at(r, 5));
        }
    }
    return s;
}
inline std::int64_t simulated(const Tables& t) {
    std::int64_t sim = 0;
    for (const Row& r : t.phase) {
        if (at(r, 2) != 2) {
            sim += at(r, 4) - at(r, 3) + 1;
        }
    }
    return sim;
}

// The thirteen tables of a stored world (WorldReader rows, primary-key order).
inline Tables tables_of_world(const db::WorldReader& r) {
    Tables t;
    for (const std::string& n : ddl_tables()) {
        auto rows = rows_of(r, n);
        t.of(n).assign(rows.begin(), rows.end());
    }
    return t;
}

// --- Content edited past the loader (3.4 item 2: seam guards) -------------------------------------

struct Detail {
    std::int64_t active_years, flare_years, final_years, flare_threshold;
};
struct EngineTuning {
    std::int64_t quiet_step_years = 25, quiet_events_max = 3, simulated_min_permille = 100,
                 simulated_max_permille = 450, islet_max_dry_cells = 1172;
    Detail sparse{75, 30, 250, 1500}, normal{150, 50, 300, 1000}, dense{250, 75, 400, 700};
};
struct ReportEntry {
    std::string kind;
    std::int64_t salience;
    std::string templ;
};
// The listing of 4.2.
inline std::vector<ReportEntry> shipped_report() {
    return {
        {"awakening", 100, "{creator} awakened the {race:plural} in {@region}."},
        {"civ_founded", 80, "The {race:plural} founded {civilization} in {@region}."},
        {"settlement_founded", 40, "{founder} founded {settlement} in {@region}."},
        {"quiet_era", 20,
         "{civilization} passed {years} quiet {years?year|years} ({generations} {generations?generation|generations}), "
         "from {@year} to {@end_year}."},
        {"flare_up", 50, "In {@year} tensions flared into {years} {years?year|years} of upheaval."},
    };
}

inline wd::Record detail_record(const Detail& d) {
    wd::Record r;
    r.add("active_years", wd::Value::integer(d.active_years));
    r.add("flare_years", wd::Value::integer(d.flare_years));
    r.add("final_years", wd::Value::integer(d.final_years));
    r.add("flare_threshold", wd::Value::integer(d.flare_threshold));
    return r;
}
inline wd::Record engine_record(const EngineTuning& t) {
    wd::Record r;
    r.add("quiet_step_years", wd::Value::integer(t.quiet_step_years));
    r.add("quiet_events_max", wd::Value::integer(t.quiet_events_max));
    r.add("simulated_min_permille", wd::Value::integer(t.simulated_min_permille));
    r.add("simulated_max_permille", wd::Value::integer(t.simulated_max_permille));
    r.add("islet_max_dry_cells", wd::Value::integer(t.islet_max_dry_cells));
    r.add("sparse", wd::Value::record(detail_record(t.sparse)));
    r.add("normal", wd::Value::record(detail_record(t.normal)));
    r.add("dense", wd::Value::record(detail_record(t.dense)));
    return r;
}
inline wd::Record report_record(const std::vector<ReportEntry>& es) {
    std::vector<wd::Value> v;
    for (const ReportEntry& e : es) {
        wd::Record r;
        r.add("kind", wd::Value::string(e.kind));
        r.add("salience", wd::Value::integer(e.salience));
        r.add("template", wd::Value::string(e.templ));
        v.push_back(wd::Value::record(std::move(r)));
    }
    wd::Record r;
    r.add("events", wd::Value::array(std::move(v)));
    return r;
}
inline wd::ContentSet with_history_record(const wd::ContentSet& base, const std::string& id, wd::Record rec) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "history") {
            continue;
        }
        for (wd::Definition& d : k.definitions) {
            if (d.id == id) {
                d.record = std::move(rec);
                return c;
            }
        }
    }
    FAIL("no history." << id << " in the content");
    return c;
}
inline wd::ContentSet with_engine(const EngineTuning& t, const wd::ContentSet& base = shipped()) {
    return with_history_record(base, "engine", engine_record(t));
}
inline wd::ContentSet with_report(const std::vector<ReportEntry>& es, const wd::ContentSet& base = shipped()) {
    return with_history_record(base, "report", report_record(es));
}

// --- Hand-made inputs -----------------------------------------------------------------------------

inline db::RegionRow region_row(std::int64_t id, const std::string& name, std::int64_t x0, std::int64_t y0,
                                std::int64_t x1, std::int64_t y1, std::int64_t cells, std::int64_t walk,
                                std::int64_t hab, std::int64_t lake = 0) {
    db::RegionRow r{};
    r.id = id;
    r.name = name;
    r.biome = 3;
    r.x_min = x0;
    r.y_min = y0;
    r.x_max = x1;
    r.y_max = y1;
    r.seed_x = x0;
    r.seed_y = y0;
    r.landmass = 1;
    r.cells = cells;
    r.lake_cells = lake;
    r.walk_cells = walk;
    r.habitability_mean = hab;
    return r;
}

// An N x N `region` (u16) and `water` (u8) layer: every cell of region 1
// unless a region row's box says otherwise (later rows win), water 0.
inline std::pair<db::LayerGrid, db::LayerGrid> layers_for(std::int64_t n, const std::vector<db::RegionRow>& regions) {
    db::LayerGrid reg = db::make_layer_grid(db::ElementType::u16, static_cast<std::uint32_t>(n),
                                            static_cast<std::uint32_t>(n));
    db::LayerGrid wat = db::make_layer_grid(db::ElementType::u8, static_cast<std::uint32_t>(n),
                                            static_cast<std::uint32_t>(n));
    auto& rv = std::get<std::vector<std::uint16_t>>(reg.values);
    for (const db::RegionRow& r : regions) {
        for (std::int64_t y = r.y_min; y <= r.y_max; ++y) {
            for (std::int64_t x = r.x_min; x <= r.x_max; ++x) {
                rv[static_cast<std::size_t>(y * n + x)] = static_cast<std::uint16_t>(r.id);
            }
        }
    }
    return {reg, wat};
}

// HistoryInputs with chronology rows of the shipped data for the present
// races (P3-03 rows at seed 0), the age start years replaced by `starts`
// (y_1 = 0, strictly increasing), detail level, span E, and the given
// regions (default: two plain non-islet regions in a 16 x 16 map).
inline eng::HistoryInputs inputs_for(const std::vector<std::int64_t>& starts, std::int64_t span,
                                     const std::string& detail = "normal",
                                     std::vector<std::string> races = {},
                                     std::vector<db::RegionRow> regions = {}, std::uint64_t seed = 0) {
    if (races.empty()) {
        const auto& six = chrono_test::six();
        races.assign(six.begin(), six.begin() + static_cast<std::ptrdiff_t>(starts.size()));
    }
    REQUIRE(races.size() == starts.size());
    if (regions.empty()) {
        regions = {region_row(1, "Alpha", 0, 0, 7, 15, 128, 100, 60), region_row(2, "Beta", 8, 0, 15, 15, 128, 90, 50)};
        regions[0].cells = 5000;
        regions[1].cells = 5000;
    }
    chrono_api::Params p = chrono_test::params_with(span, races);
    p.detail = detail;
    std::vector<std::string> names;
    for (const auto& r : regions) {
        names.push_back(r.name);
    }
    const chrono_api::Rows c = chrono_api::stage(shipped(), p, seed, names);
    eng::HistoryInputs in;
    in.seed = seed;
    in.map_size = 16;
    in.params = chrono_api::to_lib(p);
    in.regions = regions;
    std::size_t k = 0;
    for (const auto& r : c.races) {
        db::RaceRow row{};
        row.id = r.id;
        row.key = r.key;
        row.name = r.name;
        row.plural = r.plural;
        row.adjective = r.adjective;
        row.camp = r.camp;
        row.lifespan_years = r.lifespan_years;
        row.generation_years = r.generation_years;
        row.culture = r.culture;
        row.present = r.present;
        row.awakening_year = r.present ? std::optional<std::int64_t>(starts.at(k++)) : std::nullopt;
        row.creator_god = r.creator_god;
        in.races.push_back(row);
    }
    for (const auto& r : c.race_biomes) {
        in.race_biomes.push_back(db::RaceBiomeRow{r.race, r.biome, r.weight});
    }
    for (const auto& r : c.cultures) {
        in.cultures.push_back(db::CultureRow{r.id, r.key, r.name, r.race});
    }
    for (const auto& r : c.gods) {
        in.gods.push_back(db::GodRow{r.id, r.name, r.camp});
    }
    for (std::size_t i = 0; i < c.ages.size(); ++i) {
        const std::int64_t end = i + 1 < starts.size() ? starts[i + 1] - 1 : span - 1;
        in.ages.push_back(db::AgeRow{c.ages[i].id, c.ages[i].race, starts[i], end, c.ages[i].name});
    }
    auto [reg, wat] = layers_for(16, regions);
    in.region_layer = reg;
    in.water_layer = wat;
    return in;
}

// --- Test awakening handler, kinds, configurations ------------------------------------------------

// Places every race in region `home`; per awakening appends civ_founded
// (cause the awakening), founds the civilization, appends
// settlement_founded (population 200, cause civ_founded), founds a hamlet
// at cell (settlement id - 1, 0) and makes it the capital: the stub's
// event shape (7.2) without its home and naming rules. With `found` false
// it does nothing (only the engine's awakening event is written).
class TestHandler : public eng::AwakeningHandler {
public:
    explicit TestHandler(bool found = true, std::int64_t home = 1) : found_(found), home_(home) {}
    std::int64_t awakening_place(const eng::StepContext&, std::int64_t) override { return home_; }
    void awaken(eng::StepContext& ctx, std::int64_t race, std::int64_t awakening_event) override {
        if (!found_) {
            return;
        }
        const std::int64_t c = static_cast<std::int64_t>(ctx.state().civs().size()) + 1;
        std::int64_t culture = 0;
        for (const auto& r : ctx.inputs().races) {
            if (r.id == race) {
                culture = r.culture;
            }
        }
        eng::EventSpec civ_ev;
        civ_ev.kind = "civ_founded";
        civ_ev.year = ctx.year();
        civ_ev.end_year = ctx.year();
        civ_ev.region = home_;
        civ_ev.participants = {{"civilization", eng::EntityType::civilization, c}, {"race", eng::EntityType::race, race}};
        civ_ev.causes = {awakening_event};
        const std::int64_t e_civ = ctx.append(civ_ev);
        const std::int64_t got = ctx.state().found_civilization(race, culture, home_, "Civ " + std::to_string(c), e_civ);
        REQUIRE(got == c);
        const std::int64_t s = static_cast<std::int64_t>(ctx.state().settlements().size()) + 1;
        eng::EventSpec set_ev;
        set_ev.kind = "settlement_founded";
        set_ev.year = ctx.year();
        set_ev.end_year = ctx.year();
        set_ev.region = home_;
        set_ev.settlement = s;
        set_ev.participants = {{"settlement", eng::EntityType::settlement, s}, {"founder", eng::EntityType::civilization, c}};
        set_ev.values = {200};
        set_ev.causes = {e_civ};
        const std::int64_t e_set = ctx.append(set_ev);
        const std::int64_t sid = ctx.state().found_settlement(s - 1, 0, home_, c, eng::SettlementKind::hamlet, 200,
                                                              "Town " + std::to_string(s), e_set);
        REQUIRE(sid == s);
        ctx.state().set_capital(c, s);
    }

private:
    bool found_;
    std::int64_t home_;
};

// A test kind appended to the stage's catalogue (6.4: "tests may extend
// it"): id 6, `test_change`, uncaused, region and settlement optional,
// role 1 `actor` civilization 0-unbounded, no values.
inline eng::EventCatalogue test_catalogue() {
    eng::EventCatalogue cat = eng::catalogue_of(shipped());
    eng::KindDef k;
    k.id = static_cast<std::int64_t>(cat.kinds.size()) + 1;
    k.key = "test_change";
    k.caused = false;
    k.region_use = 2;
    k.settlement_use = 2;
    k.roles = {eng::RoleDef{1, "actor", eng::EntityType::civilization, 0, 0}};
    k.values = {};
    k.salience = 10;
    k.templ = "{actor:all} changed.";
    cat.kinds.push_back(k);
    return cat;
}

// An event of the test kind in the current step, charged to `civ` (0: none).
inline std::int64_t change(eng::StepContext& ctx, std::int64_t civ = 0, std::int64_t year = -1) {
    eng::EventSpec e;
    e.kind = "test_change";
    e.year = year < 0 ? ctx.year() : year;
    e.end_year = e.year;
    if (civ != 0) {
        e.participants = {{"actor", eng::EntityType::civilization, civ}};
    }
    return ctx.append(e);
}

// An engine configuration with owned parts.
struct Config {
    std::unique_ptr<eng::AwakeningHandler> handler = std::make_unique<TestHandler>();
    std::vector<eng::Subsystem*> subsystems;
    std::vector<std::unique_ptr<eng::TensionSource>> owned_tension;
    std::vector<eng::TensionSource*> tension;
    std::unique_ptr<eng::InterventionSource> interventions = std::make_unique<eng::EmptyInterventionSource>();
    eng::EventCatalogue catalogue = test_catalogue();

    void add_tension(std::unique_ptr<eng::TensionSource> t) {
        tension.push_back(t.get());
        owned_tension.push_back(std::move(t));
    }
    [[nodiscard]] eng::EngineConfig get() const {
        eng::EngineConfig c{};
        c.awakening = handler.get();
        c.subsystems = subsystems;
        c.tension = tension;
        c.interventions = interventions.get();
        c.catalogue = &catalogue;
        return c;
    }
};

// A subsystem running a function in the given steps.
class FnSubsystem : public eng::Subsystem {
public:
    using Fn = std::function<void(eng::StepContext&)>;
    FnSubsystem(std::vector<eng::Step> steps, Fn fn) : steps_(std::move(steps)), fn_(std::move(fn)) {}
    std::span<const eng::Step> steps() const override { return steps_; }
    void run(eng::StepContext& ctx) override { fn_(ctx); }

private:
    std::vector<eng::Step> steps_;
    Fn fn_;
};

// A tension source from a function (step start, length) -> report.
class FnTension : public eng::TensionSource {
public:
    using Fn = std::function<eng::TensionReport(const eng::StepContext&)>;
    explicit FnTension(Fn fn) : fn_(std::move(fn)) {}
    eng::TensionReport tension(const eng::StepContext& ctx) override { return fn_(ctx); }

private:
    Fn fn_;
};

// The FatalError text of a call, or "<no error>".
template <class F>
std::string fatal_of(F&& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return "<no error>";
}

// --- Independent SC1-SC4 (property tests) ---------------------------------------------------------

struct OwnBase {
    std::int64_t a, f, l, q, floor, cap, s_f, sched;
    std::vector<std::int64_t> ends;
};
inline OwnBase own_base(const std::vector<std::int64_t>& y, std::int64_t e, const Detail& d,
                        const EngineTuning& t = {}) {
    auto sc = [&](std::int64_t v) { return std::max<std::int64_t>(1, (v * e + 3000) / 6000); };
    OwnBase b{sc(d.active_years), sc(d.flare_years), sc(d.final_years), t.quiet_step_years,
              (t.simulated_min_permille * e + 999) / 1000, (t.simulated_max_permille * e) / 1000, 0, 0, {}};
    const std::size_t p = y.size();
    for (std::size_t k = 0; k + 1 < p; ++k) {
        b.ends.push_back(std::min(y[k] + b.a, y[k + 1]));
    }
    const bool last_ok = e - y[p - 1] >= 2;
    b.ends.push_back(last_ok ? std::min(y[p - 1] + b.a, e - 1) : e);
    b.s_f = last_ok ? std::max(e - b.l, b.ends[p - 1]) : e;
    auto total = [&] {
        std::int64_t s = e - b.s_f;
        for (std::size_t k = 0; k < p; ++k) {
            s += b.ends[k] - y[k];
        }
        return s;
    };
    std::int64_t dd = b.floor - total();
    if (dd > 0 && b.s_f < e) {
        const std::int64_t s2 = std::max(b.ends[p - 1], b.s_f - dd);
        dd -= b.s_f - s2;
        b.s_f = s2;
    }
    for (std::size_t k = 0; k + 1 < p && dd > 0; ++k) {
        const std::int64_t e2 = std::min(y[k + 1], b.ends[k] + dd);
        dd -= e2 - b.ends[k];
        b.ends[k] = e2;
    }
    b.sched = total();
    return b;
}
inline Detail detail_of(const EngineTuning& t, std::string_view lv) {
    return lv == "sparse" ? t.sparse : (lv == "dense" ? t.dense : t.normal);
}

// --- Worlds -----------------------------------------------------------------------------------------

// A golden world case generated with the shipped data (every stage),
// cached per process.
inline std::string world_of(const GoldenWorld& w, std::uint32_t threads = 1, const std::string& until = "") {
    chrono_test::WorldKey k{w.seed, w.name == "geo-phrase-512" ? "the unmapped" : "", w.map_size, w.span, w.races,
                            until, threads};
    if (w.detail == "normal") {
        return chrono_test::world(k);
    }
    // detail other than normal: generate directly (the chronology cache has no detail key).
    static std::map<std::pair<std::string, std::string>, std::string> cache;
    const auto key = std::make_pair(w.name + "/" + std::to_string(threads), until);
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    chrono_api::Params p = chrono_test::params_with(w.span, chrono_test::races_of_text(w.races));
    p.detail = w.detail;
    const std::string path = chrono_test::world_dir().file("d" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams wp = chrono_test::world_params(
        w.seed, w.map_size, p, shipped(), until.empty() ? std::nullopt : std::optional<std::string>(until), threads);
    const worldgen::db::Status s = worldgen::generate_world(wp, shipped(), path, worldgen::db::Overwrite::refuse);
    REQUIRE_MESSAGE(s.ok(), "generate_world failed: " << s.error().message);
    cache[key] = path;
    return path;
}

}  // namespace engine_test
