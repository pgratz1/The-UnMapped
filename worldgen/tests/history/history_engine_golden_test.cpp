// Spec P3-04 16.3 and 16.4: the world cases of
// worldgen/tests/golden/p3-04-engine.txt (written by the independent
// reference reference/p3_engine_reference.py): hash --detail's thirteen
// table lines, the [history] section, the phase table with triggers and
// the rendered sentence of every event, byte for byte (render_event is a
// cross-platform contract); the worked example of 17 with both
// ForcedTension variants through run_engine; R1 (history_inputs_from in the
// pipeline equals history_inputs_from_world, member by member), R2 (the
// member counts of HistoryInputs and HistoryParams), Y2 resume with each
// refusal of 3.2, and threads 1, 2, 5. The 512 cases run by default; the
// 1024 cases are in the doctest suite "history-slow"
// (worldgen-tests -ts=history-slow --no-skip).

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"

namespace {

using namespace engine_test;

std::string section(const std::string& text, const std::string& header) {
    const std::size_t at = text.find(header + "\n");
    if (at == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find("\n[", at + 1);
    return text.substr(at, end == std::string::npos ? std::string::npos : end + 1 - at);
}

// A golden world case with the stub configuration (P3-04 7) in place of the shipped history stage, which
// is peoples since generator 0.15.0 (P3-05 13): every stage up to chronology, then the stub history.
// Cached per process.
std::string stub_world_of(const GoldenWorld& g) {
    static std::map<std::string, std::string> cache;
    if (const auto it = cache.find(g.name); it != cache.end()) {
        return it->second;
    }
    static dbtest::TempDir dir("engine-stub-worlds");
    std::vector<worldgen::stages::StageInfo> stages = worldgen::stages::stages_until(std::string("chronology"));
    stages.push_back(eng::stub_stage_info());
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    worldgen::WorldParams wp = chrono_test::world_params(g.seed, g.map_size, p, shipped());
    if (g.name == "geo-phrase-512") {
        wp.seed_phrase = "the unmapped";
    }
    const std::string path = dir.file(g.name + ".db");
    REQUIRE(worldgen::stages::testing::generate_world_with_stages(wp, shipped(), stages, path).ok());
    cache[g.name] = path;
    return path;
}

void check_world_case(const GoldenWorld& g) {
    CAPTURE(g.name);
    const std::string path = stub_world_of(g);
    const db::WorldReader r = open_world(path);
    const std::map<std::string, std::string> detail = chrono_test::detail_tables(r);
    for (const auto& [t, hex] : g.tables) {
        CAPTURE(t);
        REQUIRE(detail.count(t) == 1U);
        CHECK(detail.at(t) == hex);
    }
    CHECK(g.tables.size() == 13U);
    const Tables t = tables_of_world(r);
    CHECK(table_hashes(t) == g.tables);  // the test's own P0-04 8 hash of the stored rows agrees
    CHECK(phases_text(t, true) == g.phases);
    const auto rep = worldgen::inspect::build_report(r);
    REQUIRE(rep.ok());
    std::string want = "[history]\n";
    for (const std::string& l : g.report) {
        want += l + "\n";
    }
    CHECK(section(rep.value(), "[history]") == want);
    const std::vector<std::string> got = eng::render_all(r, static_cast<std::int64_t>(t.event.size()));
    REQUIRE(got.size() == g.sentences.size());
    for (std::size_t i = 0; i < got.size(); ++i) {
        CAPTURE(i + 1);
        CHECK(got[i] == g.sentences[i]);
    }
}

// R2: exactly these members; adding one fails to compile until this test, both
// constructors and R1 are extended (3.2).
std::string first_difference(const eng::HistoryInputs& a, const eng::HistoryInputs& b) {
    const auto& [seed, map_size, params, regions, region_borders, region_biomes, races, race_biomes, cultures, gods,
                 ages, region_layer, water_layer, biomes, habitability_layer, river_layer, flow_dir_layer] = a;
    const auto& [seed2, map_size2, params2, regions2, region_borders2, region_biomes2, races2, race_biomes2, cultures2,
                 gods2, ages2, region_layer2, water_layer2, biomes2, habitability_layer2, river_layer2,
                 flow_dir_layer2] = b;
    const auto& [span, detail, civs, prace, intensity, pressure, activity, script] = params;
    const auto& [span2, detail2, civs2, prace2, intensity2, pressure2, activity2, script2] = params2;
    if (seed != seed2) return "seed";
    if (map_size != map_size2) return "map_size";
    if (span != span2) return "params.span";
    if (detail != detail2) return "params.detail";
    if (civs != civs2) return "params.civs_per_race";
    if (prace != prace2) return "params.races";
    if (intensity != intensity2) return "params.event_intensity";
    if (pressure != pressure2) return "params.monster_pressure";
    if (activity != activity2) return "params.divine_activity";
    if (script != script2) return "params.divine_script";
    if (regions != regions2) return "regions";
    if (region_borders != region_borders2) return "region_borders";
    if (region_biomes != region_biomes2) return "region_biomes";
    if (races != races2) return "races";
    if (race_biomes != race_biomes2) return "race_biomes";
    if (cultures != cultures2) return "cultures";
    if (gods != gods2) return "gods";
    if (ages != ages2) return "ages";
    if (region_layer.width != region_layer2.width || region_layer.height != region_layer2.height ||
        region_layer.values != region_layer2.values) {
        return "region_layer";
    }
    if (water_layer.width != water_layer2.width || water_layer.height != water_layer2.height ||
        water_layer.values != water_layer2.values) {
        return "water_layer";
    }
    // P3-05 3.2.
    if (biomes != biomes2) return "biomes";
    const auto same = [](const db::LayerGrid& x, const db::LayerGrid& y) {
        return x.width == y.width && x.height == y.height && x.values == y.values;
    };
    if (!same(habitability_layer, habitability_layer2)) return "habitability_layer";
    if (!same(river_layer, river_layer2)) return "river_layer";
    if (!same(flow_dir_layer, flow_dir_layer2)) return "flow_dir_layer";
    return a == b ? "" : "operator== (a member the bindings missed)";
}

std::optional<eng::HistoryInputs>& captured() {
    static std::optional<eng::HistoryInputs> c;
    return c;
}
void capture_stage(worldgen::stages::StageContext& ctx) { captured() = eng::inputs_from_context(ctx); }

void check_r1(const GoldenWorld& g) {
    CAPTURE(g.name);
    static dbtest::TempDir dir("engine-r1");
    std::vector<worldgen::stages::StageInfo> stages = worldgen::stages::stages_until(std::string("chronology"));
    stages.push_back(worldgen::stages::StageInfo{"history", &capture_stage});
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    worldgen::WorldParams wp = chrono_test::world_params(g.seed, g.map_size, p, shipped());
    if (g.name == "geo-phrase-512") {
        wp.seed_phrase = "the unmapped";
    }
    captured().reset();
    REQUIRE(worldgen::stages::testing::generate_world_with_stages(wp, shipped(), stages, dir.file(g.name + ".db")).ok());
    REQUIRE(captured().has_value());
    const db::WorldReader r = open_world(world_of(g, 1, "chronology"));
    const eng::HistoryInputs stored = eng::inputs_from_world(r, shipped());
    CHECK(first_difference(*captured(), stored) == "");
}

void check_resume(const GoldenWorld& g) {
    CAPTURE(g.name);
    const db::WorldReader part = open_world(world_of(g, 1, "chronology"));
    const db::WorldReader full = open_world(world_of(g));
    const Tables resumed = eng::history_stage(shipped(), eng::inputs_from_world(part, shipped()));
    CHECK(str(resumed.event) == str(tables_of_world(full).event));
    CHECK(resumed == tables_of_world(full));
}

}  // namespace

// The default suite runs one 512 world case (geo-seed-42-512: every world it needs is generated once per
// process and shared with the other tests of the engine group); the other world cases are in the slow
// suite, so that the default CTest entry stays fast under every preset.
bool default_case(const GoldenWorld& g) { return g.name == "geo-seed-42-512"; }

TEST_SUITE("history") {

TEST_CASE("P3-04 16.4: the 512 world case geo-seed-42-512: tables, [history], phases and every sentence") {
    check_world_case(golden().world("geo-seed-42-512"));
}

TEST_CASE("P3-04 16.3 item 19 (R1), item 20 (R2): HistoryInputs from the pipeline equals from the world (geo-seed-42-512)") {
    check_r1(golden().world("geo-seed-42-512"));
}

TEST_CASE("P3-04 16.3 item 21 (Y2): resume from a --until chronology world equals one run (geo-seed-42-512)") {
    check_resume(golden().world("geo-seed-42-512"));
}

TEST_CASE("P3-04 3.2, 16.3 item 21: each refusal of history_inputs_from_world") {
    const GoldenWorld& g = golden().world("geo-seed-42-512");
    const std::string part = world_of(g, 1, "chronology");
    // the data content hash
    {
        std::vector<ReportEntry> r = shipped_report();
        r[0].salience = 99;
        const wd::ContentSet other = with_report(r);
        const db::WorldReader w = open_world(part);
        CHECK(fatal_of([&] { (void)eng::inputs_from_world(w, other); }) ==
              "history: resume: the world's data content hash differs from the loaded data");
    }
    static dbtest::TempDir dir("engine-resume");
    // the generator version
    {
        const std::string path = dir.file("version.db");
        dbtest::write_file(path, dbtest::read_file(part));
        REQUIRE(worldgen::db::testing::execute_on_file(
                    path, "UPDATE meta SET value = '0.13.9' WHERE key = 'generator_version'")
                    .ok());
        const db::WorldReader w = open_world(path);
        CHECK(fatal_of([&] { (void)eng::inputs_from_world(w, shipped()); }) ==
              "history: resume: the world was generated by 0.13.9, this build is " +
                  std::string(worldgen::GENERATOR_VERSION));
    }
    // meta.stages: a full world and a geography-only world
    {
        const db::WorldReader w = open_world(world_of(g));
        CHECK(fatal_of([&] { (void)eng::inputs_from_world(w, shipped()); }) ==
              "history: resume: meta.stages is 'elevation,hydrology,climate,biomes,resources,regions,chronology,"
              "history', expected the stages up to chronology");
        // a geography-only world: history-slow (one more world to generate)
    }
    // The schema refusal ("history: resume: schema version <v>, this build reads 7") cannot be reached:
    // WorldReader::open already refuses another version (history_engine_schema_test.cpp).
}

TEST_CASE("P3-04 16.3 item 18, 14.1: threads 1 and 5 give identical history tables (2 in history-slow)") {
    const GoldenWorld& g = golden().world("geo-seed-42-512");
    const Tables one = tables_of_world(open_world(world_of(g, 1)));
    CHECK(tables_of_world(open_world(world_of(g, 5))) == one);
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-04 16.4, 16.3 items 19, 21: the other 512 world cases: tables, sentences, R1 and Y2") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.map_size == 512 && g.tension == "none" && !default_case(g)) {
            check_world_case(g);
            check_r1(g);
            check_resume(g);
        }
    }
}

TEST_CASE("P3-04 3.2: resume refuses a geography-only world") {
    const GoldenWorld& g = golden().world("geo-seed-42-512");
    const db::WorldReader geo = open_world(world_of(g, 1, "regions"));
    CHECK(fatal_of([&] { (void)eng::inputs_from_world(geo, shipped()); }) ==
          "history: resume: meta.stages is 'elevation,hydrology,climate,biomes,resources,regions', expected the "
          "stages up to chronology");
}

TEST_CASE("P3-04 16.3 item 18: threads 2 gives the tables of threads 1") {
    const GoldenWorld& g = golden().world("geo-seed-42-512");
    CHECK(tables_of_world(open_world(world_of(g, 2))) == tables_of_world(open_world(world_of(g, 1))));
}

TEST_CASE("P3-04 16.4: the 1024 world cases: tables, [history], phases and every sentence") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.map_size == 1024 && g.tension == "none") {
            check_world_case(g);
        }
    }
}

TEST_CASE("P3-04 17, 16.4: example-42-1024 with ForcedTension(500, 1000, 1) and (1, 5000, 1) through run_engine") {
    const GoldenWorld& base = golden().world("example-42-1024");
    const db::WorldReader part = open_world(world_of(base, 1, "chronology"));
    const eng::HistoryInputs in = eng::inputs_from_world(part, shipped());
    for (const char* name : {"example-42-1024-ft500", "example-42-1024-ft1"}) {
        const GoldenWorld& g = golden().world(name);
        CAPTURE(g.name);
        Config cfg;
        cfg.handler = eng::make_stub();
        cfg.catalogue = eng::catalogue_of(shipped());
        if (g.tension == "ForcedTension(500,1000,1)") {
            cfg.add_tension(eng::forced_tension(500, 1000, 1));
        } else {
            cfg.add_tension(eng::forced_tension(1, 5000, 1));
        }
        const Tables t = eng::run_engine(shipped(), in, cfg.get());
        CHECK(table_hashes(t) == g.tables);
        CHECK(phases_text(t, true) == g.phases);
        std::vector<std::string> lines = report_lines(t, 6000);
        CHECK(lines == g.report);
    }
}

TEST_CASE("P3-04 16.3 items 19, 21: R1 and Y2 on the 1024 world cases") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.map_size == 1024 && g.tension == "none") {
            check_r1(g);
            check_resume(g);
        }
    }
}

}  // TEST_SUITE
