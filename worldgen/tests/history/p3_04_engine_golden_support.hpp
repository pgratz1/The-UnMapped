#pragma once

// Shared by the two files of the P3-04 16.3 and 16.4 world cases
// (history_engine_golden_test.cpp and history_engine_golden_resume_test.cpp), split so that each CTest entry holds about half of the run
// time (P3-08 4 item 1). The helpers moved here unchanged.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"

namespace engine_golden {

using namespace engine_test;

using namespace engine_test;

inline std::string section(const std::string& text, const std::string& header) {
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
inline std::string stub_world_of(const GoldenWorld& g) {
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

inline void check_world_case(const GoldenWorld& g) {
    CAPTURE(g.name);
    const std::string path = stub_world_of(g);
    const db::WorldReader r = open_world(path);
    const std::map<std::string, std::string> detail = chrono_test::detail_tables(r);
    for (const auto& [t, hex] : g.tables) {
        CAPTURE(t);
        REQUIRE(detail.count(t) == 1U);
        CHECK(detail.at(t) == hex);
    }
    CHECK(g.tables.size() == 21U);   // P5-03 16.1 (schema 10): two more;  // P5-02 11.1 (schema 9): five more; P3-04 r21 23.4: region_name (schema 8)
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
inline std::string first_difference(const eng::HistoryInputs& a, const eng::HistoryInputs& b) {
    const auto& [seed, map_size, params, regions, region_borders, region_biomes, races, race_biomes, cultures, gods,
                 ages, region_layer, water_layer, biomes, habitability_layer, river_layer, flow_dir_layer,
                 elevation_layer, ore_layer, walk_area_layer] = a;
    const auto& [seed2, map_size2, params2, regions2, region_borders2, region_biomes2, races2, race_biomes2, cultures2,
                 gods2, ages2, region_layer2, water_layer2, biomes2, habitability_layer2, river_layer2,
                 flow_dir_layer2, elevation_layer2, ore_layer2, walk_area_layer2] = b;
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
    // P3-05 r6 3.2 (generator 0.17.0).
    if (!same(elevation_layer, elevation_layer2)) return "elevation_layer";
    if (!same(ore_layer, ore_layer2)) return "ore_layer";
    if (!same(walk_area_layer, walk_area_layer2)) return "walk_area_layer";
    return a == b ? "" : "operator== (a member the bindings missed)";
}

inline std::optional<eng::HistoryInputs>& captured() {
    static std::optional<eng::HistoryInputs> c;
    return c;
}
inline void capture_stage(worldgen::stages::StageContext& ctx) { captured() = eng::inputs_from_context(ctx); }

inline void check_r1(const GoldenWorld& g) {
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

inline void check_resume(const GoldenWorld& g) {
    CAPTURE(g.name);
    const db::WorldReader part = open_world(world_of(g, 1, "chronology"));
    const db::WorldReader full = open_world(world_of(g));
    const Tables resumed = eng::history_stage(shipped(), eng::inputs_from_world(part, shipped()));
    CHECK(str(resumed.event) == str(tables_of_world(full).event));
    CHECK(resumed == tables_of_world(full));
}


// The default suite runs one 512 world case (geo-seed-42-512: every world it needs is generated once per
// process and shared with the other tests of the engine group); the other world cases are in the slow
// suite, so that the default CTest entry stays fast under every preset.
inline bool default_case(const GoldenWorld& g) { return g.name == "geo-seed-42-512"; }

}  // namespace engine_golden
