// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r2) 6.5 and 6.6 item 2 on the hand fixture depths-forced
// (worldgen/tests/data/depths/depths-forced.toml; integration part 3, docs/reviews/p5-07-integration-notes.md): the
// two paths no generated world reaches. A seal forced at the gate (the hold that falls is its race's last refuge and
// 7.3's flight has no plan: "there was nowhere to flee") and a later take refused for want of a flight plan (the
// raid stands and nothing else happens). The fixture is depths-small (4.2) with `refuge_flight` 0 and
// `deep_gate_levy_permille` 0 on conflict stress geographies. Expected values are the reference's
// (p5_07_depths_reference.py golden: the `case depths-forced-<seed>` lines of golden/p5-07-depths.txt, with the
// tables, the statistics and the `path` lines of its replay log); the engine must agree.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <toml.hpp>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p3_04_engine_support.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"

using p503api::V;

namespace {

namespace fs = std::filesystem;
namespace wd = worldgen::data;

struct Forced {
    wd::ContentSet content;
    std::vector<conflict_test::CCase> cases;
};

// The fixture file: depths-small with its [conflict] and [monsters] edits (integers only), and its cases.
const Forced& forced() {
    static const Forced f = [] {
        const fs::path path = history_test::source_root() / "worldgen" / "tests" / "data" / "depths" / "depths-forced.toml";
        const toml::table doc = toml::parse_file(path.string());
        REQUIRE(doc["base"].value<std::string>() == std::optional<std::string>("depths-small"));
        Forced out;
        out.content = p507api::depths_small();
        for (const char* id : {"conflict", "monsters"}) {
            std::map<std::string, wd::Value> edits;
            if (const toml::table* t = doc[id].as_table()) {
                for (const auto& [k, v] : *t) {
                    REQUIRE(v.is_integer());
                    edits[std::string(k.str())] = V(*v.value<std::int64_t>());
                }
            }
            out.content = p503api::with_history(out.content, id, edits);
        }
        const toml::array* cs = doc["case"].as_array();
        REQUIRE(cs != nullptr);
        for (const toml::node& n : *cs) {
            const toml::table& t = *n.as_table();
            conflict_test::CCase c;
            c.kind = "stress";
            c.seed = static_cast<std::uint64_t>(*t["seed"].value<std::int64_t>());
            c.name = "depths-forced-" + std::to_string(c.seed);
            c.span = *t["span"].value<std::int64_t>();
            c.races = *t["races"].value<std::string>();
            c.civs = *t["civs"].value<std::string>();
            c.intensity = *t["intensity"].value<std::string>();
            out.cases.push_back(c);
        }
        return out;
    }();
    return f;
}

struct Expected {
    std::map<std::string, std::string> tables;
    std::vector<std::pair<std::string, std::int64_t>> stats;
    std::vector<std::vector<std::int64_t>> forced_seals, no_takes;   // (year, monster, event) / (year, monster, settlement)
};

// The case `name` of golden/p5-07-depths.txt (empty when absent).
Expected expected(const std::string& name) {
    Expected out;
    std::ifstream f(history_test::source_root() / "worldgen" / "tests" / "golden" / "p5-07-depths.txt", std::ios::binary);
    std::string line;
    bool in = false;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream s(line);
        std::string head;
        s >> head;
        if (head == "case") {
            std::string n;
            s >> n;
            in = n == name;
        } else if (!in) {
            continue;
        } else if (head == "table") {
            std::string t, h;
            s >> t >> h;
            out.tables[t] = h;
        } else if (head == "stat") {
            std::string k;
            std::int64_t v = 0;
            s >> k >> v;
            out.stats.emplace_back(k, v);
        } else if (head == "path") {
            std::string k;
            std::int64_t a = 0, b = 0, c = 0;
            s >> k >> a >> b >> c;
            (k == "forced_seal" ? out.forced_seals : out.no_takes).push_back({a, b, c});
        }
    }
    return out;
}

p503check::World run(const conflict_test::CCase& c, const wd::ContentSet& content) {
    const eng::HistoryInputs in =
        conflict_test::inputs_of(conflict_test::make_synthetic(c.seed), c.seed, conflict_test::settings_of(c), content);
    return p503check::world_of_run(p503api::history_stage(content, in), in, content);
}

std::int64_t year_of(const p503check::Index& x, std::int64_t e) { return p503check::at(*x.ev.at(e), 1); }
std::int64_t settlement_of(const p503check::Index& x, std::int64_t e) { return p503check::at(*x.ev.at(e), 6); }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 6.5, 6.6 item 2: depths-forced: a seal forced at a last refuge, later takes refused for want of a flight plan") {
    P507_REQUIRE_INTEGRATED();
    const Forced& f = forced();
    REQUIRE(f.cases.size() == 2U);
    std::size_t seals = 0, refused = 0;
    for (const conflict_test::CCase& c : f.cases) {
        CAPTURE(c.name);
        const Expected g = expected(c.name);
        if (g.tables.empty()) {
            MESSAGE(c.name << ": no lines in p5-07-depths.txt yet (written by the reference's golden)");
            continue;
        }
        const p503check::World w = run(c, f.content);
        // The engine agrees with the reference: the four tables and the statistics.
        for (const auto& [name, hash] : g.tables) {
            CAPTURE(name);
            CHECK(engine_test::table_hash(engine_test::columns_of(name), w.rows.t.of(name)) == hash);
        }
        CHECK(p507_check::stats(w) == g.stats);
        // 13's invariants that rows show.
        CHECK(p507_check::violations(w, p507_check::tuning_of(f.content)) == std::vector<std::string>{});
        const p503check::Index x(w);
        // refuge_flight 0: no flight ever follows a hold lost (7.3 has no plan).
        for (const std::int64_t e : x.events_of("refugees_settled")) {
            for (const std::int64_t k : x.cause_list(e)) CHECK(x.kind_of(k) != "hold_lost");
        }
        // 6.5: each forced seal is a seal of its monster in its year, and that horror never takes a hold.
        for (const std::vector<std::int64_t>& s : g.forced_seals) {
            CAPTURE(s[2]);
            const std::string k = x.kind_of(s[2]);
            CHECK((k == "deep_sealed" || k == "warren_sealed"));
            CHECK(year_of(x, s[2]) == s[0]);
            CHECK(p507_check::names_monster(x, s[2], s[1]));
            for (const std::int64_t e : x.events_of("hold_lost")) CHECK_FALSE(p507_check::names_monster(x, e, s[1]));
            ++seals;
        }
        // 6.6 item 2: each refused take is a raid of the horror on the settlement in that year that the settlement
        // outlives the year, with no hold_lost of it by that horror in that year.
        for (const std::vector<std::int64_t>& r : g.no_takes) {
            CAPTURE(r[0]);
            CAPTURE(r[2]);
            bool raided = false;
            for (const std::int64_t e : x.events_of("monster_raid")) {
                raided = raided || (year_of(x, e) == r[0] && settlement_of(x, e) == r[2] &&
                                    p507_check::names_monster(x, e, r[1]));
            }
            CHECK(raided);
            for (const std::int64_t e : x.events_of("hold_lost")) {
                CHECK_FALSE((year_of(x, e) == r[0] && settlement_of(x, e) == r[2]));
            }
            ++refused;
        }
    }
    // The fixture exists for these two paths: both must occur.
    if (!expected(f.cases.front().name).tables.empty()) {
        CHECK(seals >= 1U);
        CHECK(refused >= 1U);
    }
}

TEST_CASE("P5-07 depths-forced: the same tables twice") {
    P507_REQUIRE_INTEGRATED();
    const Forced& f = forced();
    const conflict_test::CCase& c = f.cases.front();
    const p503check::World a = run(c, f.content);
    const p503check::World b = run(c, f.content);
    for (const char* t : {"event", "event_participant", "event_cause", "event_value", "settlement", "monster", "site"}) {
        CAPTURE(t);
        CHECK(engine_test::table_hash(engine_test::columns_of(t), a.rows.t.of(t)) ==
              engine_test::table_hash(engine_test::columns_of(t), b.rows.t.of(t)));
    }
}

}  // TEST_SUITE
