// worldgen-cli with the biome stage and schema 3 (spec P1-04 sections 3 and
// 9; P1-03 3, C7 and 9 for table `climate`; P1-00 10.1 to 10.3).
//
// Written from the specs only. The data root is written into the sandbox
// with the shipped values of P1-01 4.4, P1-02 4, P1-03 4 and P1-04 4.1 and
// 4.2. Expected values come from the test writer's golden files
// (worldgen/tests/golden/p1-04-biomes.txt and p1-03-climate.txt, case
// shipped-512-s0, written by the Python references) and from the layers and
// rows of the world file. Render pixels are decoded from the PNG
// (png_decode.hpp) and compared with the palette and composite of P1-04 9
// recomputed here.
//
// Slow checks are in the test suite "biomes-slow", skipped by default:
//   worldgen-cli-tests -ts=biomes-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <ostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "p1_00_composite_reference.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "p1_04_biomes_data.hpp"
#include "png_decode.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;
using comp_ref::Rgb;

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
    return out;
}

void generate(const Sandbox& sb, const fs::path& data, const std::vector<std::string>& extra, const fs::path& out) {
    std::vector<std::string> args = {"generate", "--seed",    "0",    "--map-size", "512",
                                     "--data",   u8(data),    "--threads", "4", "--out", u8(out)};
    args.insert(args.end(), extra.begin(), extra.end());
    const ProcessResult r = sb.cli(args);
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
}

// The "<kind> <name> <hex>" lines of hash --detail for kind layer or table.
std::map<std::string, std::string> detail(const Sandbox& sb, const fs::path& db, std::string_view kind) {
    const ProcessResult h = sb.cli({"hash", "--db", u8(db), "--detail"});
    INFO("stderr: ", h.err);
    REQUIRE(h.exit_code == EXIT_OK);
    std::map<std::string, std::string> out;
    for (const std::string& line : lines_of(h.out)) {
        std::istringstream ss(line);
        std::string k, name, hex;
        ss >> k >> name >> hex;
        if (k == kind) {
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

// The shipped colours by code (P1-00 6.6).
const std::map<std::uint8_t, std::uint32_t>& colours() {
    static const std::map<std::uint8_t, std::uint32_t> c = {{1, 0x183878}, {2, 0x4080CC}, {3, 0x2E6034},
                                                            {4, 0x96AA5A}, {5, 0x6AA396}, {6, 0x7E6680},
                                                            {7, 0xBEBEB4}, {8, 0x605446}, {9, 0x94664A},
                                                            {10, 0xE0D296}, {11, 0x23505E}, {12, 0x8A8070}};
    return c;
}

Rgb rgb_of(std::uint32_t c) {
    return Rgb{static_cast<std::uint8_t>(c >> 16), static_cast<std::uint8_t>((c >> 8) & 0xFFU),
               static_cast<std::uint8_t>(c & 0xFFU)};
}

// The biome palette of P1-04 9: the row's colour, (255, 0, 255) without a row.
Rgb palette(std::uint8_t v, const std::map<std::uint8_t, std::uint32_t>& rows) {
    const auto it = rows.find(v);
    return it == rows.end() ? Rgb{255, 0, 255} : rgb_of(it->second);
}

// P1-00 10.2 interpolation rule (round half up, floor division).
Rgb ramp(const std::vector<std::pair<std::int64_t, Rgb>>& stops, std::int64_t t) {
    if (t < stops.front().first) {
        return stops.front().second;
    }
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
    return stops.back().second;
}

Rgb ocean_colour(std::int64_t depth) {
    static const std::vector<std::pair<std::int64_t, Rgb>> s = {
        {0, {120, 170, 205}}, {200, {88, 140, 190}}, {1000, {56, 104, 168}}, {5000, {32, 66, 130}}, {30000, {16, 34, 82}}};
    return ramp(s, depth);
}

Rgb land_colour(std::int64_t e) {
    static const std::vector<std::pair<std::int64_t, Rgb>> s = {
        {0, {70, 120, 64}},      {500, {104, 146, 78}},   {2000, {150, 168, 96}},   {5000, {178, 160, 108}},
        {10000, {150, 120, 86}}, {15000, {128, 106, 92}}, {20000, {196, 192, 188}}, {30000, {250, 250, 250}}};
    return ramp(s, e);
}

// Steps 1 and 2 of the composite map with the land colour given per cell,
// then steps 3 and 4 (comp_ref::overlay, the test writer's river drawing).
std::vector<Rgb> composite(std::uint32_t w, std::uint32_t h, const std::vector<std::int16_t>& e,
                           const std::vector<Rgb>& land, const std::vector<std::uint8_t>& water,
                           const std::vector<std::uint8_t>& river, const std::vector<std::uint8_t>& flow_dir) {
    auto ep = [&](std::int64_t x, std::int64_t y) {
        x = std::clamp<std::int64_t>(x, 0, w - 1);
        y = std::clamp<std::int64_t>(y, 0, h - 1);
        return std::max<std::int64_t>(e[static_cast<std::size_t>(y * w + x)], 0);
    };
    std::vector<Rgb> base(e.size());
    for (std::int64_t y = 0; y < h; ++y) {
        for (std::int64_t x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w + x);
            if (e[i] < 0) {
                base[i] = ocean_colour(-std::int64_t{e[i]});
                continue;
            }
            const std::int64_t s = (ep(x + 1, y) - ep(x - 1, y)) + (ep(x, y + 1) - ep(x, y - 1));
            const std::int64_t half = s >= 0 ? s / 2 : -((-s + 1) / 2);
            const std::int64_t f = std::clamp<std::int64_t>(256 + half, 160, 352);
            auto sh = [&](std::uint8_t c) { return static_cast<std::uint8_t>(std::min<std::int64_t>(255, (c * f + 128) >> 8)); };
            base[i] = Rgb{sh(land[i].r), sh(land[i].g), sh(land[i].b)};
        }
    }
    return comp_ref::overlay(w, h, base, &water, &river, &flow_dir);
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

std::string line_after(const std::string& text, std::string_view key) {
    const std::size_t at = text.find("\n" + std::string(key) + " = ");
    REQUIRE_MESSAGE(at != std::string::npos, "no line " << std::string(key));
    const std::size_t end = text.find('\n', at + 1);
    return text.substr(at + 1, end - at - 1);
}

// One generated pair of worlds per test run (pure function of the inputs).
// The Sandbox is never destroyed: its destructor runs doctest CHECKs, which
// would run after doctest has finished for a static object (a segfault at
// exit). It is held by a static pointer, so it stays reachable (no
// LeakSanitizer report), and it is created once. Generation runs inside the
// first test that needs the worlds, never in a static initializer: a failing
// REQUIRE there fails that test only; later tests fail at once on `ok`
// instead of retrying (and allocating) again. The directory is removed at
// exit by DirRemover without doctest.
struct Worlds {
    Sandbox& sb;
    fs::path data, full, climate;
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
    static Sandbox* const sb = new Sandbox;  // NOLINT: deliberately never destroyed, see above
    static DirRemover remover{sb->root().parent_path()};
    static Worlds w{*sb, {}, {}, {}};
    if (!w.tried) {
        w.tried = true;
        w.data = write_data_root(w.sb);
        w.full = w.sb.file("seed0.db");
        w.climate = w.sb.file("seed0-climate.db");
        generate(w.sb, w.data, {"--until", "biomes"}, w.full);
        generate(w.sb, w.data, {"--until", "climate"}, w.climate);
        w.ok = true;
    }
    REQUIRE_MESSAGE(w.ok, "the shared worlds of cli_biomes_test could not be generated (see the first failure)");
    return w;
}

}  // namespace

TEST_CASE("cli P1-04 3, P1-00 X1: --until biomes at 512: layer 11 biome; earlier layers and tables unchanged; golden hashes") {
    Worlds& w = worlds();
    const auto lf = detail(w.sb, w.full, "layer");
    const auto lc = detail(w.sb, w.climate, "layer");
    CHECK(lf.size() == 11U);
    CHECK(lc.size() == 10U);
    for (const auto& [name, hex] : lc) {
        CAPTURE(name);
        CHECK(lf.at(name) == hex);
    }
    const auto g4 = golden_case(UNMAPPED_P1_04_GOLDEN_PATH, "shipped-512-s0");
    const auto g3 = golden_case(UNMAPPED_P1_03_GOLDEN_PATH, "shipped-512-s0");
    REQUIRE(!g4.empty());
    CHECK(lf.at("biome") == g4.at("biome"));
    // The climate layers are the 0.9.0 reference's (P1-03 10.4; the offset of seed 0 is 20).
    CHECK(lc.at("temperature") == g3.at("temperature"));
    CHECK(lc.at("rainfall") == g3.at("rainfall"));
    for (const char* in : {"elevation", "water", "water_depth", "flow_dir", "river", "coast_dist"}) {
        CAPTURE(std::string_view(in));
        CHECK(lf.at(in) == g4.at(in));
    }
    const auto tf = detail(w.sb, w.full, "table");
    const auto tc = detail(w.sb, w.climate, "table");
    CHECK(tf.at("biome") == g4.at("table_biome"));
    CHECK(tf.at("climate") == g4.at("table_climate"));
    CHECK(tc.at("climate") == g4.at("table_climate"));
    CHECK(tf.at("pass") == tc.at("pass"));
    CHECK(tc.count("biome") == 1U);  // present and empty in a climate world
    CHECK(g4.at("wind") == "1");
    CHECK(g4.at("temp_offset_dc") == "20");
    CHECK(g3.at("temp_offset_dc") == "20");
    // P1-04 10.4: the exploration build's values, equal to the reference's.
    CHECK(lf.at("biome") == "d936d7be132e5802f5fcd875fa9e2dc96fd7c776c383d926110a318e8bdbba05");
    CHECK(tf.at("biome") == "a7878ee4c9ef402dfe8dce7f3691f0c0c84e6eb9d15eeb4cd3390e2d199f5472");
    CHECK(tf.at("climate") == "9709df66fb80b17713c5168055a0c0b5c3d60eab2ba7a853ad870803bead7bee");
}

TEST_CASE("cli P1-04 9, P1-03 9: report: [tables], wind_toward first of the climate group, biome_share and biome_patches") {
    Worlds& w = worlds();
    const ProcessResult r = w.sb.cli({"report", "--db", u8(w.full)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    const std::string& t = r.out;
    CHECK(contains(t, "\n[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 12\ncivilization = 0\nclimate = 1\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 11\nmeta = 17\nmonster = 0\nmonster_kind = 0\npass = "));
    CHECK(contains(t, "\nschema_version = 9\n"));
    CHECK(contains(t, "\nwind_toward = SE\ntemp_offset_c = 2.0\ntemperature_c = "));
    // P1-04 9's example line for 512 seed 0 (ten keys in code order).
    CHECK(contains(t, "\nbiome_share = temperate_forest:0.5810 grassland:0.1447 marsh:0.0068 highland_moor:0.0882 "
                      "alpine:0.0031 blighted_waste:0.0000 heath:0.0726 steppe:0.1021 wet_woodland:0.0015 "
                      "fen:0.0000\nbiome_patches = 123\n"));
    const auto g4 = golden_case(UNMAPPED_P1_04_GOLDEN_PATH, "shipped-512-s0");
    std::string share = g4.at("biome_share");
    std::replace(share.begin(), share.end(), '_', ' ');
    // blighted_waste's own underscore: the golden value writes spaces as "_",
    // so restore the definition ids.
    for (const char* id : {"temperate forest", "highland moor", "blighted waste", "wet woodland"}) {
        std::string from(id), to(id);
        std::replace(to.begin(), to.end(), ' ', '_');
        for (std::size_t at = share.find(from); at != std::string::npos; at = share.find(from)) {
            share.replace(at, from.size(), to);
        }
    }
    CHECK(line_after(t, "biome_share") == "biome_share = " + share);
    CHECK(line_after(t, "biome_patches") == "biome_patches = " + g4.at("biome_patches"));
    // Order: rain_contrast (climate), then biome_share, biome_patches.
    const std::size_t rc = t.find("\nrain_contrast = ");
    const std::size_t bs = t.find("\nbiome_share = ");
    REQUIRE(rc != std::string::npos);
    CHECK(t.find('\n', rc + 1) == bs);
    CHECK(t.find("\nbiome_patches = ") == t.find('\n', bs + 1));
    // The climate world: one climate row, empty biome table, no biome keys.
    const ProcessResult c = w.sb.cli({"report", "--db", u8(w.climate)});
    REQUIRE(c.exit_code == EXIT_OK);
    CHECK(contains(c.out, "\n[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\ncivilization = 0\nclimate = 1\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 10\n"));
    CHECK(contains(c.out, "\nwind_toward = SE\ntemp_offset_c = 2.0\n"));
    CHECK_FALSE(contains(c.out, "biome_share"));
}

TEST_CASE("cli P1-04 3: --until none, --until hydrology and the test pattern have both tables, empty") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    struct Case {
        std::vector<std::string> extra;
        const char* name;
    };
    for (const Case& c : {Case{{"--until", "none"}, "none.db"}, Case{{"--until", "hydrology"}, "hydro.db"},
                          Case{{"--test-pattern"}, "pattern.db"}}) {
        CAPTURE(std::string_view(c.name));
        const fs::path out = sb.file(c.name);
        generate(sb, data, c.extra, out);
        const ProcessResult r = sb.cli({"report", "--db", u8(out)});
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, "\n[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\ncivilization = 0\nclimate = 0\n"));
        CHECK_FALSE(contains(r.out, "wind_toward"));
        CHECK_FALSE(contains(r.out, "temp_offset_c"));
        CHECK(contains(r.out, "\nschema_version = 9\n"));
    }
}

TEST_CASE("cli P1-03 9: a malformed climate table is a corrupt-table error: exit 3, nothing on stdout") {
    Worlds& w = worlds();
    struct Bad {
        const char* what;
        const char* sql;
    };
    // Every reason of P1-03 9 (0.9.0), in its exact wording.
    const std::vector<Bad> bads = {
        {"2 rows, expected at most 1", "INSERT INTO climate VALUES (2, 1, 0)"},
        {"the row does not have three integer columns",
         "DROP TABLE climate; CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL) STRICT; "
         "INSERT INTO climate VALUES (1, 1)"},
        {"id 2, expected 1", "UPDATE climate SET id = 2"},
        {"wind_toward 8 outside 0..7", "UPDATE climate SET wind_toward = 8"},
        {"wind_toward -1 outside 0..7", "UPDATE climate SET wind_toward = -1"},
        {"temp_offset_dc 51 outside -50..50", "UPDATE climate SET temp_offset_dc = 51"},
        {"temp_offset_dc -51 outside -50..50", "UPDATE climate SET temp_offset_dc = -51"},
    };
    int k = 0;
    for (const Bad& b : bads) {
        CAPTURE(std::string_view(b.what));
        const fs::path bad = w.sb.file("badclimate" + std::to_string(k++) + ".db");
        fs::copy_file(w.climate, bad);
        REQUIRE(worldgen::db::testing::execute_on_file(u8(bad), b.sql).ok());
        const ProcessResult r = w.sb.cli({"report", "--db", u8(bad)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(r.err == std::string("worldgen-cli: error: corrupt table 'climate': ") + b.what + "\n");
    }
    // The bounds -50 and 50 are accepted.
    for (const char* v : {"50", "-50"}) {
        const fs::path ok = w.sb.file(std::string("okclimate") + v + ".db");
        fs::copy_file(w.climate, ok);
        REQUIRE(worldgen::db::testing::execute_on_file(u8(ok), std::string("UPDATE climate SET temp_offset_dc = ") + v).ok());
        const ProcessResult r = w.sb.cli({"report", "--db", u8(ok)});
        CHECK(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, std::string("\ntemp_offset_c = ") + (v[0] == '-' ? "-5.0" : "5.0") + "\n"));
    }
}

TEST_CASE("cli P1-03 3, P0-02 amendment 16: a schema-3 world is refused by report, hash --detail and render") {
    Worlds& w = worlds();
    const fs::path old = w.sb.file("schema3.db");
    fs::copy_file(w.climate, old);
    REQUIRE(worldgen::db::testing::execute_on_file(
                u8(old), "PRAGMA user_version = 3; UPDATE meta SET value = '3' WHERE key = 'schema_version'")
                .ok());
    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"report", "--db", u8(old)}, std::vector<std::string>{"hash", "--db", u8(old), "--detail"},
          std::vector<std::string>{"render", "--db", u8(old), "--layer", "elevation", "--out",
                                   u8(w.sb.file("schema3.png"))}}) {
        CAPTURE(args.front());
        const ProcessResult r = w.sb.cli(args);
        CHECK(r.exit_code != EXIT_OK);
        CHECK(r.out.empty());
        CHECK(contains(r.err, "world schema version 3, this build reads 9"));
    }
    CHECK_FALSE(fs::exists(w.sb.file("schema3.png")));
    // Plain `hash` hashes the file's bytes without opening it as a world
    // (P0-04 8), so it still prints a hash: P1-03 10.1 test 0's "refused by
    // hash" holds for `hash --detail` only (reported as a spec contradiction).
    CHECK(w.sb.cli({"hash", "--db", u8(old)}).exit_code == EXIT_OK);
}

TEST_CASE("cli P1-04 9: render --layer biome uses the table's colours, (255, 0, 255) for a code without a row") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    const std::vector<std::uint8_t> b = layer_of<std::uint8_t>(rd.value(), "biome");
    const fs::path a = w.sb.file("biome.auto.png");
    const fs::path n = w.sb.file("biome.named.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", "biome", "--out", u8(a)}).exit_code == EXIT_OK);
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--layer", "biome", "--palette", "biome", "--out", u8(n)})
                .exit_code == EXIT_OK);
    std::vector<Rgb> want(b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
        want[i] = palette(b[i], colours());
    }
    check_pixels(decode_png(a), want);
    CHECK(read_bytes(a) == read_bytes(n));
    // Without the forest row its cells are magenta; the others keep theirs.
    const fs::path cut = w.sb.file("cut.db");
    fs::copy_file(w.full, cut);
    REQUIRE(worldgen::db::testing::execute_on_file(u8(cut), "DELETE FROM biome WHERE id = 3").ok());
    const fs::path c = w.sb.file("cut.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(cut), "--layer", "biome", "--out", u8(c)}).exit_code == EXIT_OK);
    std::map<std::uint8_t, std::uint32_t> rows = colours();
    rows.erase(3);
    for (std::size_t i = 0; i < b.size(); ++i) {
        want[i] = palette(b[i], rows);
    }
    check_pixels(decode_png(c), want);
    // A recoloured row is drawn in its new colour: the palette reads the table.
    const fs::path re = w.sb.file("re.db");
    fs::copy_file(w.full, re);
    REQUIRE(worldgen::db::testing::execute_on_file(u8(re), "UPDATE biome SET colour = 1193046 WHERE id = 4").ok());
    const fs::path rp = w.sb.file("re.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(re), "--layer", "biome", "--out", u8(rp)}).exit_code == EXIT_OK);
    rows = colours();
    rows[4] = 0x123456;
    for (std::size_t i = 0; i < b.size(); ++i) {
        want[i] = palette(b[i], rows);
    }
    check_pixels(decode_png(rp), want);
}

TEST_CASE("cli P1-04 9, P1-00 10.2: render --composite biomes: biome colour shaded as the map, ocean, lakes and rivers") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    const std::vector<std::int16_t> e = layer_of<std::int16_t>(rd.value(), "elevation");
    const std::vector<std::uint8_t> b = layer_of<std::uint8_t>(rd.value(), "biome");
    const std::vector<std::uint8_t> water = layer_of<std::uint8_t>(rd.value(), "water");
    const std::vector<std::uint8_t> river = layer_of<std::uint8_t>(rd.value(), "river");
    const std::vector<std::uint8_t> fd = layer_of<std::uint8_t>(rd.value(), "flow_dir");
    // The test's composite first reproduces --composite map (validates the
    // shading and overlay here against the merged composite).
    std::vector<Rgb> hyps(e.size()), bio(e.size());
    for (std::size_t i = 0; i < e.size(); ++i) {
        hyps[i] = land_colour(e[i]);
        bio[i] = palette(b[i], colours());
    }
    const fs::path m = w.sb.file("m.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(w.full), "--composite", "map", "--out", u8(m)}).exit_code == EXIT_OK);
    check_pixels(decode_png(m), composite(512, 512, e, hyps, water, river, fd));
    // Default output <root>/out/<stem>.biomes.png.
    const ProcessResult r = w.sb.cli({"render", "--db", u8(w.full), "--composite", "biomes"});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    const fs::path out = w.sb.out_dir() / "seed0.biomes.png";
    CHECK(r.out == printed_path(out));
    check_pixels(decode_png(out), composite(512, 512, e, bio, water, river, fd));
    // A missing row draws (255, 0, 255) in the composite too, shaded.
    const fs::path cut = w.sb.file("cut-composite.db");
    fs::copy_file(w.full, cut);
    REQUIRE(worldgen::db::testing::execute_on_file(u8(cut), "DELETE FROM biome WHERE id = 6").ok());
    const fs::path co = w.sb.file("cut-composite.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(cut), "--composite", "biomes", "--out", u8(co)}).exit_code == EXIT_OK);
    std::map<std::uint8_t, std::uint32_t> rows = colours();
    rows.erase(6);
    for (std::size_t i = 0; i < e.size(); ++i) {
        bio[i] = palette(b[i], rows);
    }
    check_pixels(decode_png(co), composite(512, 512, e, bio, water, river, fd));
    // An empty table: every land cell magenta, shaded (lead's ruling: only a
    // missing layer is refused).
    const fs::path empty = w.sb.file("empty-table.db");
    fs::copy_file(w.full, empty);
    REQUIRE(worldgen::db::testing::execute_on_file(u8(empty), "DELETE FROM biome").ok());
    const fs::path eo = w.sb.file("empty-table.png");
    REQUIRE(w.sb.cli({"render", "--db", u8(empty), "--composite", "biomes", "--out", u8(eo)}).exit_code == EXIT_OK);
    for (std::size_t i = 0; i < e.size(); ++i) {
        bio[i] = Rgb{255, 0, 255};
    }
    check_pixels(decode_png(eo), composite(512, 512, e, bio, water, river, fd));
}

TEST_CASE("cli P1-04 9, 10.1 test 11: --composite biomes without biome, without elevation, with biome i16: exit 3, no file") {
    Sandbox sb;
    using worldgen::stages::testing::FixtureLayer;
    auto fixture = [&](const char* name, std::vector<FixtureLayer> layers) {
        const fs::path p = sb.file(name);
        REQUIRE(worldgen::stages::testing::write_fixture_world(u8(p), 3, layers).ok());
        return p;
    };
    const auto e = worldgen::db::LayerGrid{4, 3, std::vector<std::int16_t>(12, 50)};
    const auto b = worldgen::db::LayerGrid{4, 3, std::vector<std::uint8_t>(12, 3)};
    const auto bi16 = worldgen::db::LayerGrid{4, 3, std::vector<std::int16_t>(12, 3)};
    struct Case {
        fs::path db;
        std::string err;
    };
    const fs::path no_b = fixture("no-biome.db", {{"elevation", e, worldgen::db::Codec::delta_varint}});
    const fs::path no_e = fixture("no-elevation.db", {{"biome", b, worldgen::db::Codec::delta_varint}});
    const fs::path none = fixture("no-layers.db", {});
    const fs::path i16 = fixture("biome-i16.db", {{"elevation", e, worldgen::db::Codec::delta_varint},
                                                  {"biome", bi16, worldgen::db::Codec::delta_varint}});
    const std::vector<Case> cases = {
        {no_b, "worldgen-cli: error: world file '" + u8(no_b) + "' has no layer 'biome'; available layers: elevation\n"},
        {no_e, "worldgen-cli: error: world file '" + u8(no_e) + "' has no layer 'elevation'; available layers: biome\n"},
        {none, "worldgen-cli: error: world file '" + u8(none) + "' has no layer 'elevation'; the world has no layers\n"},
        {i16, ""},
    };
    int k = 0;
    for (const Case& c : cases) {
        CAPTURE(u8(c.db));
        const fs::path out = sb.file("c" + std::to_string(k++) + ".png");
        const ProcessResult r = sb.cli({"render", "--db", u8(c.db), "--composite", "biomes", "--out", u8(out)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        if (!c.err.empty()) {
            CHECK(r.err == c.err);
        } else {
            CHECK(contains(r.err, "layer 'biome' is i16, expected u8"));
        }
        CHECK_FALSE(file_exists(out));
    }
    // --palette biome suits u8 only: a usage error on elevation (i16).
    const ProcessResult p = sb.cli({"render", "--db", u8(i16), "--layer", "elevation", "--palette", "biome", "--out",
                                    u8(sb.file("p.png"))});
    CHECK(p.exit_code == EXIT_USAGE);
    CHECK(contains(p.err, "u8"));
}

TEST_CASE("cli P1-04 9, 10.1 test 10: report refuses malformed biome rows and a biome value without a row: exit 3") {
    Worlds& w = worlds();
    auto rd = worldgen::db::WorldReader::open(u8(w.full));
    REQUIRE(rd.ok());
    const std::vector<std::uint8_t> b = layer_of<std::uint8_t>(rd.value(), "biome");
    // The lowest cell with code 6 (moor), for the missing-row case.
    const auto it = std::find(b.begin(), b.end(), std::uint8_t{6});
    REQUIRE(it != b.end());
    const auto idx = static_cast<std::size_t>(it - b.begin());
    struct Bad {
        const char* sql;
        std::string err;
    };
    const std::vector<Bad> bads = {
        {"UPDATE biome SET key = '' WHERE id = 5", "worldgen-cli: error: corrupt table 'biome': row 5: empty key\n"},
        {"UPDATE biome SET water = 2 WHERE id = 7", "worldgen-cli: error: corrupt table 'biome': row 7: water not 0 or 1\n"},
        {"UPDATE biome SET colour = 16777216 WHERE id = 3",
         "worldgen-cli: error: corrupt table 'biome': row 3: colour outside 0..16777215\n"},
        {"INSERT INTO biome VALUES (255, 'z', 'Z', 0, 0)",
         "worldgen-cli: error: corrupt table 'biome': row 255: id outside 1..254\n"},
        {"DELETE FROM biome WHERE id = 6", "worldgen-cli: error: corrupt layer 'biome': value 6 at (" +
                                               std::to_string(idx % 512U) + ", " + std::to_string(idx / 512U) +
                                               ") has no row in table 'biome'\n"},
    };
    int k = 0;
    for (const Bad& x : bads) {
        CAPTURE(std::string_view(x.sql));
        const fs::path bad = w.sb.file("badbiome" + std::to_string(k++) + ".db");
        fs::copy_file(w.full, bad);
        REQUIRE(worldgen::db::testing::execute_on_file(u8(bad), x.sql).ok());
        const ProcessResult r = w.sb.cli({"report", "--db", u8(bad)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(r.err == x.err);
    }
}

TEST_CASE("cli P1-00 10.2: render --help lists every palette --palette accepts, and the composites") {
    Sandbox sb;
    const ProcessResult h = sb.cli({"render", "--help"});
    REQUIRE(h.exit_code == EXIT_OK);
    const std::string& t = h.out;
    const std::size_t at = t.find("\n  --palette P");  // the option line, not the usage
    REQUIRE(at != std::string::npos);
    // The --palette description runs until the next option line.
    const std::size_t end = t.find("\n  --", at + 1);
    const std::string desc = t.substr(at, end - at);
    auto listed = [&](std::string_view name) {
        const std::regex word("(^|[^a-z0-9_])" + std::string(name) + "([^a-z0-9_]|$)");
        return std::regex_search(desc, word);
    };
    // Every palette name of P1-00 10.2 and P1-03, P1-04 9 with a layer it
    // suits: each is accepted and listed.
    const std::vector<std::pair<std::string, std::string>> accepted = {
        {"auto", "biome"},          {"gray", "biome"},          {"hypsometric", "elevation"},
        {"water", "water"},         {"river", "river"},         {"log2", "flow_accum"},
        {"direction", "flow_dir"},  {"ids", "lake"},            {"signed", "coast_dist"},
        {"temperature", "temperature"}, {"rainfall", "rainfall"}, {"biome", "biome"}};
    Worlds& w = worlds();
    for (const auto& [p, layer] : accepted) {
        CAPTURE(p);
        CHECK(listed(p));
        const ProcessResult r = w.sb.cli({"render", "--db", u8(w.full), "--layer", layer, "--palette", p, "--out",
                                          u8(w.sb.file("help-" + p + ".png"))});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_OK);
    }
    CHECK(sb.cli({"render", "--db", u8(w.full), "--layer", "biome", "--palette", "biomes"}).exit_code == EXIT_USAGE);
    // The composites map and biomes are named.
    CHECK(contains(t, "--composite"));
    CHECK(contains(t, "map"));
    CHECK(contains(t, "biomes"));
}

TEST_CASE("cli P1-00 10.1: --until biomes is accepted; an unknown stage is a usage error") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    // Every geography stage is implemented since P1-06 (generator 0.11.0):
    // an unknown name stands in for the unimplemented stage of earlier
    // versions.
    const ProcessResult r = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(data), "--until",
                                    "bogus", "--out", u8(sb.file("r.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    // A data root without the biome definitions is an input error (exit 3).
    const fs::path bare = sb.file("bare");
    fs::create_directories(bare / "worldgen");
    write_text(bare / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(bare / "worldgen" / "hydrology.toml", HYDROLOGY_TOML);
    write_text(bare / "worldgen" / "climate.toml", std::string(elev_api::CLIMATE_TOML));
    write_text(bare / "worldgen" / "biomes.toml", biome_data::TUNING_TOML);
    write_text(bare / "worldgen" / "resources.toml", biome_data::RESOURCES_TOML);
    write_text(bare / "worldgen" / "regions.toml", biome_data::REGIONS_TOML);
    const ProcessResult m = sb.cli({"generate", "--seed", "1", "--map-size", "512", "--data", u8(bare), "--until",
                                    "none", "--out", u8(sb.file("m.db"))});
    CHECK(m.exit_code == EXIT_INPUT);
}

TEST_SUITE("biomes-slow" * doctest::skip()) {

TEST_CASE("cli P1-00 X3 (slow): --until biomes at 512, seeds 0 and 42, --threads 1, 2 and 5: byte-identical files") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    for (const char* seed : {"0", "42"}) {
        std::vector<Bytes> files;
        for (const char* threads : {"1", "2", "5"}) {
            const fs::path out = sb.file(std::string("s") + seed + "-t" + threads + ".db");
            const ProcessResult r = sb.cli({"generate", "--seed", seed, "--map-size", "512", "--data", u8(data),
                                            "--threads", threads, "--out", u8(out)});
            REQUIRE(r.exit_code == EXIT_OK);
            files.push_back(read_bytes(out));
        }
        CHECK(files[0] == files[1]);
        CHECK(files[0] == files[2]);
    }
}

}  // TEST_SUITE
