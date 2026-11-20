// Spec P3-06 16.3 (golden vectors) and 16.2 items 16 and 18: the world
// cases of worldgen/tests/golden/p3-06-conflict.txt (written by the
// independent reference reference/p3_conflict_reference.py) generated with
// the shipped stages (generator 0.16.0: peoples and conflict): hash
// --detail's thirteen table lines, the [history] section, the [stats] keys
// of 15.4 from the rows, the per-war lines re-derived from the rows, the
// rendered sentences (every event of the 512 cases, every event of kinds
// 20-30 of the 1024 cases), the invariants of 14 from the rows; resume
// (P3-04 Y2: the history of an --until chronology world equals the complete
// world's) and threads 1 and 5 with conflict installed; the settings
// --event-intensity turbulent and --civs-per-race 4 (cases
// hist-conflict-turbulent-512, hist-conflict-civs4-512); the hard bounds of
// 15.5 on the 1024 cases; and the sample of 15.2 as the golden case
// hist-conflict-42-1024 (16.3 item 20), checked like the other 1024 worlds
// against the file, with its log below the first claim equal to the
// peoples-only world's (16.2 item 18a). The spec's 15.2 figures are
// 0.16.0's (r7) and none of them is asserted here.
//
// geo-seed-42-512 runs by default (three worlds: --until chronology,
// complete, and complete with five threads); every other case is in the
// doctest suite "history-slow" (worldgen-tests -ts=history-slow --no-skip).
// Written from the spec only; needs no P3-06 name of the library.

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/inspect/report.hpp"

namespace {

using namespace conflict_test;

// A world of a golden world case with the shipped data (every stage up to `until`, "" all), cached.
std::string world_path(const CCase& g, std::uint32_t threads = 1, const std::string& until = "") {
    const bool phrase = g.name == "geo-phrase-512";
    if (g.detail == "normal" && g.civs == "auto" && g.intensity == "normal") {
        return chrono_test::world(chrono_test::WorldKey{g.seed, phrase ? "the unmapped" : "", g.size, g.span, g.races,
                                                        until, threads});
    }
    static std::map<std::string, std::string> cache;
    const std::string key = g.name + "/" + std::to_string(threads) + "/" + until;
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    p.civs = g.civs == "auto" ? std::nullopt : std::optional<std::int64_t>(std::stoll(g.civs));
    p.intensity = g.intensity;
    const std::string path = chrono_test::world_dir().file("c" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams wp = chrono_test::world_params(
        g.seed, g.size, p, shipped(), until.empty() ? std::nullopt : std::optional<std::string>(until), threads);
    REQUIRE(worldgen::generate_world(wp, shipped(), path, db::Overwrite::refuse).ok());
    cache[key] = path;
    return path;
}

std::string section(const std::string& text, const std::string& header) {
    const std::size_t at = text.find(header + "\n");
    if (at == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find("\n[", at + 1);
    return text.substr(at, end == std::string::npos ? std::string::npos : end + 1 - at);
}

void check_world_case(const CCase& g) {
    CAPTURE(g.name);
    // Y2: the shipped history of the --until chronology world equals the complete world's
    const db::WorldReader part = open_world(world_path(g, 1, "chronology"));
    const eng::HistoryInputs in = eng::inputs_from_world(part, shipped());
    const Tables resumed = eng::history_stage(shipped(), in);
    const db::WorldReader r = open_world(world_path(g));
    const Tables t = tables_of_world(r);
    CHECK(resumed == t);
    const std::map<std::string, std::string> detail = chrono_test::detail_tables(r);
    CHECK(g.tables.size() == 13U);
    for (const auto& [name, hex] : g.tables) {
        CAPTURE(name);
        CHECK(detail.at(name) == hex);
    }
    CHECK(table_hashes(t) == g.tables);
    const auto rep = worldgen::inspect::build_report(r);
    REQUIRE(rep.ok());
    std::string want = "[history]\n";
    for (const std::string& l : g.report) {
        want += l + "\n";
    }
    CHECK(section(rep.value(), "[history]") == want);
    CHECK(report_lines_05(t, in) == g.report);
    CHECK(stats_lines_06(t, in) == g.stats);
    CHECK(war_lines_from_rows(t) == golden_wars_without_how(g));
    const std::vector<std::string> sent = eng::render_all(r, static_cast<std::int64_t>(t.event.size()));
    for (const auto& [id, text] : g.sentences) {
        CAPTURE(id);
        REQUIRE(static_cast<std::size_t>(id) <= sent.size());
        CHECK(sent[static_cast<std::size_t>(id - 1)] == text);
    }
    std::size_t conflict_events = 0;
    for (const Row& e : t.event) conflict_events += at(e, 3) >= k_claim ? 1U : 0U;
    CHECK(g.sentences.size() == (g.size <= 512 ? sent.size() : conflict_events));
    check_invariants_06(t, in, 25, true);
    check_invariants_09(t, in, shipped());  // P3-06 r9 20.13 (generator 0.18.0)
    check_invariants_10(t, shipped());      // P3-06 r10 20.19, 20.22
    // P3-07 r8 21 (reference-only until P3-07 is implemented): the golden notes and summary of the case exist
    CHECK_FALSE(g.summary.empty());
}

// 15.5 hard bounds from the rows; the capacity bound with the reference's C of the P3-05 golden file
// (same geography and races), the runaway bound from region_control.
void check_hard_bounds(const CCase& g, const char* peoples_case) {
    CAPTURE(g.name);
    const db::WorldReader r = open_world(world_path(g));
    const Tables t = tables_of_world(r);
    // the inputs of the --until chronology world (a complete world is refused as a resume input, P3-04 3.2)
    const eng::HistoryInputs in = eng::inputs_from_world(open_world(world_path(g, 1, "chronology")), shipped());
    std::int64_t alive = 0, standing = 0;
    for (const Row& c : t.civilization) alive += is_null(c[8]) ? 1 : 0;
    for (const Row& s : t.settlement) standing += (at(s, 10) == 1 || at(s, 10) == 2) ? 1 : 0;
    CHECK(alive >= 1);
    CHECK(standing >= 3);
    std::map<std::int64_t, std::int64_t> civ_race;
    for (const Row& c : t.civilization) civ_race[at(c, 0)] = at(c, 2);
    for (const Row& a : rows_of(r, "age")) {
        bool any = false;
        for (const Row& c : t.civilization) any = any || (at(c, 2) == a[1].integer && is_null(c[8]));
        CHECK_MESSAGE(any, "race " << a[1].integer << " extinct");  // races_alive = the present races
    }
    const GCase& pg = peoples_test::golden_case(peoples_case);
    for (const Row& s : t.settlement) {
        if (at(s, 10) == 1 || at(s, 10) == 2) {
            CHECK(at(s, 11) <= 2 * pg.score.at({at(s, 4), civ_race[at(s, 8)]}).second);
        }
    }
    const std::int64_t sim = simulated(t);
    CHECK(static_cast<std::int64_t>(t.event.size()) * 1000 <= 200 * sim * 1000);
    CHECK(sim * 1000 / in.params.span <= 450);
    // runaway: years in which one civilization holds more than 800 permille of the non-islet land
    std::set<std::int64_t> land;
    for (const db::RegionRow& rg : in.regions) {
        if (rg.cells - rg.lake_cells >= 1172) land.insert(rg.id);
    }
    std::set<std::int64_t> years = {in.params.span};
    for (const Row& rc : t.region_control) {
        years.insert(at(rc, 1));
        if (!is_null(rc[2])) years.insert(at(rc, 2));
    }
    std::int64_t over = 0;
    for (auto it = years.begin(); std::next(it) != years.end(); ++it) {
        std::map<std::int64_t, std::int64_t> held;
        for (const Row& rc : t.region_control) {
            if (land.count(at(rc, 0)) != 0 && at(rc, 1) <= *it && (is_null(rc[2]) || at(rc, 2) > *it)) {
                held[at(rc, 3)] += 1;
            }
        }
        std::int64_t m = 0;
        for (const auto& [c, n] : held) m = std::max(m, n * 1000 / static_cast<std::int64_t>(land.size()));
        over += m > 800 ? *std::next(it) - *it : 0;
    }
    CHECK(over <= 1000);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 16.3 item 19, 16.2 item 18: geo-seed-42-512 (tables, [history], [stats], wars, sentences, resume, threads)") {
    const CCase& g = conflict_case("geo-seed-42-512");
    check_world_case(g);
    CHECK(tables_of_world(open_world(world_path(g, 5))) == tables_of_world(open_world(world_path(g, 1))));
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-06 16.3 item 19: the other 512 golden worlds, with --event-intensity turbulent and --civs-per-race 4") {
    for (const char* name : {"geo-seed-0-512", "geo-phrase-512", "hist-span-2000-512", "hist-conflict-turbulent-512",
                             "hist-conflict-civs4-512"}) {
        check_world_case(conflict_case(name));
    }
    // the two settings reach the conflict history (P3-06 5.2: intensity scales strain; 16.2 item 15)
    const auto& base = conflict_case("geo-seed-42-512");
    CHECK(conflict_case("hist-conflict-turbulent-512").tables.at("relation") != base.tables.at("relation"));
    CHECK(conflict_case("hist-conflict-civs4-512").tables.at("relation") != base.tables.at("relation"));
}

TEST_CASE("P3-06 16.3 item 19, 16.2 item 16: the 1024 golden worlds and the hard bounds of 15.5") {
    const std::vector<std::pair<const char*, const char*>> cases = {
        {"geo-seed-1-1024", "geo-seed-1-1024"},
        {"hist-races-elf-orc-1024", "hist-races-elf-orc-1024"},
        {"hist-dense-1024", "hist-dense-1024"},
        {"hist-conflict-42-1024", "hist-peoples-42-1024"}};
    for (const auto& [name, peoples] : cases) {
        check_world_case(conflict_case(name));
        check_hard_bounds(conflict_case(name), peoples);
    }
}

// The sample of 15.2 (seed 42 at 1024: 1030 events, the five flare-ups, the end state, the wars and the
// quoted sentences) is generator 0.16.0's (P3-06 r7 15: "The 0.16.0 figures of 15 stay as measured"); from
// 0.17.0 (P3-08) the case is checked against the golden file like every other world, and 16.3 item 20
// becomes the peoples-only prefix of 16.2 item 18a (r7).
TEST_CASE("P3-06 16.3 item 20, 16.2 item 18a (r7): seed 42 at 1024 below its first claim is the peoples-only log") {
    const CCase& g = conflict_case("hist-conflict-42-1024");
    const Tables t = tables_of_world(open_world(world_path(g)));
    const GCase& pg = peoples_test::golden_case("hist-peoples-42-1024");
    chrono_api::Params p = chrono_test::params_with(pg.span, chrono_test::races_of_text(pg.races));
    const Tables pt = tables_of_world(open_world(peoples_test::peoples_world(pg.name, 42, "", 1024, p)));
    (void)check_peoples_prefix(t, pt);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
