// Spec P5-02 (docs/specs/p5-02-beasts-monsters.md r0) 14, 18, 19.2 and 19.3: the shipped history of the world
// cases of worldgen/tests/golden/p5-02-monsters.txt (the conflict reference's replay with the monsters subsystem,
// reference/p5_02_monsters_reference.py) against the file: the tables beast_population, monster and site (rows
// and P0-04 8's hash), settlement.ending, the M, G and S counts and each monster's and site's line; invariants 1,
// 3, 4, 6, 7, 9 and 10 of section 14 from the rows; the fixtures monsters-small (the conflict stress content with
// every wake_rate x 2000, living_permille 1000, overrun_permille 1000) and zero (7.5); I0 on the stress
// geographies (the zero tuning against the configuration without the subsystem: the history tables equal) and
// on seeds 0 to 4 at 512 in the suite "monsters-slow"; determinism across thread counts.
//
// geo-seed-42-512, geo-seed-0-512 and the fixtures run by default; every other world case (the 1024 ones, and
// p5-monsters-high-1024 with --monster-pressure high) is in the suite "monsters-slow". Written from the specs
// before the implementation (test-writer rule); library names beyond P5-12's: the five tables of
// p3_04_engine_api.hpp's Tables (GUESS members of HistoryResult, p5_02_monsters_api.hpp).

#include "p3_06_conflict_support.hpp"
#include "p5_02_monsters_api.hpp"
#include "p5_02_monsters_content.hpp"
#include "p5_03_relations_api.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace conflict_test;

// --- The golden file -----------------------------------------------------------------------------------------

struct MCase {
    std::string kind, name, data;   // kind world or fixture; data monsters-small or zero
    std::uint64_t seed = 0;
    std::int64_t size = 192, span = 0;
    std::string races, detail = "normal", civs = "auto", intensity = "normal", pressure = "normal";
    std::map<std::string, std::pair<std::int64_t, std::string>> tables;   // name -> (rows, hash)
    std::string ending, M, G, S;
    std::vector<std::string> monsters, sites;   // the m<id> and si<id> lines without the case name
};

struct MGolden {
    std::vector<MCase> cases;
    std::vector<std::string> kinds_seen, events_seen;
};

const MGolden& monsters_golden() {
    static const MGolden g = [] {
        MGolden out;
        std::ifstream f(UNMAPPED_P5_02_GOLDEN_PATH, std::ios::binary);
        REQUIRE_MESSAGE(f.good(), "cannot read " << UNMAPPED_P5_02_GOLDEN_PATH);
        std::map<std::string, std::size_t> idx;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            const std::vector<std::string> w = split(line, ' ');
            if (w[0] == "world" || w[0] == "fixture") {
                MCase c;
                c.kind = w[0];
                c.name = w[1];
                c.seed = std::stoull(w[2]);
                if (w[0] == "world") {
                    c.size = std::stoll(w[3]);
                    c.span = std::stoll(w[4]);
                    c.races = w[5];
                    c.detail = w[6];
                    c.civs = w[7];
                    c.intensity = w[8];
                    c.pressure = w[9];
                } else {
                    c.span = std::stoll(w[3]);
                    c.races = w[4];
                    c.civs = w[5];
                    c.intensity = w[6];
                    c.data = w[7];
                }
                idx[c.name] = out.cases.size();
                out.cases.push_back(c);
                continue;
            }
            if (w[0] == "kinds_seen") {
                out.kinds_seen = split(w[1], ',');
                continue;
            }
            if (w[0] == "events_seen") {
                out.events_seen = split(w[1], ',');
                continue;
            }
            const auto it = idx.find(w[0]);
            REQUIRE_MESSAGE(it != idx.end(), "golden line of no case: " << line);
            MCase& c = out.cases[it->second];
            const std::string rest = line.substr(w[0].size() + 1);
            if (w[1] == "table") {
                c.tables[w[2]] = {std::stoll(w[3]), w[4]};
            } else if (w[1] == "ending") {
                c.ending = w[2];
            } else if (w[1] == "M") {
                c.M = rest;
            } else if (w[1] == "G") {
                c.G = rest;
            } else if (w[1] == "S") {
                c.S = rest;
            } else if (w[1].rfind("si", 0) == 0) {
                c.sites.push_back(rest);
            } else if (w[1].rfind("m", 0) == 0) {
                c.monsters.push_back(rest);
            } else {
                FAIL("unknown golden item: " << line);
            }
        }
        return out;
    }();
    return g;
}

const MCase& mcase(const char* name) {
    for (const MCase& c : monsters_golden().cases) {
        if (c.name == name) return c;
    }
    FAIL("no monsters golden case " << name);
    return monsters_golden().cases.front();
}

// --- Content edits (the reference's monsters_small_data and zero_data) -----------------------------------------

using monsters_content::monsters_small_content;
using monsters_content::pressure_of;
using monsters_content::with_monsters;

// 7.5 zero: every pressure factor 0, drag and feed caps 0, bold_permille 0, outpost_permille 0, ghost_permille 0.
const wd::ContentSet& zero_content() {
    static const wd::ContentSet c = with_monsters(
        shipped(),
        {{"pressure", pressure_of(0, 0, 0)}, {"drag_max_permille", wd::Value::integer(0)},
         {"feed_max_permille", wd::Value::integer(0)}, {"bold_permille", wd::Value::integer(0)},
         {"outpost_permille", wd::Value::integer(0)}},
        {}, {}, {{"ghost_permille", wd::Value::integer(0)}});
    return c;
}
// From 0.24.0 the history stage also runs P5-03's relations subsystem: I0 of P5-02 compares the stage with the
// zero monsters tuning and P5-03's zero relations tuning (P5-03 7.4: equal to the subsystem not installed) with
// the configuration that installs neither.
const wd::ContentSet& zero_both_content() {
    // P5-04 (0.26.0): and P5-04's zero, since the configuration without monsters builds no alliances (P5-04 7.14)
    static const wd::ContentSet c = p503api::alliances_zero_of(p503api::zero_of(zero_content()));
    return c;
}
// The shipped content with ghost_permille 0 (the configuration without the subsystem of I0).
const wd::ContentSet& no_ghost_content() {
    static const wd::ContentSet c = with_monsters(shipped(), {}, {}, {}, {{"ghost_permille", wd::Value::integer(0)}});
    return c;
}

// --- Rows -------------------------------------------------------------------------------------------------------

std::string ending_hash(const Tables& t) {
    std::vector<Row> rows;
    for (const Row& s : t.settlement) rows.push_back({s[0], s.at(14)});
    return table_hash(2, rows);
}

std::map<std::string, std::int64_t> kind_ids(const Tables& t) {
    std::map<std::string, std::int64_t> m;
    for (const Row& k : t.event_kind) m[k[1].bytes] = at(k, 0);
    return m;
}

std::int64_t count_kind(const Tables& t, std::int64_t kid) {
    std::int64_t n = 0;
    for (const Row& e : t.event) n += at(e, 3) == kid ? 1 : 0;
    return n;
}

// The M, G and S lines of the golden file from the C++ rows (the reference's case_lines).
std::string m_line(const Tables& t) {
    const auto k = kind_ids(t);
    std::int64_t alive = 0, slain = 0, died = 0, raids = 0, killed = 0, open_held = 0;
    for (const Row& m : t.monster) {
        alive += (at(m, 9) == 1 || at(m, 9) == 2) ? 1 : 0;
        slain += at(m, 9) == 3 ? 1 : 0;
        died += at(m, 9) == 4 ? 1 : 0;
    }
    std::map<std::int64_t, std::int64_t> dead_of;   // raid event -> dead (value seq 1)
    for (const Row& v : t.event_value) {
        if (at(v, 1) == 1) dead_of[at(v, 0)] = at(v, 2);
    }
    for (const Row& e : t.event) {
        if (at(e, 3) == k.at("monster_raid")) {
            ++raids;
            killed += dead_of[at(e, 0)];
        }
    }
    for (const Row& s : t.site) open_held += (at(s, 2) == 1 && at(s, 13) == 0 && at(s, 6) != 0) ? 1 : 0;
    return "M monsters " + std::to_string(t.monster.size()) + " alive " + std::to_string(alive) + " slain " +
           std::to_string(slain) + " died " + std::to_string(died) + " raids " + std::to_string(raids) + " killed " +
           std::to_string(killed) + " overruns " + std::to_string(count_kind(t, k.at("settlement_overrun"))) +
           " risen " + std::to_string(count_kind(t, k.at("undead_rose"))) + " lair_settlement_open " +
           std::to_string(open_held);
}

std::string s_line(const Tables& t) {
    const auto k = kind_ids(t);
    std::int64_t outposts = 0, ended = 0, flares = 0;
    for (const Row& s : t.site) {
        outposts += at(s, 2) == 2 ? 1 : 0;
        ended += (at(s, 2) == 2 && at(s, 13) != 0) ? 1 : 0;
    }
    std::map<std::int64_t, std::int64_t> kind_of;
    for (const Row& e : t.event) kind_of[at(e, 0)] = at(e, 3);
    for (const Row& p : t.phase) {
        if (at(p, 2) == 3 && at(p, 5) != 0 && kind_of[at(p, 5)] == k.at("monster_appeared")) ++flares;
    }
    return "S outposts " + std::to_string(outposts) + " ended " + std::to_string(ended) + " bold " +
           std::to_string(count_kind(t, k.at("beasts_bold"))) + " monster_flares " + std::to_string(flares);
}

// 8.3: the ending of every settlement derived from the log (a faded row through its ruin_faded's cause, E1).
std::map<std::int64_t, std::int64_t> derived_endings(const Tables& t) {
    std::map<std::int64_t, std::string> key;
    for (const Row& k : t.event_kind) key[at(k, 0)] = k[1].bytes;
    std::map<std::int64_t, std::int64_t> kind_of;
    std::map<std::int64_t, std::int64_t> settlement_of;
    for (const Row& e : t.event) {
        kind_of[at(e, 0)] = at(e, 3);
        settlement_of[at(e, 0)] = at(e, 6);
    }
    std::map<std::int64_t, std::vector<std::int64_t>> causes;
    for (const Row& c : t.event_cause) causes[at(c, 0)].push_back(at(c, 1));
    std::map<std::int64_t, std::int64_t> faded_ruin;
    for (const auto& [e, k] : kind_of) {
        if (key[k] == "ruin_faded") faded_ruin[settlement_of[e]] = *std::min_element(causes[e].begin(), causes[e].end());
    }
    std::map<std::int64_t, std::int64_t> out;
    for (const Row& s : t.settlement) {
        const std::int64_t status = at(s, 10);
        if (status == 1 || status == 2) {
            out[at(s, 0)] = 0;
            continue;
        }
        const std::int64_t ruin = status == 3 ? at(s, 13) : faded_ruin[at(s, 0)];
        const std::string& k = key[kind_of[ruin]];
        std::int64_t e = 5;
        if (k == "sacked") {
            e = 1;
        } else if (k == "settlement_overrun") {
            e = 2;
        } else if (k == "settlement_abandoned" || k == "settlement_abandoned_to") {
            bool plague = false, famine = false, fled = false;
            for (const std::int64_t c : causes[ruin]) {
                const std::string& ck = key[kind_of[c]];
                plague = plague || ck == "plague" || ck == "plague_spread" || ck == "plague_outbreak";
                famine = famine || ck == "famine";
                fled = fled || ck == "settlement_fled";   // P5-03 11.3: code 6, tested before 3 and 4
            }
            e = fled ? 6 : plague ? 3 : famine ? 4 : 5;
        }
        out[at(s, 0)] = e;
    }
    return out;
}

// Invariants 1, 3 (one open lair per region), 4 (one appearance, the monster's events inside its life, one
// overrun at most, one slaying or death), 6 (the lairs of living monsters open, of ended monsters ended in the
// death year; a site's settlement ruined or faded), 7 (an overrun settlement's ruin event is the overrun; overruns
// at most a quarter of the settlements), 9 (an open outpost's founder alive) and 10 (ending) on rows.
// quarter = false leaves out invariant 7's quarter bound (P5-00 8.3), a calibration target that no rule enforces:
// the stress fixture monsters-small (overrun_permille 1000, r1's prey on small places, 26.2) exceeds it by design.
std::vector<std::string> violations(const Tables& t, bool quarter = true) {
    std::vector<std::string> out;
    const auto k = kind_ids(t);
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> last;
    for (const Row& r : t.beast_population) {
        const auto key = std::make_pair(at(r, 0), at(r, 1));
        if (at(r, 3) < 0 || at(r, 3) > 1000000) out.push_back("1: count out of bounds");
        const auto it = last.find(key);
        if (it != last.end() && it->second == at(r, 3)) out.push_back("1: a row repeats the count");
        if (it == last.end() && at(r, 3) == 0) out.push_back("1: a first row of 0");
        last[key] = at(r, 3);
    }
    std::set<std::int64_t> marks;
    for (const Row& s : t.site) {
        marks.insert(at(s, 10));
        if (at(s, 12) != 0) marks.insert(at(s, 12));
    }
    for (const std::int64_t e : marks) {
        std::set<std::int64_t> regions;
        for (const Row& s : t.site) {
            if (at(s, 2) == 1 && at(s, 10) <= e && (at(s, 12) == 0 || e < at(s, 12)) &&
                !regions.insert(at(s, 3)).second) {
                out.push_back("3: two open lairs in a region at e" + std::to_string(e));
            }
        }
    }
    std::map<std::int64_t, std::vector<const Row*>> events_of_monster;
    std::map<std::int64_t, std::vector<std::int64_t>> monster_parts;   // event -> monster ids in role 1
    for (const Row& p : t.event_participant) {
        if (at(p, 3) == 8 && at(p, 2) == 1) monster_parts[at(p, 0)].push_back(at(p, 4));
    }
    for (const Row& e : t.event) {
        for (const std::int64_t m : monster_parts[at(e, 0)]) events_of_monster[m].push_back(&e);
    }
    for (const Row& m : t.monster) {
        const std::int64_t id = at(m, 0);
        std::int64_t appearances = 0, overruns = 0, slain = 0, died = 0;
        for (const Row* e : events_of_monster[id]) {
            const std::int64_t kd = at(*e, 3);
            if (kd == k.at("monster_appeared") || kd == k.at("undead_rose")) {
                ++appearances;
                if (at(*e, 0) != at(m, 5)) out.push_back("4: monster " + std::to_string(id) + " appears twice");
            }
            overruns += kd == k.at("settlement_overrun") ? 1 : 0;
            slain += kd == k.at("monster_slain") ? 1 : 0;
            // P5-06 11.2: a slaying by a hero is a slaying
            slain += k.count("monster_slain_by_hero") != 0U && kd == k.at("monster_slain_by_hero") ? 1 : 0;
            died += kd == k.at("monster_died") ? 1 : 0;
            if (at(*e, 0) < at(m, 5) || (m[6].type != db::ValueType::null && at(*e, 0) > at(m, 7))) {
                out.push_back("4: monster " + std::to_string(id) + " has an event outside its life");
            }
        }
        if (appearances != 1) out.push_back("4: monster " + std::to_string(id) + " appearances");
        if (overruns > 1) out.push_back("4: monster " + std::to_string(id) + " overran twice");
        if ((at(m, 9) == 3) != (slain == 1) || (at(m, 9) == 4) != (died == 1) || slain > 1 || died > 1) {
            out.push_back("4: monster " + std::to_string(id) + " status and its end events");
        }
        const Row& lair = t.site.at(static_cast<std::size_t>(at(m, 3) - 1));
        if (at(lair, 2) != 1 || at(lair, 8) != id) out.push_back("6: lair_site of monster " + std::to_string(id));
        const bool living = at(m, 9) == 1 || at(m, 9) == 2;
        if (living && at(lair, 13) != 0) out.push_back("6: a living monster's lair ended");
        if (!living && (at(lair, 13) == 0 || at(lair, 11) != at(m, 6))) {
            out.push_back("6: an ended monster's lair not ended in its death year");
        }
    }
    std::map<std::int64_t, const Row*> stl;
    for (const Row& s : t.settlement) stl[at(s, 0)] = &s;
    for (const Row& s : t.site) {
        // P5-04 7.16, P5-06 7.8: P5-02's site rules read kinds 1 and 2 (a mine's town and a library's city stand)
        if (at(s, 6) == 0 || (at(s, 2) != 1 && at(s, 2) != 2)) continue;
        const Row* x = stl[at(s, 6)];
        if (x == nullptr || (at(*x, 10) != 3 && at(*x, 10) != 4)) out.push_back("6: a site's settlement stands");
    }
    std::int64_t overruns = 0;
    for (const Row& e : t.event) {
        if (at(e, 3) != k.at("settlement_overrun")) continue;
        ++overruns;
        const Row* x = stl[at(e, 6)];
        if (x != nullptr && at(*x, 10) == 3 && at(*x, 13) != at(e, 0)) out.push_back("7: overrun is not the ruin event");
    }
    if (quarter && 4 * overruns > static_cast<std::int64_t>(t.settlement.size())) {
        out.push_back("7: overruns above a quarter");
    }
    std::map<std::int64_t, bool> alive;
    for (const Row& c : t.civilization) alive[at(c, 0)] = c[8].type == db::ValueType::null;
    for (const Row& s : t.site) {
        if (at(s, 2) == 2 && at(s, 13) == 0 && !alive[at(s, 7)]) out.push_back("9: open outpost of an ended realm");
    }
    const auto want = derived_endings(t);
    for (const Row& s : t.settlement) {
        if (s.size() < 15U || at(s, 14) != want.at(at(s, 0))) {
            out.push_back("10: settlement " + std::to_string(at(s, 0)) + " ending");
        }
    }
    return out;
}

void check_against(const MCase& g, const Tables& t) {
    CAPTURE(g.name);
    for (const auto& [name, rows] : std::vector<std::pair<std::string, const std::vector<Row>*>>{
             {"beast_population", &t.beast_population}, {"monster", &t.monster}, {"site", &t.site}}) {
        CAPTURE(name);
        REQUIRE(g.tables.count(name) == 1U);
        CHECK(static_cast<std::int64_t>(rows->size()) == g.tables.at(name).first);
        const std::size_t cols = name == "beast_population" ? 4U : name == "monster" ? 11U : 14U;
        CHECK(table_hash(cols, *rows) == g.tables.at(name).second);
    }
    CHECK(ending_hash(t) == g.ending);
    CHECK(m_line(t) == g.M);
    CHECK(s_line(t) == g.S);
    CHECK(violations(t, g.data != "monsters-small") == std::vector<std::string>{});
}

std::string world_of(const MCase& g) {
    if (g.pressure == "normal") {
        const CCase& c = conflict_case(g.name.c_str());
        const bool phrase = c.name == "geo-phrase-512";
        if (c.detail == "normal" && c.civs == "auto" && c.intensity == "normal") {
            return chrono_test::world(
                chrono_test::WorldKey{c.seed, phrase ? "the unmapped" : "", c.size, c.span, c.races, "", 1});
        }
    }
    static std::map<std::string, std::string> cache;
    if (const auto it = cache.find(g.name); it != cache.end()) return it->second;
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    p.civs = g.civs == "auto" ? std::nullopt : std::optional<std::int64_t>(std::stoll(g.civs));
    p.intensity = g.intensity;
    p.pressure = g.pressure;
    const std::string path = chrono_test::world_dir().file("mon" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams wp = chrono_test::world_params(g.seed, g.size, p, shipped(), std::nullopt, 1);
    REQUIRE(worldgen::generate_world(wp, shipped(), path, db::Overwrite::refuse).ok());
    cache[g.name] = path;
    return path;
}

void check_world_case(const MCase& g) {
    const db::WorldReader r = open_world(world_of(g));
    check_against(g, tables_of_world(r));
}

Tables fixture_run(const MCase& g, const wd::ContentSet& content) {
    CCase c;
    c.kind = "stress";
    c.name = g.name;
    c.seed = g.seed;
    c.span = g.span;
    c.races = g.races;
    c.civs = g.civs;
    c.intensity = g.intensity;
    const eng::HistoryInputs in = inputs_of(make_synthetic(g.seed), g.seed, settings_of(c), content);
    return eng::history_stage(content, in);
}

const std::vector<std::string>& i0_tables() {
    static const std::vector<std::string> v = {"event", "event_participant", "event_cause", "event_value", "phase",
                                               "civilization", "settlement", "settlement_census", "region_control",
                                               "relation", "region_name"};
    return v;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-02 18: the golden file lists the world cases of p3-06-conflict.txt, p5-monsters-high-1024 and fixtures") {
    std::set<std::string> worlds;
    for (const MCase& c : monsters_golden().cases) {
        if (c.kind == "world" && c.pressure == "normal") worlds.insert(c.name);
    }
    std::set<std::string> conflict;
    for (const CCase& c : conflict_golden()) {
        if (c.kind == "world") conflict.insert(c.name);
    }
    CHECK(worlds == conflict);
    const MCase& high = mcase("p5-monsters-high-1024");
    CHECK(high.seed == 7U);
    CHECK(high.size == 1024);
    CHECK(high.pressure == "high");
    // every kind appears and every event kind but beasts_bold is written over the monsters-small fixtures; at
    // 0.24.0 (P5-03 14.2: the slaughter that raises the dead scales with the pressure) no host rises on them, so
    // the undead and undead_rose are witnessed by the world cases instead (geo-seed-1-1024's m5, hist-conflict-73-
    // 1024's m6 and three more); at 0.25.0 (P5-03a 6: the threshold follows the world's population) a host rises
    // on the monsters-small fixtures too
    CHECK(monsters_golden().kinds_seen ==
          std::vector<std::string>{"dragon", "giant", "ogre", "troll", "undead", "wyvern"});
    CHECK(monsters_golden().events_seen.size() >= 10U);
    std::size_t undead = 0;
    for (const MCase& c : monsters_golden().cases) {
        for (const std::string& m : c.monsters) undead += m.find(" undead ") != std::string::npos ? 1U : 0U;
    }
    CHECK(undead >= 5U);
    // the table lines of the world cases equal p3-06-conflict.txt's
    for (const MCase& c : monsters_golden().cases) {
        if (c.kind != "world" || c.pressure != "normal") continue;
        CAPTURE(c.name);
        const CCase& cc = conflict_case(c.name.c_str());
        for (const char* t : {"beast_population", "monster", "site"}) CHECK(cc.tables.at(t) == c.tables.at(t).second);
    }
}

TEST_CASE("P5-02 14, 18: geo-seed-42-512 and geo-seed-0-512 against the golden file; invariants from the rows") {
    check_world_case(mcase("geo-seed-42-512"));
    check_world_case(mcase("geo-seed-0-512"));
}

TEST_CASE("P5-02 19.3: the fixtures monsters-small and zero against the golden file; invariants from the rows") {
    for (const MCase& g : monsters_golden().cases) {
        if (g.kind != "fixture") continue;
        const Tables t = fixture_run(g, g.data == "zero" ? zero_content() : monsters_small_content());
        check_against(g, t);
        if (g.data == "zero") {
            CHECK(t.monster.empty());
            // P5-04: only mines (kind 3) without monsters; P5-06: and towers (kind 4)
            for (const Row& s : t.site) CHECK((at(s, 2) == 3 || at(s, 2) == 4));
        }
    }
}

TEST_CASE("P5-02 7.5 I0: with the zero tuning the history tables equal those without the subsystem (stress maps)") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const CCase& c = conflict_case(("stress-" + std::to_string(seed)).c_str());   // p3-06-conflict.txt's grid
        const eng::HistoryInputs in = inputs_of(make_synthetic(seed), seed, settings_of(c), zero_both_content());
        const Tables with = eng::history_stage(zero_both_content(), in);
        const eng::HistoryInputs in2 = inputs_of(make_synthetic(seed), seed, settings_of(c), no_ghost_content());
        const ConflictConfig cfg(no_ghost_content(), false);   // peoples, conflict, renaming; no monsters, no view
        const Tables without = cfg.run(no_ghost_content(), in2);
        for (const std::string& name : i0_tables()) {
            CAPTURE(name);
            if (name == "settlement") {   // both carry ending (the column is derived from the log)
                CHECK(with.settlement == without.settlement);
            } else {
                CHECK(with.of(name) == without.of(name));
            }
        }
    }
}

// 19.1 item 3 on a world where boldness occurs (phase 1 found none on the fixtures): geo-seed-0-512 has two
// beasts_bold events with the shipped tuning of 0.23.0 (r1), one at 0.24.0. Each is checked against 6.2.3 from the rows.
TEST_CASE("P5-02 6.2.3: beasts_bold on geo-seed-0-512: settlement, owner, beast, toll and one per region and year") {
    const MCase& g = mcase("geo-seed-0-512");
    const Tables t = tables_of_world(open_world(world_of(g)));
    const auto k = kind_ids(t);
    std::map<std::int64_t, const Row*> stl;
    for (const Row& s : t.settlement) stl[at(s, 0)] = &s;
    std::map<std::int64_t, std::int64_t> menace;
    for (const Row& b : t.beast) menace[at(b, 0)] = at(b, 4);
    std::map<std::int64_t, std::vector<const Row*>> parts, vals;
    for (const Row& p : t.event_participant) parts[at(p, 0)].push_back(&p);
    for (const Row& v : t.event_value) vals[at(v, 0)].push_back(&v);
    std::map<std::int64_t, std::int64_t> phase_kind;
    for (const Row& p : t.phase) phase_kind[at(p, 0)] = at(p, 2);
    std::set<std::pair<std::int64_t, std::int64_t>> region_years;
    std::int64_t n = 0;
    for (const Row& e : t.event) {
        if (at(e, 3) != k.at("beasts_bold")) continue;
        ++n;
        const std::int64_t id = at(e, 0), year = at(e, 1), region = at(e, 5), sid = at(e, 6);
        CAPTURE(id);
        CHECK(phase_kind[at(e, 4)] != 2);   // quiet steps write none (6.2.3; phase kind 2 quiet)
        CHECK(region_years.insert({region, year}).second);   // at most one per region per year
        REQUIRE(stl.count(sid) == 1U);
        CHECK(at(*stl[sid], 4) == region);   // t lies in r
        std::int64_t ps = 0, pc = 0;
        for (const Row* p : parts[id]) {
            if (at(*p, 3) == 4) ps = at(*p, 4);
            if (at(*p, 3) == 3) pc = at(*p, 4);
        }
        CHECK(ps == sid);
        CHECK(pc >= 1);   // the owner (P3-05: every standing settlement has one)
        REQUIRE(vals[id].size() == 2U);   // 10.1: beast, dead
        const std::int64_t beast = at(*vals[id][0], 2), dead = at(*vals[id][1], 2);
        REQUIRE(menace.count(beast) == 1U);
        CHECK(menace[beast] > 0);   // P5-03 13: drawn by B x menace, so a species of menace 0 is never chosen
        CHECK(dead >= 0);
    }
    CHECK(n == 1);   // 0.24.0: one (p3-06-conflict.txt, geo-seed-0-512 events_beasts_bold = 1); two at 0.23.0
}

// 19.2 resume (P3-04 16.3 item 21): a world generated --until chronology and resumed through the history stage
// equals the world generated in one run, every history table (the five of schema 9 included), on a 512 world with
// --monster-pressure high (seed 7, the pressure of the world-hashes case), where raids, sleeps and hunts happen.
TEST_CASE("P5-02 19.2: resume from a --until chronology world equals one run (seed 7, 512, --monster-pressure high)") {
    chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("all"));
    p.pressure = "high";
    static dbtest::TempDir dir("monsters-resume");
    const std::string part = dir.file("part.db"), full = dir.file("full.db");
    REQUIRE(worldgen::generate_world(chrono_test::world_params(7, 512, p, shipped(), std::string("chronology")),
                                     shipped(), part, db::Overwrite::refuse)
                .ok());
    REQUIRE(worldgen::generate_world(chrono_test::world_params(7, 512, p, shipped()), shipped(), full,
                                     db::Overwrite::refuse)
                .ok());
    const db::WorldReader r = open_world(part);
    const Tables resumed = eng::history_stage(shipped(), eng::inputs_from_world(r, shipped()));
    const Tables one = tables_of_world(open_world(full));
    CHECK(!one.monster.empty());
    CHECK(count_kind(one, kind_ids(one).at("monster_raid")) > 0);
    for (const char* t : {"beast", "monster_kind", "beast_population", "monster", "site", "settlement", "event",
                          "event_participant", "event_value", "event_cause", "phase"}) {
        CAPTURE(t);
        CHECK(resumed.of(t) == one.of(t));
    }
    CHECK(resumed == one);
}

TEST_CASE("P5-02 19.2: the history of a 512 world is the same with 1 and 3 threads") {
    const MCase& g = mcase("geo-seed-42-512");
    const CCase& c = conflict_case(g.name.c_str());
    const db::WorldReader a = open_world(chrono_test::world(chrono_test::WorldKey{c.seed, "", c.size, c.span, c.races, "", 1}));
    const db::WorldReader b = open_world(chrono_test::world(chrono_test::WorldKey{c.seed, "", c.size, c.span, c.races, "", 3}));
    for (const char* t : {"beast_population", "monster", "site", "settlement"}) {
        CAPTURE(t);
        CHECK(rows_of(a, t) == rows_of(b, t));
    }
}

}  // TEST_SUITE

TEST_SUITE("monsters-slow" * doctest::skip()) {

TEST_CASE("P5-02 18: every other world case against the golden file; invariants from the rows") {
    for (const MCase& g : monsters_golden().cases) {
        if (g.kind == "world" && g.name != "geo-seed-42-512" && g.name != "geo-seed-0-512") {
            check_world_case(g);
        }
    }
}

TEST_CASE("P5-02 19.2: invariants on seeds 0 to 9 at 512 and 0 to 4 at 1024 with --monster-pressure high") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U}) {
        CAPTURE(seed);
        const db::WorldReader r =
            open_world(chrono_test::world(chrono_test::WorldKey{seed, "", 512, 6000, "all", "", 1}));
        CHECK(violations(tables_of_world(r)) == std::vector<std::string>{});
    }
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 4U}) {
        CAPTURE(seed);
        MCase g;
        g.name = "high-" + std::to_string(seed);
        g.seed = seed;
        g.size = 1024;
        g.span = 6000;
        g.races = "all";
        g.pressure = "high";
        const db::WorldReader r = open_world(world_of(g));
        CHECK(violations(tables_of_world(r)) == std::vector<std::string>{});
    }
}

}  // TEST_SUITE
