// worldgen-cli with the hydrology stage (spec P1-02 sections 3 and H9;
// P1-00 sections 10.1 to 10.3 and 14 X1, X2, X3).
//
// Written from the specs only. The data root is written into the sandbox
// with the shipped values of P1-01 4.4 and P1-02 4. The layer hashes of the
// 512 seed 0 world are compared with the golden case shipped-512-s0 of
// worldgen/tests/golden/p1-02-hydrology.txt (the test writer's reference),
// so the command line is checked against the same values as the library.
//
// Slow checks (X3 at 1, 2 and 5 threads for seeds 0 and 42) are in the test
// suite "hydrology-slow", skipped by default:
//   worldgen-cli-tests -ts=hydrology-slow --no-skip

#include <doctest/doctest.h>

#include <fstream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"

namespace {

using namespace clitest;

fs::path write_data_root(const Sandbox& sb) {
    const fs::path root = sb.file("data");
    fs::create_directories(root / "worldgen");
    write_text(root / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(root / "worldgen" / "hydrology.toml", "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n");
    if (elev_api::worldgen_id_registered("climate")) {
        write_text(root / "worldgen" / "climate.toml", elev_api::CLIMATE_TOML);  // P1-03 4
    }
    return root;
}

// The items of one case of the hydrology golden file.
std::map<std::string, std::string> golden_case(const std::string& name) {
    std::ifstream in(UNMAPPED_P1_02_GOLDEN_PATH, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open the hydrology golden file");
    std::map<std::string, std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string a, b, c;
        ss >> a >> b >> c;
        if (a == name && !b.empty()) {
            out[b] = c;
        }
    }
    REQUIRE_MESSAGE(!out.empty(), "no golden case " << name);
    return out;
}

// The "layer <name> <hex>" lines of hash --detail, in order.
std::vector<std::pair<std::string, std::string>> layer_lines(const Sandbox& sb, const fs::path& db) {
    const ProcessResult h = sb.cli({"hash", "--db", u8(db), "--detail"});
    INFO("stderr: ", h.err);
    REQUIRE(h.exit_code == EXIT_OK);
    std::vector<std::pair<std::string, std::string>> out;
    for (const std::string& line : lines_of(h.out)) {
        if (starts_with(line, "layer ")) {
            std::istringstream ss(line);
            std::string tag, name, hex;
            ss >> tag >> name >> hex;
            out.emplace_back(name, hex);
        }
    }
    return out;
}

ProcessResult generate(const Sandbox& sb, const fs::path& data, const std::vector<std::string>& extra,
                       const std::string& out) {
    std::vector<std::string> args = {"generate", "--map-size", "512", "--data", u8(data), "--out", u8(sb.file(out))};
    args.insert(args.end(), extra.begin(), extra.end());
    const ProcessResult r = sb.cli(args);
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return r;
}

const std::vector<std::string>& hydrology_layer_names() {
    static const std::vector<std::string> n = {"water", "water_depth", "flow_dir", "flow_accum",
                                               "river", "lake",        "coast_dist"};
    return n;
}

}  // namespace

TEST_CASE("cli P1-00 X1, P1-02 H9, P1-00 10.2, 10.3: generate --until hydrology at 512; hash, report and render") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    generate(sb, data, {"--seed", "0", "--until", "hydrology", "--threads", "2"}, "h.db");
    generate(sb, data, {"--seed", "0", "--until", "elevation", "--threads", "5"}, "e.db");
    generate(sb, data, {"--seed", "0"}, "d.db");  // default: every implemented stage

    // Layers in catalogue order, with the golden layer hashes of the
    // reference (case shipped-512-s0).
    const auto lines = layer_lines(sb, sb.file("h.db"));
    REQUIRE(lines.size() == 8U);

    // The default runs elevation, hydrology and every later implemented
    // stage (P1-00 10.1): its first eight layers are those of h.db.
    const auto dflt = layer_lines(sb, sb.file("d.db"));
    REQUIRE(dflt.size() >= 8U);
    for (std::size_t i = 0; i < 8; ++i) {
        CHECK(dflt[i] == lines[i]);
    }
    CHECK(lines[0].first == "elevation");
    const std::map<std::string, std::string> g = golden_case("shipped-512-s0");
    CHECK(lines[0].second == g.at("elevation"));
    for (std::size_t i = 0; i < hydrology_layer_names().size(); ++i) {
        const std::string& name = hydrology_layer_names()[i];
        CAPTURE(name);
        CHECK(lines[i + 1].first == name);
        CHECK(lines[i + 1].second == g.at(name));
    }

    // The elevation layer is the same with and without hydrology after it.
    const auto elev_only = layer_lines(sb, sb.file("e.db"));
    REQUIRE(elev_only.size() == 1U);
    CHECK(elev_only[0] == lines[0]);

    // report: stages, the layer lines with their catalogue types, and the
    // hydrology statistics after the elevation statistics, in 10.3 order.
    const ProcessResult rep = sb.cli({"report", "--db", u8(sb.file("h.db"))});
    INFO("stderr: ", rep.err);
    REQUIRE(rep.exit_code == EXIT_OK);
    CHECK(contains(rep.out, "\nstages = elevation,hydrology\n"));
    for (const char* layer : {"\nelevation i16 512x512 codec=1 ", "\nwater u8 512x512 codec=1 ",
                              "\nwater_depth u16 512x512 codec=1 ", "\nflow_dir u8 512x512 codec=1 ",
                              "\nflow_accum u32 512x512 codec=1 ", "\nriver u8 512x512 codec=1 ",
                              "\nlake u32 512x512 codec=1 ", "\ncoast_dist i16 512x512 codec=1 "}) {
        CAPTURE(std::string(layer));
        CHECK(contains(rep.out, layer));
    }
    std::size_t at = rep.out.find("\n[geography]\n");
    REQUIRE(at != std::string::npos);
    at = rep.out.find("\nedge_parallel_run_m = ", at);
    REQUIRE(at != std::string::npos);
    for (const char* key : {"lakes", "lake_cells", "lake_share_of_land", "largest_lake_cells", "river_cells",
                            "river_share_of_land", "river_sources", "river_mouths", "major_rivers",
                            "longest_river_m", "largest_basin_share"}) {
        CAPTURE(std::string(key));
        const std::size_t k = rep.out.find("\n" + std::string(key) + " = ", at);
        CHECK(k != std::string::npos);
        if (k != std::string::npos) {
            at = k;
        }
    }
    // The counts agree with the reference.
    CHECK(contains(rep.out, "\nlakes = " + g.at("lakes") + "\n"));
    CHECK(contains(rep.out, "\nriver_cells = " + g.at("river_cells") + "\n"));
    // An elevation-only world has no hydrology lines (appendix C 9).
    const ProcessResult rep_e = sb.cli({"report", "--db", u8(sb.file("e.db"))});
    REQUIRE(rep_e.exit_code == EXIT_OK);
    CHECK_FALSE(contains(rep_e.out, "\nlakes = "));

    // render (P1-00 10.2): the composite map draws water, so it differs from
    // the elevation-only one (seed 0 at 512 has lakes and rivers, golden
    // case shipped-512-s0); every hydrology layer renders under auto.
    for (const char* db : {"h.db", "e.db"}) {
        CAPTURE(db);
        const ProcessResult r = sb.cli({"render", "--db", u8(sb.file(db)), "--composite", "map"});
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
    }
    const Bytes with_water = read_bytes(sb.out_dir() / "h.map.png");
    CHECK(png_header(with_water).width == 512U);
    CHECK(png_header(with_water).height == 512U);
    CHECK(with_water != read_bytes(sb.out_dir() / "e.map.png"));
    for (const std::string& layer : hydrology_layer_names()) {
        CAPTURE(layer);
        const fs::path out = sb.file(layer + ".png");
        const ProcessResult r = sb.cli({"render", "--db", u8(sb.file("h.db")), "--layer", layer, "--out", u8(out)});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_OK);
        CHECK(file_exists(out));
    }
}

TEST_CASE("cli P1-00 10.1: --until hydrology is accepted; an unimplemented later stage is not") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    // climate (P1-03) is implemented after hydrology; biomes (P1-04) not yet
    // (cli_climate_test.cpp checks --until climate).
    const ProcessResult r = sb.cli({"generate", "--seed", "0", "--map-size", "512", "--until", "biomes", "--data",
                                    u8(data), "--out", u8(sb.file("c.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK_FALSE(file_exists(sb.file("c.db")));
    const ProcessResult tp = sb.cli({"generate", "--seed", "0", "--test-pattern", "--until", "hydrology", "--data",
                                     u8(data), "--out", u8(sb.file("t.db"))});
    CHECK(tp.exit_code == EXIT_USAGE);
}

TEST_SUITE("hydrology-slow" * doctest::skip()) {

TEST_CASE("cli P1-00 X3 (slow): generate at 512, seeds 0 and 42, --threads 1, 2 and 5: byte-identical files") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    for (const char* seed : {"0", "42"}) {
        CAPTURE(seed);
        for (const char* threads : {"1", "2", "5"}) {
            generate(sb, data, {"--seed", seed, "--threads", threads}, std::string("s") + seed + "-" + threads + ".db");
        }
        const Bytes a = read_bytes(sb.file(std::string("s") + seed + "-1.db"));
        CHECK(read_bytes(sb.file(std::string("s") + seed + "-2.db")) == a);
        CHECK(read_bytes(sb.file(std::string("s") + seed + "-5.db")) == a);
    }
    // seed 42 against the reference.
    const auto lines = layer_lines(sb, sb.file("s42-1.db"));
    const std::map<std::string, std::string> g = golden_case("shipped-512-s42");
    REQUIRE(lines.size() >= 8U);  // the default also runs every later implemented stage
    for (std::size_t i = 1; i < 8U; ++i) {
        CAPTURE(lines[i].first);
        CHECK(lines[i].second == g.at(lines[i].first));
    }
}

}  // TEST_SUITE
