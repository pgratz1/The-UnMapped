// worldgen-cli generate with the elevation stage (spec P1-00 section 14 X1
// and X3 for the first stage; P1-01 sections 3, 9 and E11).
//
// Written from the specs only. The data root is written into the sandbox:
// worldgen/elevation.toml with the shipped values of P1-01 4.4 and
// worldgen/hydrology.toml with those of P1-02 (the `worldgen` kind needs
// every id of its set when its directory exists, P1-00 6.1).

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "p3_01_history_data.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"

namespace {

using namespace clitest;

fs::path write_data_root(const Sandbox& sb) {
    const fs::path root = sb.file("data");
    fs::create_directories(root / "worldgen");
    write_text(root / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(root / "worldgen" / "hydrology.toml", "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n");
    write_text(root / "worldgen" / "climate.toml", std::string(elev_api::CLIMATE_TOML));  // P1-03 4
    write_text(root / "worldgen" / "biomes.toml", std::string(elev_api::BIOMES_TOML));  // P1-04 4.2
    write_text(root / "worldgen" / "resources.toml", std::string(elev_api::RESOURCES_TOML));  // P1-05 4.2
    write_text(root / "worldgen" / "regions.toml", std::string(elev_api::REGIONS_TOML));      // P1-06 4
    fs::create_directories(root / "biomes");
    write_text(root / "biomes" / "biomes.toml", std::string(elev_api::BIOME_DEFINITIONS_TOML));  // P1-04 4.1
    history_data::write_to(root);  // races/, cultures/, history/ (P3-01) when registered
    return root;
}

}  // namespace

TEST_CASE("cli P1-00 X1, X3: generate --until elevation at 512 writes one elevation layer, independent of --threads") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    for (const char* threads : {"2", "5"}) {
        CAPTURE(threads);
        const ProcessResult r = sb.cli({"generate", "--seed", "0", "--map-size", "512", "--until", "elevation",
                                        "--data", u8(data), "--threads", threads, "--out",
                                        u8(sb.file(std::string("t") + threads + ".db"))});
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
    }
    CHECK(read_bytes(sb.file("t2.db")) == read_bytes(sb.file("t5.db")));

    const ProcessResult h = sb.cli({"hash", "--db", u8(sb.file("t2.db")), "--detail"});
    REQUIRE(h.exit_code == EXIT_OK);
    std::vector<std::string> layer_lines;
    for (const std::string& line : lines_of(h.out)) {
        if (starts_with(line, "layer ")) {
            layer_lines.push_back(line);
        }
    }
    REQUIRE(layer_lines.size() == 1U);
    CHECK(starts_with(layer_lines[0], "layer elevation "));
    CHECK(layer_lines[0].size() == std::string("layer elevation ").size() + 64U);

    const ProcessResult rep = sb.cli({"report", "--db", u8(sb.file("t2.db"))});
    REQUIRE(rep.exit_code == EXIT_OK);
    CHECK(contains(rep.out, "\nelevation i16 512x512 codec=1 "));
    CHECK(contains(rep.out, "\n[geography]\n"));
    CHECK(contains(rep.out, "\ncells = 262144\n"));
    CHECK(contains(rep.out, "\nstages = elevation\n"));
}
