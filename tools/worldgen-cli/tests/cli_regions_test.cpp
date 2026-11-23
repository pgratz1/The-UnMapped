// worldgen-cli with the regions stage (spec P1-06 sections 3, 4, 9 and
// 10.1 tests 4, 6 to 8, 10.3; P0-02 amendment 17; P0-03 amendment 17; P0-04
// amendment 18; P1-00 10.1 to 10.3, 14 X1 and X3).
//
// Written from the specs only. The data root is written into the sandbox
// with the shipped values of P1-01 4.4 to P1-06 4 (biome_data::write_to adds
// worldgen/regions.toml once the stage registers it). Expected values come
// from the test writer's golden file (worldgen/tests/golden/p1-06-regions.txt,
// written by the Python reference) and from the layers of the world file.
// Render pixels are decoded from the PNG (png_decode.hpp).
//
// Slow checks are in the test suite "regions-slow", skipped by default:
//   worldgen-cli-tests -ts=regions-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "p1_04_biomes_data.hpp"
#include "p1_06_regions_data.hpp"
#include "png_decode.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    friend bool operator==(const Rgb&, const Rgb&) = default;
};

constexpr std::string_view HYDROLOGY_TOML =
    "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n";

fs::path write_data_root(const Sandbox& sb, const std::string& dir = "data") {
    const fs::path root = sb.file(dir);
    fs::create_directories(root / "worldgen");
    write_text(root / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(root / "worldgen" / "hydrology.toml", HYDROLOGY_TOML);
    write_text(root / "worldgen" / "climate.toml", std::string(elev_api::CLIMATE_TOML));
    biome_data::write_to(root);
    region_data::write_to(root);
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

// The "<kind> <name> <hex>" lines of hash --detail, in order.
std::vector<std::vector<std::string>> detail_lines(const Sandbox& sb, const fs::path& db) {
    const ProcessResult h = sb.cli({"hash", "--db", u8(db), "--detail"});
    INFO("stderr: ", h.err);
    REQUIRE(h.exit_code == EXIT_OK);
    std::vector<std::vector<std::string>> out;
    for (const std::string& line : lines_of(h.out)) {
        std::istringstream ss(line);
        std::string k, name, hex;
        ss >> k >> name >> hex;
        out.push_back({k, name, hex});
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

// The `ids` palette of P1-00 10.2.
Rgb ids(std::uint64_t v) {
    if (v == 0) {
        return Rgb{};
    }
    const std::uint64_t h = worldgen::mix64(v);
    auto ch = [&](unsigned j) { return static_cast<std::uint8_t>(64U + (((h >> (8U * j)) & 0xFFU) * 191U) / 255U); };
    return Rgb{ch(0), ch(1), ch(2)};
}

struct Image {
    std::uint32_t width = 0, height = 0;
    std::vector<Rgb> px;
};

Image decode_png(const fs::path& p) {
    const pngtest::Image img = pngtest::decode(read_bytes(p));
    REQUIRE(img.ok);
    Image out{img.width, img.height, std::vector<Rgb>(static_cast<std::size_t>(img.width) * img.height)};
    for (std::size_t i = 0; i < out.px.size(); ++i) {
        out.px[i] = Rgb{img.rgb[3 * i], img.rgb[3 * i + 1], img.rgb[3 * i + 2]};
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

// One generated world per test run (the Sandbox is never destroyed; its
// directory is removed at exit), as in cli_resources_test.cpp.
struct Worlds {
    Sandbox& sb;
    fs::path data, full, resources;
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
    static Worlds w{*sb, {}, {}, {}};
    if (!w.tried) {
        w.tried = true;
        w.data = write_data_root(w.sb);
        w.full = w.sb.file("seed0.db");
        generate(w.sb, w.data, {}, w.full);
        w.resources = w.sb.file("seed0-resources.db");
        generate(w.sb, w.data, {"--until", "resources"}, w.resources);
        w.ok = true;
    }
    REQUIRE_MESSAGE(w.ok, "the shared worlds of cli_regions_test could not be generated (see the first failure)");
    return w;
}

const std::vector<std::string> REPORT_KEYS = {"regions",
                                              "region_area_km2",
                                              "region_borders",
                                              "region_borders_walkable",
                                              "region_borders_river",
                                              "region_borders_pass",
                                              "regions_isolated",
                                              "regions_upland"};

}  // namespace

TEST_CASE("cli P1-06 3, P1-00 X1: the default run at 512 writes ids 1 to 19 and the golden layer and tables") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    CHECK(rd.value().stages() == std::vector<std::string>{"elevation", "hydrology", "climate", "biomes", "resources",
                                                          "regions", "chronology", "history"});  // P3-04 9.5
    const auto infos = rd.value().layers();
    REQUIRE(infos.ok());
    REQUIRE(infos.value().size() == 19U);
    CHECK(infos.value()[18].name == "region");
    CHECK(infos.value()[18].id == 19);
    const auto gold = golden_case(UNMAPPED_P1_06_GOLDEN_PATH, "shipped-512-s0");
    const auto lines = detail_lines(w.sb, w.full);
    // hash --detail: the table lines in byte order of the name, then the
    // layers in id order with region last (P0-04 amendment 18 (b)).
    std::vector<std::string> tables, layers;
    std::map<std::string, std::string> hex;
    for (const auto& l : lines) {
        if (l[0] == "table") {
            tables.push_back(l[1]);
        } else if (l[0] == "layer") {
            layers.push_back(l[1]);
        }
        hex[l[0] + " " + l[1]] = l[2];
    }
    CHECK(tables == std::vector<std::string>{"age", "biome", "civilization", "climate", "culture", "event", "event_cause",
                                             "event_kind", "event_kind_role", "event_kind_value", "event_participant",
                                             "event_value", "god", "layer", "meta", "pass", "phase", "race",
                                             "race_biome", "region", "region_biome", "region_border",
                                             "region_control", "region_name", "relation", "settlement",
                                             "settlement_census"});
    REQUIRE_FALSE(layers.empty());
    CHECK(layers.back() == "region");
    CHECK(hex["layer region"] == gold.at("region"));
    CHECK(hex["table region"] == gold.at("table_region"));
    CHECK(hex["table region_biome"] == gold.at("table_region_biome"));
    CHECK(hex["table region_border"] == gold.at("table_region_border"));
    // The thirteen inputs are those of 0.10.0 (the golden case's input items).
    for (const char* name : {"elevation", "water", "water_depth", "flow_dir", "river", "biome", "fertility", "ore",
                             "timber", "fresh_water", "forage", "habitability", "walk_area"}) {
        CAPTURE(std::string_view(name));
        CHECK(hex[std::string("layer ") + name] == gold.at(name));
    }
    // --until regions is the default run without the chronology stage
    // (P3-03 4.3): the same layers and region tables, empty chronology tables.
    const fs::path u = w.sb.file("until-regions.db");
    generate(w.sb, w.data, {"--until", "regions"}, u);
    const auto until_lines = detail_lines(w.sb, u);
    std::map<std::string, std::string> until_hex;
    for (const auto& l : until_lines) {
        until_hex[l[0] + " " + l[1]] = l[2];
    }
    for (const auto& [key, value] : hex) {
        CAPTURE(key);
        static const std::set<std::string> later = {
            "table age",          "table culture",           "table god",          "table race",
            "table race_biome",   "table civilization",      "table event",        "table event_cause",
            "table event_kind",   "table event_kind_role",   "table event_kind_value", "table event_participant",
            "table event_value",  "table phase",             "table region_control", "table relation",
            "table settlement",   "table settlement_census", "table region_name"};  // chronology (P3-03) and history (P3-04, P5-12)
        if (key.rfind("file ", 0) == 0U || key == "table meta" || later.count(key) != 0U) {
            continue;
        }
        CHECK(until_hex[key] == value);
    }
}

TEST_CASE("cli P1-06 9: report: the eight region keys close [geography], with the golden values and the spec's example") {
    Worlds& w = worlds();
    const ProcessResult r = w.sb.cli({"report", "--db", u8(w.full)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    const auto gold = golden_case(UNMAPPED_P1_06_GOLDEN_PATH, "shipped-512-s0");
    // The region keys close [geography]; [chronology] follows it (P3-03 9.1).
    std::vector<std::string> all = lines_of(r.out);
    const auto chronology = std::find(all.begin(), all.end(), std::string("[chronology]"));
    all.erase(chronology, all.end());
    INFO(r.out);
    REQUIRE(all.size() >= REPORT_KEYS.size());
    for (std::size_t k = 0; k < REPORT_KEYS.size(); ++k) {
        const std::string& line = all[all.size() - REPORT_KEYS.size() + k];
        const std::string head = REPORT_KEYS[k] + " = ";
        REQUIRE(line.rfind(head, 0) == 0U);
        std::string v = line.substr(head.size());
        std::replace(v.begin(), v.end(), ' ', '_');
        CHECK(v == gold.at(REPORT_KEYS[k]));
    }
    CHECK(contains(r.out, "\nregions = 14\nregion_area_km2 = 0.06 1.69 3.50\nregion_borders = 18\n"
                          "region_borders_walkable = 9\nregion_borders_river = 0\nregion_borders_pass = 0\n"
                          "regions_isolated = 3\nregions_upland = 6\n"));
    // After the resources keys (P1-00 10.3).
    CHECK(r.out.find("\nwalk_share = ") < r.out.find("\nregions = "));
    // [tables] (P0-04 amendment 18 (a)).
    CHECK(contains(r.out, "\npass = 0\nphase = "));
    CHECK(contains(r.out, "\nrace = 6\nrace_biome = 48\nregion = 14\nregion_biome = "));
    CHECK(contains(r.out, "\nregion_border = 18\n"));
    CHECK(contains(r.out, "\nschema_version = 8\n"));
}

TEST_CASE("cli P1-06 10.3: --until resources leaves the three tables empty and no region keys") {
    Worlds& w = worlds();
    const ProcessResult r = w.sb.cli({"report", "--db", u8(w.resources)});
    REQUIRE(r.exit_code == EXIT_OK);
    CHECK(contains(r.out, "\nregion = 0\nregion_biome = 0\nregion_border = 0\n"));
    CHECK_FALSE(contains(r.out, "\nregions = "));
    // --until none: every table empty, 35 pages (143360 bytes) since schema 8 (P5-12 12).
    const fs::path e = w.sb.file("none.db");
    generate(w.sb, w.data, {"--until", "none"}, e);
    CHECK(read_bytes(e).size() == 176128U);   // P5-02 11.1 (r1): 43 pages
    const ProcessResult re = w.sb.cli({"report", "--db", u8(e)});
    REQUIRE(re.exit_code == EXIT_OK);
    CHECK(contains(re.out, "\n[tables]\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 0\nmeta = 17\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\n"
                           "region_biome = 0\nregion_border = 0\n"));
}

TEST_CASE("cli P1-06 9, 10.1 test 8: render --layer region uses ids; --palette ids gives the same file") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    const std::vector<std::uint16_t> v = layer_of<std::uint16_t>(rd.value(), "region");
    const fs::path a = w.sb.file("region.auto.png");
    const fs::path n = w.sb.file("region.ids.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", "region", "--out", u8(a)}).exit_code == EXIT_OK);
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", "region", "--palette", "ids", "--out", u8(n)})
                .exit_code == EXIT_OK);
    std::vector<Rgb> want(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        want[i] = ids(v[i]);
    }
    check_pixels(decode_png(a).px, want);
    CHECK(read_bytes(a) == read_bytes(n));
}

TEST_CASE("cli P0-04 amendment 18 (e), 10.1 test 8: --composite regions is --composite biomes with region borders black") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    const std::vector<std::uint16_t> reg = layer_of<std::uint16_t>(rd.value(), "region");
    const fs::path b = w.sb.file("b.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--composite", "biomes", "--out", u8(b)}).exit_code == EXIT_OK);
    // Given explicitly here; the default output name is checked below.
    const fs::path r = w.sb.file("r.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--composite", "regions", "--out", u8(r)}).exit_code == EXIT_OK);
    const Image bi = decode_png(b);
    const Image ri = decode_png(r);
    REQUIRE(ri.width == 512U);
    REQUIRE(ri.height == 512U);
    std::vector<Rgb> want = bi.px;
    std::size_t blacks = 0;
    for (std::uint32_t y = 0; y < 512; ++y) {
        for (std::uint32_t x = 0; x < 512; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * 512 + x;
            const std::uint16_t v = reg[i];
            if (v == 0) {
                continue;
            }
            const bool e = x + 1 < 512 && reg[i + 1] != 0 && reg[i + 1] != v;
            const bool s = y + 1 < 512 && reg[i + 512] != 0 && reg[i + 512] != v;
            if (e || s) {
                want[i] = Rgb{};
                ++blacks;
            }
        }
    }
    CHECK(blacks > 100U);
    check_pixels(ri.px, want);
    // --scale 2 fills each cell's block.
    const fs::path r2 = w.sb.file("r2.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--composite", "regions", "--scale", "2", "--out", u8(r2)})
                .exit_code == EXIT_OK);
    const Image s2 = decode_png(r2);
    REQUIRE(s2.width == 1024U);
    for (std::uint32_t y = 0; y < 1024; y += 37) {
        for (std::uint32_t x = 0; x < 1024; x += 41) {
            if (!(s2.px[static_cast<std::size_t>(y) * 1024 + x] == want[static_cast<std::size_t>(y / 2) * 512 + x / 2])) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(false);
                y = 1024;
                break;
            }
        }
    }
    // Default output <root>/out/<stem>.regions.png.
    const ProcessResult d = w.sb.cli({"render", "--db", u8(w.full), "--composite", "regions"});
    INFO("stderr: ", d.err);
    REQUIRE(d.exit_code == EXIT_OK);
    const fs::path def = w.sb.out_dir() / "seed0.regions.png";
    CHECK(d.out == printed_path(def));
    CHECK(read_bytes(def) == read_bytes(r));
}

TEST_CASE("cli P0-04 amendment 18 (e), 10.1 test 8: --composite regions without a region layer exits 3, no file; help") {
    Worlds& w = worlds();
    const fs::path out = w.sb.file("none.regions.png");
    const ProcessResult r = w.sb.cli({"render", "--db", u8(w.resources), "--composite", "regions", "--out", u8(out)});
    CHECK(r.exit_code == EXIT_INPUT);
    CHECK(contains(r.err, "has no layer 'region'; available layers: "));
    CHECK(r.out.empty());
    CHECK_FALSE(file_exists(out));
    const ProcessResult l = w.sb.cli({"render", "--db", u8(w.resources), "--layer", "region", "--out", u8(out)});
    CHECK(l.exit_code == EXIT_INPUT);
    CHECK_FALSE(file_exists(out));
    const ProcessResult h = w.sb.cli({"render", "--help"});
    REQUIRE(h.exit_code == EXIT_OK);
    CHECK(contains(h.out, "regions"));
    CHECK(contains(h.out, "biomes"));
    CHECK(contains(h.out, "map"));
}

TEST_CASE("cli P0-04 amendment 18 (e): --composite regions refuses a region layer not u16 or not elevation's size") {
    Sandbox sb;
    using worldgen::stages::testing::FixtureLayer;
    auto fixture = [&](const char* name, std::vector<FixtureLayer> layers) {
        const fs::path p = sb.file(name);
        REQUIRE(worldgen::stages::testing::write_fixture_world(u8(p), 3, layers).ok());
        return p;
    };
    const auto e = worldgen::db::LayerGrid{4, 3, std::vector<std::int16_t>(12, 50)};
    const auto b = worldgen::db::LayerGrid{4, 3, std::vector<std::uint8_t>(12, 3)};
    const auto r8 = worldgen::db::LayerGrid{4, 3, std::vector<std::uint8_t>(12, 1)};
    const auto rs = worldgen::db::LayerGrid{4, 2, std::vector<std::uint16_t>(8, 1)};
    const auto ok = worldgen::db::LayerGrid{4, 3, std::vector<std::uint16_t>{1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 3, 3}};
    const auto c = worldgen::db::Codec::delta_varint;
    const fs::path a = fixture("r8.db", {{"elevation", e, c}, {"biome", b, c}, {"region", r8, c}});
    const fs::path s = fixture("rs.db", {{"elevation", e, c}, {"biome", b, c}, {"region", rs, c}});
    const fs::path g = fixture("ok.db", {{"elevation", e, c}, {"biome", b, c}, {"region", ok, c}});
    const fs::path out = sb.file("x.png");
    const ProcessResult r1 = sb.cli({"render", "--db", u8(a), "--composite", "regions", "--out", u8(out)});
    CHECK(r1.exit_code == EXIT_INPUT);
    CHECK(r1.err == "worldgen-cli: error: cannot render the composite regions: layer 'region' is u8, expected u16; "
                    "available layers: elevation, biome, region\n");
    CHECK_FALSE(file_exists(out));
    const ProcessResult r2 = sb.cli({"render", "--db", u8(s), "--composite", "regions", "--out", u8(out)});
    CHECK(r2.exit_code == EXIT_INPUT);
    CHECK(r2.err == "worldgen-cli: error: cannot render the composite regions: layer 'region' is not the size of layer "
                    "'elevation'; available layers: elevation, biome, region\n");
    CHECK_FALSE(file_exists(out));
    // Without water: biome colours shaded, black on (1, 0), (1, 1) and the
    // west/north cells of the 2|3 and 1|3 pairs: (0..3, 1).
    const ProcessResult r3 = sb.cli({"render", "--db", u8(g), "--composite", "regions", "--out", u8(out)});
    INFO("stderr: ", r3.err);
    REQUIRE(r3.exit_code == EXIT_OK);
    const Image im = decode_png(out);
    REQUIRE(im.px.size() == 12U);
    for (std::size_t i = 0; i < 12; ++i) {
        CAPTURE(i);
        const bool black = i == 1 || (i >= 4 && i <= 7);
        CHECK((im.px[i] == Rgb{}) == black);
    }
}

TEST_CASE("cli P0-02 amendment 17, 10.1 test 6: a version 4 world is refused by report, hash --detail and render") {
    Worlds& w = worlds();
    const fs::path v4 = w.sb.file("v4.db");
    fs::copy_file(w.resources, v4);
    REQUIRE(worldgen::db::testing::execute_on_file(u8(v4), "PRAGMA user_version = 4").ok());
    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"report", "--db", u8(v4)}, std::vector<std::string>{"hash", "--db", u8(v4), "--detail"},
          std::vector<std::string>{"render", "--db", u8(v4), "--layer", "elevation", "--out", u8(w.sb.file("v4.png"))}}) {
        CAPTURE(args[0]);
        const ProcessResult r = w.sb.cli(args);
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(contains(r.err, "world schema version 4, this build reads 8"));
        CHECK(r.out.empty());
    }
    // Plain `hash` hashes the bytes without opening the world (P0-04 8).
    CHECK(w.sb.cli({"hash", "--db", u8(v4)}).exit_code == EXIT_OK);
}

TEST_CASE("cli P1-06 4, P0-03 amendment 17: bad regions tuning is an input error with the loader's message") {
    Sandbox sb;
    struct Case {
        std::vector<std::pair<std::string, std::string>> overrides;
        std::string message;
    };
    const std::vector<Case> cases = {
        {{{"names", region_data::names_toml("east", "east")}},
         "is \"east\"; expected a string of at most 32 bytes, a word ([A-Z][a-z]*)"},
        {{{"names", region_data::names_toml("north", "North West")}},
         "is \"North West\"; expected a string of at most 32 bytes, a word ([A-Z][a-z]*)"},
        {{{"seed_tile_m", "2401"}},
         "worldgen/regions.toml:1:1: error: check 'regions_tile_fits_spacing' failed for 'worldgen.regions': "
         "seed_tile_m must be at most twice seed_spacing_m"},
    };
    int k = 0;
    for (const Case& c : cases) {
        CAPTURE(c.message);
        const fs::path data = write_data_root(sb, "data" + std::to_string(k));
        write_text(data / "worldgen" / "regions.toml", region_data::tuning_toml(c.overrides));
        const fs::path out = sb.file("t" + std::to_string(k++) + ".db");
        const ProcessResult r = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(data), "--until",
                                        "none", "--out", u8(out)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(contains(r.err, c.message));
        CHECK_FALSE(file_exists(out));
    }
    // A data root without worldgen/regions.toml.
    const fs::path bare = write_data_root(sb, "bare");
    fs::remove(bare / "worldgen" / "regions.toml");
    const ProcessResult m = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(bare), "--until",
                                    "none", "--out", u8(sb.file("m.db"))});
    CHECK(m.exit_code == EXIT_INPUT);
    CHECK(contains(m.err, "worldgen.regions"));
}

TEST_CASE("cli P1-00 10.1: --until regions is accepted; a stage after it does not exist") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    // Every stage of STAGE_NAMES is implemented since P3-04 (history); an
    // unknown name stands for a stage that does not exist.
    const ProcessResult r = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(data), "--until",
                                    "chronicle", "--out", u8(sb.file("h.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK_FALSE(file_exists(sb.file("h.db")));
}

TEST_SUITE("regions-slow" * doctest::skip()) {

TEST_CASE("cli P1-00 X3 (slow): 512, seeds 0 and 42, --threads 1, 2 and 5: byte-identical files; golden at 42") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    for (const char* seed : {"0", "42"}) {
        std::vector<Bytes> files;
        for (const char* threads : {"1", "2", "5"}) {
            const fs::path out = sb.file(std::string("s") + seed + "-t" + threads + ".db");
            generate(sb, data, {}, out, seed, threads);
            files.push_back(read_bytes(out));
        }
        CHECK(files[0] == files[1]);
        CHECK(files[0] == files[2]);
    }
    const auto gold = golden_case(UNMAPPED_P1_06_GOLDEN_PATH, "shipped-512-s42");
    std::map<std::string, std::string> hex;
    for (const auto& l : detail_lines(sb, sb.file("s42-t1.db"))) {
        hex[l[0] + " " + l[1]] = l[2];
    }
    CHECK(hex["layer region"] == gold.at("region"));
    CHECK(hex["table region"] == gold.at("table_region"));
    CHECK(hex["table region_border"] == gold.at("table_region_border"));
    CHECK(hex["table region_biome"] == gold.at("table_region_biome"));
}

TEST_CASE("cli P1-06 10.4 (slow): the phrase world at 512 and 1024 seed 1: golden layer and tables") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    struct Run {
        std::vector<std::string> args;
        const char* name;
    };
    const std::vector<Run> runs = {
        {{"--seed-phrase", "The Unmapped", "--map-size", "512"}, "shipped-512-phrase"},
        {{"--seed", "1", "--map-size", "1024"}, "shipped-1024-s1"},
    };
    for (const Run& run : runs) {
        CAPTURE(std::string_view(run.name));
        const fs::path out = sb.file(std::string(run.name) + ".db");
        std::vector<std::string> args = {"generate", "--data", u8(data), "--threads", "4", "--out", u8(out)};
        args.insert(args.end(), run.args.begin(), run.args.end());
        const ProcessResult r = sb.cli(args);
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        const auto gold = golden_case(UNMAPPED_P1_06_GOLDEN_PATH, run.name);
        std::map<std::string, std::string> hex;
        for (const auto& l : detail_lines(sb, out)) {
            hex[l[0] + " " + l[1]] = l[2];
        }
        CHECK(hex["layer region"] == gold.at("region"));
        CHECK(hex["table region"] == gold.at("table_region"));
        CHECK(hex["table region_border"] == gold.at("table_region_border"));
        CHECK(hex["table region_biome"] == gold.at("table_region_biome"));
        const ProcessResult rep = sb.cli({"report", "--db", u8(out)});
        REQUIRE(rep.exit_code == EXIT_OK);
        CHECK(contains(rep.out, "\nregions = " + gold.at("regions") + "\n"));
    }
}

}  // TEST_SUITE
