// worldgen-cli with the resources stage (spec P1-05 sections 3, 9 and
// 10.1 tests 7 and 8, 10.3; P1-00 10.1 to 10.3, 14 X1 and X3).
//
// Written from the specs only. The data root is written into the sandbox
// with the shipped values of P1-01 4.4 to P1-05 4.2 (biome_data::write_to
// adds worldgen/resources.toml and the biome field `forage` once the stage
// registers them). Expected values come from the test writer's golden files
// (worldgen/tests/golden/p1-05-resources.txt and p1-04-biomes.txt, case
// shipped-512-s0, written by the Python references) and from the layers of
// the world file. Render pixels are decoded from the PNG (png_decode.hpp)
// and compared with the `score` palette of P1-05 9 recomputed here.
//
// Slow checks are in the test suite "resources-slow", skipped by default:
//   worldgen-cli-tests -ts=resources-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
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
#include "p1_04_biomes_data.hpp"
#include "png_decode.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    friend bool operator==(const Rgb&, const Rgb&) = default;
};

constexpr std::string_view HYDROLOGY_TOML =
    "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n";

fs::path write_data_root(const Sandbox& sb) {
    const fs::path root = sb.file("data");
    fs::create_directories(root / "worldgen");
    write_text(root / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(root / "worldgen" / "hydrology.toml", HYDROLOGY_TOML);
    write_text(root / "worldgen" / "climate.toml", std::string(elev_api::CLIMATE_TOML));
    biome_data::write_to(root);
    return root;
}

std::map<std::string, std::string> golden_case(const char* path, const std::string& name) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path);
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
    REQUIRE_MESSAGE(!out.empty(), "no golden case " << name << " in " << path);
    return out;
}

void generate(const Sandbox& sb, const fs::path& data, const std::vector<std::string>& extra, const fs::path& out,
              const char* seed = "0", const char* threads = "4") {
    std::vector<std::string> args = {"generate", "--seed", seed,     "--map-size", "512",   "--data",
                                     u8(data),   "--threads", threads, "--out",      u8(out)};
    args.insert(args.end(), extra.begin(), extra.end());
    const ProcessResult r = sb.cli(args);
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
}

// The "<kind> <name> <hex>" lines of hash --detail for kind layer.
std::map<std::string, std::string> layer_hashes(const Sandbox& sb, const fs::path& db) {
    const ProcessResult h = sb.cli({"hash", "--db", u8(db), "--detail"});
    INFO("stderr: ", h.err);
    REQUIRE(h.exit_code == EXIT_OK);
    std::map<std::string, std::string> out;
    for (const std::string& line : lines_of(h.out)) {
        std::istringstream ss(line);
        std::string k, name, hex;
        ss >> k >> name >> hex;
        if (k == "layer") {
            out[name] = hex;
        }
    }
    return out;
}

template <typename T>
std::vector<T> layer_of(const worldgen::db::WorldReader& r, const char* name) {
    auto l = r.read_layer(name);
    REQUIRE_MESSAGE(l.ok(), "cannot read layer " << std::string(name));
    const std::vector<T>* v = l.value().grid.get_if<T>();
    REQUIRE(v != nullptr);
    return *v;
}

// P1-05 9 `score` by the interpolation rule of P1-00 10.2 (round half up,
// floor division).
Rgb score(std::int64_t t) {
    static const std::array<std::pair<std::int64_t, Rgb>, 5> stops = {
        {{0, {68, 1, 84}}, {64, {59, 82, 139}}, {128, {33, 145, 140}}, {192, {94, 201, 98}}, {255, {253, 231, 37}}}};
    if (t >= stops.back().first) {
        return stops.back().second;
    }
    for (std::size_t k = 0; k + 1 < stops.size(); ++k) {
        const auto [t0, c0] = stops[k];
        const auto [t1, c1] = stops[k + 1];
        if (t0 <= t && t < t1) {
            auto ch = [&](std::uint8_t a, std::uint8_t b) {
                const std::int64_t num = 2 * (std::int64_t{b} - a) * (t - t0) + (t1 - t0);
                const std::int64_t den = 2 * (t1 - t0);
                const std::int64_t q = num >= 0 ? num / den : -((-num + den - 1) / den);
                return static_cast<std::uint8_t>(a + q);
            };
            return Rgb{ch(c0.r, c1.r), ch(c0.g, c1.g), ch(c0.b, c1.b)};
        }
    }
    return stops.front().second;
}

std::vector<Rgb> decode_png(const fs::path& p) {
    const pngtest::Image img = pngtest::decode(read_bytes(p));
    REQUIRE(img.ok);
    std::vector<Rgb> out(static_cast<std::size_t>(img.width) * img.height);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = Rgb{img.rgb[3 * i], img.rgb[3 * i + 1], img.rgb[3 * i + 2]};
    }
    return out;
}

void check_pixels(const std::vector<Rgb>& got, const std::vector<Rgb>& want) {
    REQUIRE(got.size() == want.size());
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (!(got[i] == want[i])) {
            CAPTURE(i);
            CHECK(int{got[i].r} == int{want[i].r});
            CHECK(int{got[i].g} == int{want[i].g});
            CHECK(int{got[i].b} == int{want[i].b});
            return;
        }
    }
}

// One generated world per test run, as in cli_biomes_test.cpp (the Sandbox
// is never destroyed; its directory is removed at exit).
struct Worlds {
    Sandbox& sb;
    fs::path data, full;
    bool tried = false;
    bool ok = false;
};

struct DirRemover {
    fs::path dir;
    ~DirRemover() {
        std::error_code ec;
        if (!dir.empty()) {
            fs::remove_all(dir, ec);
        }
    }
};

Worlds& worlds() {
    static Sandbox* const sb = new Sandbox;  // NOLINT: deliberately never destroyed
    static DirRemover remover{sb->root().parent_path()};
    static Worlds w{*sb, {}, {}};
    if (!w.tried) {
        w.tried = true;
        w.data = write_data_root(w.sb);
        w.full = w.sb.file("seed0.db");
        generate(w.sb, w.data, {"--until", "resources"}, w.full);
        w.ok = true;
    }
    REQUIRE_MESSAGE(w.ok, "the shared world of cli_resources_test could not be generated (see the first failure)");
    return w;
}

const std::vector<std::string> OUTPUTS = {"fertility", "ore",          "timber",   "fresh_water",
                                          "forage",    "habitability", "walk_area"};

}  // namespace

TEST_CASE("cli P1-05 3, P1-00 X1: --until resources at 512: layers 12 to 18; golden hashes; earlier layers as at 0.9.0") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    CHECK(rd.value().stages() == std::vector<std::string>{"elevation", "hydrology", "climate", "biomes", "resources"});
    const auto infos = rd.value().layers();
    REQUIRE(infos.ok());
    REQUIRE(infos.value().size() == 18U);
    for (std::size_t k = 0; k < OUTPUTS.size(); ++k) {
        CHECK(infos.value()[11 + k].name == OUTPUTS[k]);
        CHECK(infos.value()[11 + k].id == static_cast<std::int64_t>(12 + k));
    }
    const auto res = golden_case(UNMAPPED_P1_05_GOLDEN_PATH, "shipped-512-s0");
    const auto bio = golden_case(UNMAPPED_P1_04_GOLDEN_PATH, "shipped-512-s0");
    const auto hashes = layer_hashes(w.sb, w.full);
    for (const std::string& name : OUTPUTS) {
        CAPTURE(name);
        CHECK(hashes.at(name) == res.at(name));
    }
    // The layers before the stage are those of 0.9.0 (p1-04-biomes.txt).
    for (const char* name : {"elevation", "water", "water_depth", "flow_dir", "river", "coast_dist", "temperature",
                             "rainfall", "biome"}) {
        CAPTURE(name);
        CHECK(hashes.at(name) == bio.at(name));
    }
    // The default run continues with regions (P1-06, generator 0.11.0): its
    // layers 1 to 18 are those of --until resources, and `region` follows.
    const fs::path d = w.sb.file("default.db");
    generate(w.sb, w.data, {}, d);
    const auto dh = layer_hashes(w.sb, d);
    for (const auto& [name, hash] : hashes) {
        CAPTURE(name);
        CHECK(dh.at(name) == hash);
    }
    CHECK(dh.size() == hashes.size() + 1U);
    CHECK(dh.count("region") == 1U);
}

TEST_CASE("cli P1-05 9: report: the eight resources keys close [geography], with the golden values") {
    Worlds& w = worlds();
    const ProcessResult r = w.sb.cli({"report", "--db", u8(w.full)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    const auto res = golden_case(UNMAPPED_P1_05_GOLDEN_PATH, "shipped-512-s0");
    // The last eight lines, with spaces in the values written as "_" (the
    // golden file's encoding; names such as fresh_water hold "_" already).
    const std::vector<std::string> keys = {"scores_p50",     "scores_p90", "fertile_share",   "habitable_share",
                                           "ore_rich_share", "walk_areas", "walk_main_share", "walk_share"};
    const std::vector<std::string> all = lines_of(r.out);
    INFO(r.out);
    REQUIRE(all.size() >= keys.size());
    for (std::size_t k = 0; k < keys.size(); ++k) {
        const std::string& line = all[all.size() - keys.size() + k];
        const std::string head = keys[k] + " = ";
        REQUIRE(line.rfind(head, 0) == 0U);
        std::string v = line.substr(head.size());
        std::replace(v.begin(), v.end(), ' ', '_');
        CHECK(v == res.at(keys[k]));
    }
    // Right after the biome keys (P1-00 10.3).
    CHECK(contains(r.out, "\nbiome_patches = "));
    CHECK(r.out.find("\nbiome_patches = ") < r.out.find("\nscores_p50 = "));
    // The spec's example for this world (P1-05 9).
    CHECK(contains(r.out, "\nscores_p50 = fertility:0 ore:2 timber:38 fresh_water:102 forage:128 habitability:0\n"));
    CHECK(contains(r.out, "\nwalk_areas = 4\nwalk_main_share = 0.1826\nwalk_share = 0.4286\n"));
}

TEST_CASE("cli P1-05 9, 10.1 test 8: render --layer of each score layer uses `score`; walk_area uses ids") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    for (const char* name : {"fertility", "ore", "timber", "fresh_water", "forage", "habitability"}) {
        CAPTURE(name);
        const std::vector<std::uint8_t> v = layer_of<std::uint8_t>(rd.value(), name);
        const fs::path a = w.sb.file(std::string(name) + ".auto.png");
        const fs::path n = w.sb.file(std::string(name) + ".named.png");
        REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", name, "--out", u8(a)}).exit_code == EXIT_OK);
        REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", name, "--palette", "score", "--out", u8(n)})
                    .exit_code == EXIT_OK);
        std::vector<Rgb> want(v.size());
        for (std::size_t i = 0; i < v.size(); ++i) {
            want[i] = score(v[i]);
        }
        check_pixels(decode_png(a), want);
        CHECK(read_bytes(a) == read_bytes(n));
    }
    const fs::path wa = w.sb.file("walk.auto.png");
    const fs::path wi = w.sb.file("walk.ids.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", "walk_area", "--out", u8(wa)}).exit_code == EXIT_OK);
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", "walk_area", "--palette", "ids", "--out", u8(wi)})
                .exit_code == EXIT_OK);
    CHECK(read_bytes(wa) == read_bytes(wi));
    // The palette example of 9 and the stops.
    CHECK(score(100) == Rgb{44, 117, 140});
    CHECK(score(0) == Rgb{68, 1, 84});
    CHECK(score(255) == Rgb{253, 231, 37});
}

TEST_CASE("cli P1-05 9, 10.1 test 8: --palette score on a layer that is not u8 exits 2; render --help lists score") {
    Worlds& w = worlds();
    for (const char* layer : {"elevation", "walk_area", "rainfall"}) {
        CAPTURE(layer);
        const ProcessResult r = w.sb.cli({"render", "--db", u8(w.full), "--layer", layer, "--palette", "score", "--out",
                                          u8(w.sb.file(std::string("bad-") + layer + ".png"))});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(contains(r.err, "score"));
        CHECK(contains(r.err, "u8"));
        CHECK(r.out.empty());
        CHECK_FALSE(file_exists(w.sb.file(std::string("bad-") + layer + ".png")));
    }
    const ProcessResult h = w.sb.cli({"render", "--help"});
    REQUIRE(h.exit_code == EXIT_OK);
    CHECK(contains(h.out, "score"));
}

TEST_CASE("cli P1-05 9, 10.1 test 7: report refuses a score layer not u8 and walk_area not u16: exit 3, nothing on stdout") {
    Sandbox sb;
    using worldgen::db::Codec;
    struct Case {
        const char* layer;
        worldgen::db::LayerGrid grid;
        std::string message;
    };
    const std::uint32_t n = 8;
    const std::vector<Case> cases = {
        {"fertility", worldgen::db::LayerGrid{n, n, std::vector<std::int16_t>(n * n, 1)},
         "worldgen-cli: error: corrupt layer 'fertility': type i16, expected u8"},
        {"walk_area", worldgen::db::LayerGrid{n, n, std::vector<std::uint8_t>(n * n, 1)},
         "worldgen-cli: error: corrupt layer 'walk_area': type u8, expected u16"},
        {"habitability", worldgen::db::LayerGrid{4, n, std::vector<std::uint8_t>(4 * n, 1)},
         "worldgen-cli: error: corrupt layer 'habitability': size 4x8, expected 8x8"},
    };
    int k = 0;
    for (const Case& c : cases) {
        CAPTURE(c.layer);
        std::vector<worldgen::stages::testing::FixtureLayer> layers;
        layers.push_back({"elevation", worldgen::db::LayerGrid{n, n, std::vector<std::int16_t>(n * n, 50)}, Codec::delta_varint});
        layers.push_back({"water", worldgen::db::LayerGrid{n, n, std::vector<std::uint8_t>(n * n, 0)}, Codec::delta_varint});
        for (const std::string& name : OUTPUTS) {
            if (name == c.layer) {
                layers.push_back({name, c.grid, Codec::delta_varint});
            } else if (name == "walk_area") {
                layers.push_back({name, worldgen::db::LayerGrid{n, n, std::vector<std::uint16_t>(n * n, 1)}, Codec::delta_varint});
            } else {
                layers.push_back({name, worldgen::db::LayerGrid{n, n, std::vector<std::uint8_t>(n * n, 9)}, Codec::delta_varint});
            }
        }
        const fs::path db = sb.file("c" + std::to_string(k++) + ".db");
        REQUIRE(worldgen::stages::testing::write_fixture_world(u8(db), 1, layers).ok());
        const ProcessResult r = sb.cli({"report", "--db", u8(db)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(contains(r.err, c.message));
    }
}

TEST_CASE("cli P1-00 10.1: --until resources is accepted; an unknown stage is a usage error") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    // Every geography stage is implemented since P1-06: an unknown name.
    const ProcessResult r = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(data), "--until",
                                    "bogus", "--out", u8(sb.file("r.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK_FALSE(file_exists(sb.file("r.db")));
    // A data root without worldgen/resources.toml is an input error (exit 3).
    const fs::path bare = sb.file("bare");
    fs::create_directories(bare / "worldgen");
    write_text(bare / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(bare / "worldgen" / "hydrology.toml", HYDROLOGY_TOML);
    write_text(bare / "worldgen" / "climate.toml", std::string(elev_api::CLIMATE_TOML));
    write_text(bare / "worldgen" / "biomes.toml", biome_data::TUNING_TOML);
    write_text(bare / "worldgen" / "regions.toml", biome_data::REGIONS_TOML);  // P1-06 4
    fs::create_directories(bare / "biomes");
    for (const auto& f : biome_data::shipped_files()) {
        write_text(bare / "biomes" / std::string(f.name), f.text);
    }
    history_fixture::copy_to(bare);  // P3-01: so that only worldgen/resources.toml is missing
    const ProcessResult m = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(bare), "--until",
                                    "none", "--out", u8(sb.file("m.db"))});
    CHECK(m.exit_code == EXIT_INPUT);
    CHECK(contains(m.err, "worldgen.resources"));
}

TEST_SUITE("resources-slow" * doctest::skip()) {

TEST_CASE("cli P1-00 X3 (slow): --until resources at 512, seeds 0 and 42, --threads 1, 2 and 5: byte-identical files") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    for (const char* seed : {"0", "42"}) {
        std::vector<Bytes> files;
        for (const char* threads : {"1", "2", "5"}) {
            const fs::path out = sb.file(std::string("s") + seed + "-t" + threads + ".db");
            generate(sb, data, {"--until", "resources"}, out, seed, threads);
            files.push_back(read_bytes(out));
        }
        CHECK(files[0] == files[1]);
        CHECK(files[0] == files[2]);
    }
}

}  // TEST_SUITE
