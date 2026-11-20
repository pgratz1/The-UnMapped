// worldgen-cli with the climate stage (spec P1-03 sections 3, 9 and C7;
// P1-00 sections 10.1 to 10.3 and 14 X1, X2).
//
// Written from the specs only. The data root is written into the sandbox
// with the shipped values of P1-01 4.4, P1-02 4 and P1-03 4. The climate
// report values are recomputed here from the layers of the world file
// (read with the library's reader) by the definitions of P1-03 9 and
// P1-00 10.3. When the climate golden file holds the case shipped-512-s0
// (worldgen/tests/golden/p1-03-climate.txt, the test writer's reference),
// the layer hashes and report values are compared with it as well.
//
// Slow checks are in the test suite "climate-slow", skipped by default:
//   worldgen-cli-tests -ts=climate-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "p3_01_history_fixture.hpp"
#include "cli_test_support.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "worldgen/db/reader.hpp"

namespace {

using namespace clitest;

constexpr std::string_view HYDROLOGY_TOML =
    "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n";

// P1-03 section 4.
constexpr std::string_view CLIMATE_TOML =
    "[worldgen.climate]\n"
    "temp_north_c = 5\n"
    "temp_south_c = 9\n"
    "lapse_c_per_km = \"6.5\"\n"
    "temp_noise = { wavelength_m = 6000, octaves = 3, gain = \"0.5\" }\n"
    "temp_noise_amp_c = 1\n"
    "wind_weights = [1, 1, 1, 1, 1, 1, 1, 1]\n"
    "smooth_radius_m = 320\n"
    "rain_base_mm = 800\n"
    "rain_oro_mm = 2500\n"
    "moisture_path_mm_km = 32000\n"
    "loss_max = \"0.25\"\n"
    "ocean_recharge_m = 500\n"
    "coast_rain_mm = 40\n"
    "coast_rain_reach_m = 1500\n"
    "rain_noise = { wavelength_m = 5000, octaves = 3, gain = \"0.5\" }\n"
    "rain_noise_amp = \"0.4\"\n"
    "rain_min_mm = 250\n"
    "temp_offset_max_c = 2\n";

fs::path write_data_root(const Sandbox& sb, std::string_view climate = CLIMATE_TOML) {
    const fs::path root = sb.file("data");
    fs::create_directories(root / "worldgen");
    write_text(root / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(root / "worldgen" / "hydrology.toml", HYDROLOGY_TOML);
    write_text(root / "worldgen" / "climate.toml", climate);
    write_text(root / "worldgen" / "biomes.toml", std::string(elev_api::BIOMES_TOML));  // P1-04 4.2
    write_text(root / "worldgen" / "resources.toml", std::string(elev_api::RESOURCES_TOML));  // P1-05 4.2
    write_text(root / "worldgen" / "regions.toml", std::string(elev_api::REGIONS_TOML));      // P1-06 4
    fs::create_directories(root / "biomes");
    write_text(root / "biomes" / "biomes.toml", std::string(elev_api::BIOME_DEFINITIONS_TOML));  // P1-04 4.1
    history_fixture::copy_to(root);  // P3-01: the required history kinds
    return root;
}

// The items of one case of the climate golden file (empty if absent).
std::map<std::string, std::string> golden_case(const std::string& name) {
    std::ifstream in(UNMAPPED_P1_03_GOLDEN_PATH, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open the climate golden file");
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

// The "table <name> <hex>" lines of hash --detail, without `meta` (stages
// differ) and `layer` (it gains the climate rows; the layers themselves are
// compared by their layer lines).
std::vector<std::string> table_lines(const Sandbox& sb, const fs::path& db) {
    const ProcessResult h = sb.cli({"hash", "--db", u8(db), "--detail"});
    REQUIRE(h.exit_code == EXIT_OK);
    std::vector<std::string> out;
    for (const std::string& line : lines_of(h.out)) {
        // Table climate is written by the climate stage itself (schema 3),
        // so it is not among the tables an earlier stage left unchanged.
        if (starts_with(line, "table ") && !starts_with(line, "table meta ") && !starts_with(line, "table layer ") &&
            !starts_with(line, "table climate ")) {
            out.push_back(line);
        }
    }
    return out;
}

void generate(const Sandbox& sb, const fs::path& data, const std::vector<std::string>& extra, const std::string& out) {
    std::vector<std::string> args = {"generate", "--map-size", "512", "--data", u8(data), "--out", u8(sb.file(out))};
    args.insert(args.end(), extra.begin(), extra.end());
    const ProcessResult r = sb.cli(args);
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
}

template <typename T>
std::vector<T> layer_of(const worldgen::db::WorldReader& r, const char* name) {
    auto l = r.read_layer(name);
    REQUIRE_MESSAGE(l.ok(), "cannot read layer " << std::string(name));
    const std::vector<T>* v = l.value().grid.get_if<T>();
    REQUIRE(v != nullptr);
    return *v;
}

std::string metres1(std::int64_t v) {
    const std::int64_t a = v < 0 ? -v : v;
    return (v < 0 ? "-" : "") + std::to_string(a / 10) + "." + std::to_string(a % 10);
}

std::string ratio4(std::uint64_t a, std::uint64_t b) {
    if (b == 0U) {
        return "n/a";
    }
    std::uint64_t q = (a * 10000U) / b;
    if (2U * ((a * 10000U) % b) >= b) {
        ++q;
    }
    std::string f = std::to_string(q % 10000U);
    f.insert(0, 4U - f.size(), '0');
    return std::to_string(q / 10000U) + "." + f;
}

// The four climate report lines of P1-03 9, from the world's layers.
std::vector<std::string> expected_climate_lines(const fs::path& db) {
    auto r = worldgen::db::WorldReader::open(u8(db));
    REQUIRE(r.ok());
    const std::vector<std::int16_t> e = layer_of<std::int16_t>(r.value(), "elevation");
    const std::vector<std::int16_t> t = layer_of<std::int16_t>(r.value(), "temperature");
    const std::vector<std::uint16_t> p = layer_of<std::uint16_t>(r.value(), "rainfall");
    std::vector<std::int64_t> lt, lr;
    std::uint64_t below = 0;
    for (std::size_t i = 0; i < e.size(); ++i) {
        if (e[i] > 0) {
            lt.push_back(t[i]);
            lr.push_back(p[i]);
            below += t[i] < 0 ? 1U : 0U;
        }
    }
    REQUIRE(!lt.empty());
    std::sort(lt.begin(), lt.end());
    std::sort(lr.begin(), lr.end());
    auto pct = [](const std::vector<std::int64_t>& v, std::size_t q) {
        return v[std::min(v.size() - 1, (q * v.size()) / 100)];
    };
    std::string tl = "temperature_c =", rl = "rainfall_mm =";
    for (const std::size_t q : {0U, 10U, 50U, 90U, 100U}) {
        tl += " " + metres1(pct(lt, q));
        rl += " " + std::to_string(pct(lr, q));
    }
    const std::int64_t p10 = pct(lr, 10), p90 = pct(lr, 90);
    return {tl, rl, "land_below_0c = " + ratio4(below, lt.size()),
            "rain_contrast = " + (p10 == 0 ? std::string("n/a")
                                           : ratio4(static_cast<std::uint64_t>(p90), static_cast<std::uint64_t>(p10)))};
}

}  // namespace

TEST_CASE("cli P1-00 X1, P1-03 3 and 9: generate --until climate at 512; hash, report and render") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    generate(sb, data, {"--seed", "0", "--until", "climate", "--threads", "2"}, "c.db");
    generate(sb, data, {"--seed", "0", "--until", "hydrology", "--threads", "5"}, "h.db");
    // (The default run includes climate: planned_stages in
    // stages_climate_golden_test.cpp and the slow X3 case below.)

    const auto lines = layer_lines(sb, sb.file("c.db"));
    REQUIRE(lines.size() == 10U);
    // Layers 9 and 10 are temperature and rainfall; the first eight are
    // those of the hydrology world, unchanged, and so are the tables.
    CHECK(lines[8].first == "temperature");
    CHECK(lines[9].first == "rainfall");
    const auto hyd = layer_lines(sb, sb.file("h.db"));
    REQUIRE(hyd.size() == 8U);
    for (std::size_t i = 0; i < 8; ++i) {
        CAPTURE(hyd[i].first);
        CHECK(lines[i] == hyd[i]);
    }
    CHECK(table_lines(sb, sb.file("c.db")) == table_lines(sb, sb.file("h.db")));
    // The reference's layer hashes, when the golden file holds this world.
    const std::map<std::string, std::string> g = golden_case("shipped-512-s0");
    if (!g.empty()) {
        CHECK(lines[0].second == g.at("elevation"));
        CHECK(lines[8].second == g.at("temperature"));
        CHECK(lines[9].second == g.at("rainfall"));
    } else {
        MESSAGE("climate golden case shipped-512-s0 absent: layer hashes not compared with the reference");
    }

    // report: stages, the two layer lines with their catalogue types, and
    // the climate keys after the hydrology keys, with the values of P1-03 9.
    const ProcessResult rep = sb.cli({"report", "--db", u8(sb.file("c.db"))});
    INFO("stderr: ", rep.err);
    REQUIRE(rep.exit_code == EXIT_OK);
    CHECK(contains(rep.out, "\nstages = elevation,hydrology,climate\n"));
    CHECK(contains(rep.out, "\ntemperature i16 512x512 codec=1 "));
    CHECK(contains(rep.out, "\nrainfall u16 512x512 codec=1 "));
    const std::size_t basin = rep.out.find("\nlargest_basin_share = ");
    REQUIRE(basin != std::string::npos);
    std::size_t at = basin;
    const std::vector<std::string> want = expected_climate_lines(sb.file("c.db"));
    for (const std::string& line : want) {
        CAPTURE(line);
        const std::size_t k = rep.out.find("\n" + line + "\n", at);
        CHECK(k != std::string::npos);
        if (k != std::string::npos) {
            at = k + 1;
        }
    }
    if (!g.empty()) {
        for (const char* key : {"temperature_c", "rainfall_mm", "land_below_0c", "rain_contrast"}) {
            CAPTURE(std::string_view(key));
            std::string v = g.at(key);
            std::replace(v.begin(), v.end(), '_', ' ');
            CHECK(contains(rep.out, "\n" + std::string(key) + " = " + v + "\n"));
        }
    }
    // A hydrology world has no climate lines.
    const ProcessResult rep_h = sb.cli({"report", "--db", u8(sb.file("h.db"))});
    REQUIRE(rep_h.exit_code == EXIT_OK);
    CHECK_FALSE(contains(rep_h.out, "\ntemperature_c = "));
    CHECK_FALSE(contains(rep_h.out, "\nrain_contrast = "));

    // render: each new layer under auto uses its registered palette, the same
    // as the named palette, and not gray.
    for (const char* layer : {"temperature", "rainfall"}) {
        CAPTURE(std::string_view(layer));
        const fs::path a = sb.file(std::string(layer) + ".auto.png");
        const fs::path n = sb.file(std::string(layer) + ".named.png");
        const fs::path gr = sb.file(std::string(layer) + ".gray.png");
        const ProcessResult ra = sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", layer, "--out", u8(a)});
        INFO("stderr: ", ra.err);
        REQUIRE(ra.exit_code == EXIT_OK);
        REQUIRE(sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", layer, "--palette", layer, "--out", u8(n)})
                    .exit_code == EXIT_OK);
        REQUIRE(sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", layer, "--palette", "gray", "--out", u8(gr)})
                    .exit_code == EXIT_OK);
        const Bytes pa = read_bytes(a);
        CHECK(png_header(pa).width == 512U);
        CHECK(png_header(pa).height == 512U);
        CHECK(read_bytes(n) == pa);
        CHECK(read_bytes(gr) != pa);
    }
    // P1-03 9: temperature suits i16 layers only, rainfall u16 only; naming a
    // palette that does not suit the layer is a usage error (appendix C 1).
    CHECK(sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", "water", "--palette", "temperature"}).exit_code ==
          EXIT_USAGE);
    CHECK(sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", "rainfall", "--palette", "temperature"})
              .exit_code == EXIT_USAGE);
    CHECK(sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", "temperature", "--palette", "rainfall"})
              .exit_code == EXIT_USAGE);
    CHECK(sb.cli({"render", "--db", u8(sb.file("c.db")), "--layer", "water_depth", "--palette", "rainfall", "--out",
                  u8(sb.file("wd.png"))})
              .exit_code == EXIT_OK);  // water_depth is u16
}

TEST_CASE("cli P1-00 10.1: --until climate is accepted; an unknown stage is a usage error; tuning errors are input errors") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    // Every geography stage is implemented since P1-06: an unknown name.
    const ProcessResult b = sb.cli({"generate", "--seed", "0", "--map-size", "512", "--until", "bogus", "--data",
                                    u8(data), "--out", u8(sb.file("b.db"))});
    CHECK(b.exit_code == EXIT_USAGE);
    CHECK_FALSE(file_exists(sb.file("b.db")));
    const ProcessResult tp = sb.cli({"generate", "--seed", "0", "--test-pattern", "--until", "climate", "--data",
                                     u8(data), "--out", u8(sb.file("t.db"))});
    CHECK(tp.exit_code == EXIT_USAGE);
    // A data root without climate.toml, or with the named check failing, is
    // refused before any stage runs (P1-00 6.1, P1-03 4), whatever --until.
    Sandbox sb2;
    const fs::path bad = write_data_root(sb2, "[worldgen.climate]\n");
    const ProcessResult r = sb2.cli({"generate", "--seed", "0", "--map-size", "512", "--until", "none", "--data",
                                     u8(bad), "--out", u8(sb2.file("x.db"))});
    CHECK(r.exit_code == EXIT_INPUT);
    CHECK(contains(r.err, "climate"));
    Sandbox sb3;
    std::string zero(CLIMATE_TOML);
    zero.replace(zero.find("[1, 1, 1, 1, 1, 1, 1, 1]"), 24, "[0, 0, 0, 0, 0, 0, 0, 0]");
    const fs::path z = write_data_root(sb3, zero);
    const ProcessResult rz = sb3.cli({"generate", "--seed", "0", "--map-size", "512", "--until", "none", "--data",
                                      u8(z), "--out", u8(sb3.file("z.db"))});
    CHECK(rz.exit_code == EXIT_INPUT);
    CHECK(contains(rz.err, "worldgen/climate.toml:1:1: error: check 'wind_weights_positive' failed for "
                           "'worldgen.climate': wind_weights must not all be 0"));
}

TEST_SUITE("climate-slow" * doctest::skip()) {

TEST_CASE("cli P1-00 X3 (slow): --until climate at 512, seeds 0 and 42, --threads 1, 2 and 5: byte-identical files") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    for (const char* seed : {"0", "42"}) {
        CAPTURE(std::string_view(seed));
        for (const char* threads : {"1", "2", "5"}) {
            generate(sb, data, {"--seed", seed, "--until", "climate", "--threads", threads},
                     std::string("s") + seed + "-" + threads + ".db");
        }
        const Bytes a = read_bytes(sb.file(std::string("s") + seed + "-1.db"));
        CHECK(read_bytes(sb.file(std::string("s") + seed + "-2.db")) == a);
        CHECK(read_bytes(sb.file(std::string("s") + seed + "-5.db")) == a);
    }
    // The default run includes climate: its first ten layers are those of
    // --until climate (byte-identical while climate is the last stage).
    generate(sb, data, {"--seed", "0"}, "default.db");
    const auto dflt = layer_lines(sb, sb.file("default.db"));
    const auto upto = layer_lines(sb, sb.file("s0-1.db"));
    REQUIRE(dflt.size() >= 10U);
    REQUIRE(upto.size() == 10U);
    for (std::size_t i = 0; i < 10; ++i) {
        CHECK(dflt[i] == upto[i]);
    }
    const std::map<std::string, std::string> g = golden_case("shipped-512-s42");
    if (!g.empty()) {
        const auto lines = layer_lines(sb, sb.file("s42-1.db"));
        REQUIRE(lines.size() == 10U);
        CHECK(lines[8].second == g.at("temperature"));
        CHECK(lines[9].second == g.at("rainfall"));
    }
}

}  // TEST_SUITE
