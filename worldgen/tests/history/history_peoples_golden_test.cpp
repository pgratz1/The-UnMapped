// Spec P3-05 16.3 (golden vectors) and 16.2 items 18 and 20: the world
// cases of worldgen/tests/golden/p3-05-peoples.txt (written by the
// independent reference reference/p3_peoples_reference.py): hash --detail's
// thirteen table lines, the [history] section with the keys of 15.4, the
// homes of every awakening, sigma and C of every region and race through
// PeoplesView, the rendered sentences (every event of the 512 cases, the
// first 300 and last 50 of the 1024 cases); resume (P3-04 Y2) and threads 1
// and 5 with peoples installed; the hard bounds of 15.5 on the 1024 cases.
// The sample of 15.2 is the golden case hist-peoples-42-1024 (16.3 item 22),
// checked like the other 1024 worlds: the spec's text is illustrative of
// 0.15.0 (r7) and no figure of it is asserted here.
//
// Since generator 0.16.0 the worlds are the peoples-only configuration
// (p3_05_peoples_support.hpp, peoples_world: every stage up to chronology,
// then the engine with peoples alone); the shipped stage with conflict is
// tested against p3-06-conflict.txt by the P3-06 tests.
//
// geo-seed-42-512 runs by default (two worlds: --until chronology and
// complete); every other case is in the doctest suite "history-slow"
// (worldgen-tests -ts=history-slow --no-skip). Empty until the
// implementation's header exists (p3_05_peoples_api.hpp).

#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/inspect/report.hpp"

namespace {

using namespace peoples_test;

// A world of a golden world case with the shipped data, cached per process: up to chronology (until
// "chronology"), or the peoples-only history (the P3-05 golden file's since generator 0.16.0; the shipped
// stage adds conflict, P3-06, whose golden worlds are p3-06-conflict.txt).
std::string world_path(const GCase& g, std::uint32_t threads = 1, const std::string& until = "") {
    const bool phrase = g.name == "geo-phrase-512";
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    if (until.empty()) {
        return peoples_world(g.name, g.seed, phrase ? "the unmapped" : "", g.size, p, threads);
    }
    if (g.detail == "normal") {
        return chrono_test::world(chrono_test::WorldKey{g.seed, phrase ? "the unmapped" : "", g.size, g.span, g.races,
                                                        until, threads});
    }
    static std::map<std::string, std::string> cache;
    const std::string key = g.name + "/" + std::to_string(threads) + "/" + until;
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    const std::string path = chrono_test::world_dir().file("p" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams wp = chrono_test::world_params(g.seed, g.size, p, shipped(), std::optional<std::string>(until),
                                                         threads);
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

void check_world_case(const GCase& g) {
    CAPTURE(g.name);
    // --until chronology: resume (Y2) and the PeoplesView probe of sigma and C
    const db::WorldReader part = open_world(world_path(g, 1, "chronology"));
    const eng::HistoryInputs in = eng::inputs_from_world(part, shipped());
    PeoplesConfig cfg(shipped());
    const PeoplesConfig* p = &cfg;
    bool probed = false;
    cfg.add_after({eng::Step::t2}, [&](eng::StepContext&) {
        if (probed) return;
        probed = true;
        for (const auto& [key, sc] : g.score) {
            CAPTURE(key.first);
            CAPTURE(key.second);
            CHECK(p->view().score(key.first, key.second) == sc.first);
            CHECK(p->view().capacity(key.first, key.second) == sc.second);
        }
    });
    const Tables probed_run = eng::run_engine(shipped(), in, cfg.get());
    CHECK(probed);
    PeoplesConfig plain(shipped());
    const Tables resumed = eng::run_engine(shipped(), in, plain.get());
    CHECK(probed_run == resumed);
    // the complete world
    const db::WorldReader r = open_world(world_path(g));
    const Tables t = tables_of_world(r);
    CHECK(resumed == t);  // Y2 with peoples installed (16.2 item 20; the peoples-only stage since 0.16.0)
    const std::map<std::string, std::string> detail = chrono_test::detail_tables(r);
    CHECK(g.tables.size() == 14U);  // P3-04 r21 23.4: region_name (schema 8)
    for (const auto& [name, hex] : g.tables) {
        CAPTURE(name);
        CHECK(detail.at(name) == hex);
    }
    CHECK(table_hashes(t) == g.tables);
    CHECK(homes_from_rows(t) == homes_without_n(g));
    const auto rep = worldgen::inspect::build_report(r);
    REQUIRE(rep.ok());
    std::string want = "[history]\n";
    for (const std::string& l : g.report) {
        want += l + "\n";
    }
    CHECK(section(rep.value(), "[history]") == want);
    CHECK(report_lines_05(t, in) == g.report);
    const std::vector<std::string> sent = eng::render_all(r, static_cast<std::int64_t>(t.event.size()));
    for (const auto& [id, text] : g.sentences) {
        CAPTURE(id);
        REQUIRE(static_cast<std::size_t>(id) <= sent.size());
        CHECK(sent[static_cast<std::size_t>(id - 1)] == text);
    }
    if (g.size <= 512) {
        CHECK(g.sentences.size() == sent.size());
    }
}

// 15.5 hard bounds from the rows (the capacity bound with the reference's C).
void check_hard_bounds(const GCase& g) {
    CAPTURE(g.name);
    const db::WorldReader r = open_world(world_path(g));
    const Tables t = tables_of_world(r);
    std::int64_t alive = 0, standing = 0;
    for (const Row& c : t.civilization) alive += is_null(c[8]) ? 1 : 0;
    for (const Row& s : t.settlement) standing += (at(s, 10) == 1 || at(s, 10) == 2) ? 1 : 0;
    CHECK(alive >= 1);
    CHECK(standing >= (g.size >= 1024 ? 3 : 1));
    std::set<std::int64_t> races_with_civ;
    std::map<std::int64_t, std::int64_t> civ_race;
    for (const Row& c : t.civilization) {
        races_with_civ.insert(at(c, 2));
        civ_race[at(c, 0)] = at(c, 2);
    }
    for (const Row& a : rows_of(r, "age")) {
        CHECK(races_with_civ.count(a[1].integer) == 1U);
    }
    for (const Row& s : t.settlement) {
        if (at(s, 10) == 1 || at(s, 10) == 2) {
            CHECK(at(s, 11) <= 2 * g.score.at({at(s, 4), civ_race[at(s, 8)]}).second);
        }
    }
    CHECK(static_cast<std::int64_t>(t.event.size()) * 1000 <= 200 * simulated(t) * 1000);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 16.3 item 21, 16.2 item 20: geo-seed-42-512 (tables, [history], homes, sigma and C, sentences, resume)") {
    check_world_case(golden_case("geo-seed-42-512"));
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-05 16.3 item 21: the other 512 golden worlds") {
    for (const char* name : {"geo-seed-0-512", "geo-phrase-512", "hist-span-2000-512"}) {
        check_world_case(golden_case(name));
    }
}

TEST_CASE("P3-05 16.3 item 21, 16.2 item 18: the 1024 golden worlds and the hard bounds of 15.5") {
    for (const char* name : {"geo-seed-1-1024", "hist-races-elf-orc-1024", "hist-dense-1024", "hist-peoples-42-1024"}) {
        check_world_case(golden_case(name));
        check_hard_bounds(golden_case(name));
    }
}

TEST_CASE("P3-05 r6 6.2, 8.4: on a real 1024 world dwarves found fortresses off the walkable ground and ladder kinds on it") {
    // seed 42 at 1024 (peoples only), real layers elevation, ore and walk_area: the counts of the reference's
    // golden line (0.18.0: five fortresses and three of ladder kinds; the cell rule does not make every
    // dwarven founding a fortress)
    const GCase& g = golden_case("hist-peoples-42-1024");
    const db::WorldReader r = open_world(world_path(g));
    const Tables t = tables_of_world(r);
    const auto walk = r.read_layer("walk_area");
    REQUIRE(walk.ok());
    const auto& wa = std::get<std::vector<std::uint16_t>>(walk.value().grid.values);
    std::map<std::int64_t, std::int64_t> civ_race, first_kind;
    for (const Row& c : t.civilization) civ_race[at(c, 0)] = at(c, 2);
    for (const Row& c : t.settlement_census) first_kind.emplace(at(c, 0), at(c, 3));
    std::int64_t forts = 0, ladder = 0;
    for (const Row& s : t.settlement) {
        if (civ_race.at(at(s, 5)) != 3) continue;
        const bool fort = first_kind.at(at(s, 0)) == fortress;
        (fort ? forts : ladder) += 1;
        if (wa.at(static_cast<std::size_t>(at(s, 3) * 1024 + at(s, 2))) == 0) {
            CHECK_MESSAGE(fort, "dwarven settlement " << at(s, 0) << " on walk_area 0 is not a fortress");
        }
    }
    // P3-10 4 item 2: the reference's line `hist-peoples-42-1024 dwarf_kinds forts <n> ladder <m>`
    REQUIRE(g.dwarf_forts >= 0);
    REQUIRE(g.dwarf_ladder >= 0);
    CHECK(forts == g.dwarf_forts);
    CHECK(ladder == g.dwarf_ladder);
}

TEST_CASE("P3-05 16.2 item 20: threads 1 and 5 give identical history tables") {
    for (const char* name : {"geo-seed-42-512", "hist-dense-1024"}) {
        const GCase& g = golden_case(name);
        CAPTURE(g.name);
        CHECK(tables_of_world(open_world(world_path(g, 5))) == tables_of_world(open_world(world_path(g, 1))));
    }
}

// The sample of 15.2 (seed 42 at 1024: its first 60 sentences, 530 events and the end state) is the
// prototype's 0.15.0 measurement, reproduced through 0.16.0; P3-05 r6 changes it (region names of P1-06 r3,
// names of P3-02 r5, the r6 rules), and the spec keeps the 0.15.0 text. From 0.17.0 the case
// hist-peoples-42-1024 is checked against the golden file like the other 1024 worlds (above).

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
