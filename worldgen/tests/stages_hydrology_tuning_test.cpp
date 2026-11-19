// Tuning of the hydrology stage: definition `worldgen.hydrology` (spec P1-02
// section 4; P1-00 section 6; P0-03 sections 4, 7 and amendment 16).
//
// Written from the specs only. Every case loads an in-memory data root
// through the production registry, so the checks are those a user meets
// when editing game/data/worldgen/hydrology.toml. The effect of the value
// on the river layer (T = ceil(area / 256)) is tested in
// stages_hydrology_examples_test.cpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_04_biomes_data.hpp"
#include "p3_01_history_fixture.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/geo/noise.hpp"

using namespace hydro_test;

namespace {

std::string all_errors(const hydro_api::TuningLoad& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

hydro_api::TuningLoad load(std::string_view hydrology) {
    return hydro_api::load_tuning_text(elev_test::elevation_toml(), hydrology);
}

void check_rejected(std::string_view hydrology, std::string_view mention) {
    const hydro_api::TuningLoad l = load(hydrology);
    CHECK_FALSE(l.ok());
    const std::string text = all_errors(l);
    INFO(text);
    CHECK(text.find(std::string(mention)) != std::string::npos);
}

std::string read_text(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

TEST_CASE("P1-02 4: the shipped value loads; the bounds 256 and 1000000000 are inclusive") {
    for (const std::int64_t v : {std::int64_t{256}, std::int64_t{257}, std::int64_t{125000}, std::int64_t{500000}, std::int64_t{1000000000}}) {
        CAPTURE(v);
        const hydro_api::TuningLoad l = load(hydrology_toml(v));
        CHECK_MESSAGE(l.ok(), all_errors(l));
        if (l.ok()) {
            static_cast<void>(hydro_api::tuning_from(*l.content));
        }
    }
}

TEST_CASE("P1-02 4: river_min_area_m2 outside int(256, 1000000000) is rejected") {
    for (const std::int64_t v : {std::int64_t{255}, std::int64_t{0}, std::int64_t{-1}, std::int64_t{1000000001},
                                 std::int64_t{1} << 40}) {
        CAPTURE(v);
        check_rejected(hydrology_toml(v), "river_min_area_m2");
    }
}

TEST_CASE("P1-02 4, P1-00 6.1: river_min_area_m2 is required and typed int; no unknown field") {
    SUBCASE("missing") {
        check_rejected("[worldgen.hydrology]\nflat_noise = " + std::string(SHIPPED_FLAT_NOISE) + "\n",
                       "river_min_area_m2");
    }
    SUBCASE("a string") { check_rejected("[worldgen.hydrology]\nriver_min_area_m2 = \"500000\"\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n", "river_min_area_m2"); }
    SUBCASE("a float") { check_rejected("[worldgen.hydrology]\nriver_min_area_m2 = 500000.0\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n", "river_min_area_m2"); }
    SUBCASE("an unknown field") {
        check_rejected("[worldgen.hydrology]\nriver_min_area_m2 = 500000\nlake_min_area_m2 = 1\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n", "lake_min_area_m2");
    }
}

TEST_CASE("P1-00 6.1: the hydrology definition is required beside elevation") {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", elev_test::elevation_toml(), false});
    wd::MemoryDataRoot root;
    // The biome data (P1-04 4.1, 4.2), so that only the definition under
    // test is missing.
    biome_data::add_to(registry, root, dir);
    root.directories.push_back(dir);
    history_fixture::add_to(root);  // P3-01: the required history kinds
    const wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    CHECK_FALSE(r.ok());
    std::string text;
    for (const auto& e : r.errors) {
        text += e.to_string() + "\n";
    }
    INFO(text);
    CHECK(text.find("hydrology") != std::string::npos);
}

TEST_CASE("P1-02 4, P1-00 6.4: the shipped file and both fixture files hold river_min_area_m2 = 125000 (lead, 2026-11-16)") {
    const std::filesystem::path root(UNMAPPED_TEST_SOURCE_ROOT);
    for (const char* rel : {"game/data/worldgen/hydrology.toml", "worldgen/tests/data/geo/worldgen/hydrology.toml",
                            "worldgen/tests/data/geo-small/worldgen/hydrology.toml"}) {
        CAPTURE(rel);
        const std::string text = read_text(root / rel);
        CHECK(text.find("[worldgen.hydrology]") != std::string::npos);
        CHECK(text.find("river_min_area_m2 = 125000") != std::string::npos);
        CHECK(text.find("flat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }") != std::string::npos);
        const hydro_api::TuningLoad l = load(text);
        CHECK_MESSAGE(l.ok(), all_errors(l));
    }
}

TEST_CASE("P1-02 4: flat_noise is a required fbm_m record; the shipped value loads as (400, 1, 0.5)") {
    const hydro_api::TuningLoad l = load(hydrology_toml(SHIPPED_AREA));
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    const hydro_api::Tuning t = hydro_api::tuning_from(*l.content);
    CHECK(t.river_min_area_m2 == SHIPPED_AREA);
    CHECK(t.flat_noise == worldgen::geo::FbmM{400, 1, worldgen::Fx::HALF});
    check_rejected("[worldgen.hydrology]\nriver_min_area_m2 = 125000\n", "flat_noise");
    check_rejected(hydrology_toml(SHIPPED_AREA, "400"), "flat_noise");
    check_rejected(hydrology_toml(SHIPPED_AREA, "\"400\""), "flat_noise");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, octaves = 1 }"), "gain");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, gain = \"0.5\" }"), "octaves");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ octaves = 1, gain = \"0.5\" }"), "wavelength_m");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ cycles = \"3\", octaves = 1, gain = \"0.5\" }"), "cycles");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, octaves = 1, gain = 0.5 }"), "floating-point");
}

TEST_CASE("P1-02 4, P1-01 4.1: flat_noise bounds: wavelength_m int(64, 65536), octaves int(1, 10), gain fx(0.0625, 1)") {
    for (const char* ok : {"{ wavelength_m = 64, octaves = 1, gain = \"0.5\" }",
                           "{ wavelength_m = 65536, octaves = 10, gain = 1 }",
                           "{ wavelength_m = 400, octaves = 1, gain = \"0.0625\" }"}) {
        CAPTURE(ok);
        const hydro_api::TuningLoad l = load(hydrology_toml(SHIPPED_AREA, ok));
        CHECK_MESSAGE(l.ok(), all_errors(l));
    }
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 63, octaves = 1, gain = \"0.5\" }"), "wavelength_m");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 65537, octaves = 1, gain = \"0.5\" }"),
                   "wavelength_m");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, octaves = 0, gain = \"0.5\" }"), "octaves");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, octaves = 11, gain = \"0.5\" }"), "octaves");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, octaves = 1, gain = \"0.0624\" }"), "gain");
    check_rejected(hydrology_toml(SHIPPED_AREA, "{ wavelength_m = 400, octaves = 1, gain = \"1.0001\" }"), "gain");
}
