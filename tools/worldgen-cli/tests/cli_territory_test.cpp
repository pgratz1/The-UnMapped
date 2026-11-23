// worldgen-cli render --composite territory [--year Y] (spec P4-01 section 7, 13.6 items 10 to 13; P0-04
// amendment 23): the default name <stem>.territory-<E>.png and the printed path, --year 0 and E, --scale 2,
// --out and an existing output without --force; the usage errors (exit 2) in their order, the year range
// after opening the file; worlds without the layers or without history and a corrupt history (exit 3, the
// messages of the regions composite and of report --history); the render --help lines of section 7; and the
// PNG's pixels against the library's render_territory on the same world (the CLI formats nothing itself).
//
// Written from the spec only. Reading: the range error of section 7 item 5 is a usage error (exit 2), so it
// is followed by the usage hint line like every usage error; the test checks the error line and exit code
// and that the hint is the render hint when present.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "png_decode.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/territory.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

using namespace clitest;
namespace db = worldgen::db;
namespace wh = worldgen::history;
namespace wi = worldgen::inspect;

constexpr std::string_view RENDER_HINT = "worldgen-cli: run 'worldgen-cli render --help' for usage\n";

fs::path source_root() { return fs::path(UNMAPPED_TEST_SOURCE_ROOT); }

fs::path generate(const Sandbox& sb, const std::string& name, std::vector<std::string> args) {
    std::vector<std::string> full = {"--data", u8(source_root() / "game" / "data"), "--out", u8(sb.file(name + ".db"))};
    full.insert(full.end(), args.begin(), args.end());
    return sb.generate(full);
}

// A usage error: exit 2, nothing on stdout, the error line first, then (reading above) the render hint.
void check_usage(const ProcessResult& r, const std::string& message) {
    CAPTURE(r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
    const std::string line = "worldgen-cli: error: " + message + "\n";
    CHECK(starts_with(r.err, line));
    if (r.err.size() > line.size()) CHECK(r.err.substr(line.size()) == RENDER_HINT);
}

// The library's territory image of a world at year y and scale k.
std::vector<std::uint8_t> library_territory(const fs::path& path, std::int64_t y, std::uint32_t k) {
    auto opened = db::WorldReader::open(u8(path));
    REQUIRE(opened.ok());
    const db::WorldReader r = std::move(opened).value();
    auto h = wh::load_history(r);
    REQUIRE(h.ok());
    auto layer = [&](const char* name) -> std::optional<db::LayerGrid> {
        auto l = r.read_layer(name);
        if (!l.ok()) return std::nullopt;
        return std::move(l.value().grid);
    };
    const std::optional<db::LayerGrid> elevation = layer("elevation"), biome = layer("biome"), region = layer("region"),
                                       water = layer("water"), river = layer("river"), flow_dir = layer("flow_dir");
    REQUIRE(elevation.has_value());
    REQUIRE(biome.has_value());
    REQUIRE(region.has_value());
    auto rows = db::read_biome_rows(r);
    REQUIRE(rows.ok());
    std::vector<wi::BiomeColour> table;
    for (const db::BiomeRow& b : rows.value()) table.push_back({b.id, b.colour});
    const wi::TerritoryBase base =
        wi::TerritoryBase::build(*elevation, water ? &*water : nullptr, river ? &*river : nullptr,
                                 flow_dir ? &*flow_dir : nullptr, *biome, *region, table);
    const auto fill = wi::territory_fill(h.value().timeline, wi::civilization_colours(h.value().tables), y);
    return wi::render_territory(base, fill, k);
}

pngtest::Image png_of(const fs::path& p) {
    const pngtest::Image img = pngtest::decode(read_bytes(p));
    REQUIRE(img.ok);
    return img;
}

}  // namespace

TEST_SUITE("territory") {

TEST_CASE("cli P4-01 13.6 items 10 and 13: render --composite territory on a default 512 world") {
    Sandbox sb;
    const fs::path db = generate(sb, "world", {"--seed", "42", "--map-size", "512"});
    // the default year is E = 6000, in the default name, printed as an absolute path
    const ProcessResult r = sb.cli({"render", "--db", u8(db), "--composite", "territory"});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    CHECK(r.err.empty());
    const fs::path present = sb.out_dir() / "world.territory-6000.png";
    CHECK(r.out == printed_path(present));
    REQUIRE(file_exists(present));
    const pngtest::Image img = png_of(present);
    CHECK(img.width == 512U);
    CHECK(img.height == 512U);
    CHECK(img.rgb == library_territory(db, 6000, 1));
    // --year E names the same file: exists now, so exit 4 without --force, and the file is untouched
    const Bytes before = read_bytes(present);
    const ProcessResult again = sb.cli({"render", "--db", u8(db), "--composite", "territory", "--year", "6000"});
    CHECK(again.exit_code == EXIT_OUTPUT);
    CHECK(again.out.empty());
    CHECK(read_bytes(present) == before);
    const ProcessResult forced =
        sb.cli({"render", "--db", u8(db), "--composite", "territory", "--year", "6000", "--force"});
    CHECK(forced.exit_code == EXIT_OK);
    CHECK(read_bytes(present) == before);
    // --year 0: the name carries the year
    const ProcessResult y0 = sb.cli({"render", "--db", u8(db), "--year", "0", "--composite", "territory"});
    REQUIRE(y0.exit_code == EXIT_OK);
    const fs::path first = sb.out_dir() / "world.territory-0.png";
    CHECK(y0.out == printed_path(first));
    CHECK(png_of(first).rgb == library_territory(db, 0, 1));
    // a middle year, --scale 2 and --out
    const fs::path out = sb.file("t3000.png");
    const ProcessResult s2 =
        sb.cli({"render", "--db", u8(db), "--composite", "territory", "--year", "3000", "--scale", "2", "--out", u8(out)});
    REQUIRE(s2.exit_code == EXIT_OK);
    CHECK(s2.out == printed_path(out));
    const pngtest::Image big = png_of(out);
    CHECK(big.width == 1024U);
    CHECK(big.height == 1024U);
    CHECK(big.rgb == library_territory(db, 3000, 2));
    CHECK_FALSE(file_exists(sb.out_dir() / "world.territory-3000.png"));
    // an existing --out without --force: exit 4
    const ProcessResult exists = sb.cli({"render", "--db", u8(db), "--composite", "territory", "--out", u8(out)});
    CHECK(exists.exit_code == EXIT_OUTPUT);
    CHECK(png_of(out).rgb == big.rgb);
    // the regions composite is unchanged by the new composite's existence: equal to territory with nothing held
    const ProcessResult reg = sb.cli({"render", "--db", u8(db), "--composite", "regions"});
    REQUIRE(reg.exit_code == EXIT_OK);
    CHECK(reg.out == printed_path(sb.out_dir() / "world.regions.png"));
}

TEST_CASE("cli P4-01 13.6 item 11: usage errors, the year range and the help text") {
    Sandbox sb;
    const fs::path db = generate(sb, "world", {"--seed", "7", "--map-size", "512", "--history-span", "2000"});
    const std::string path = u8(db);
    // --year without --composite territory
    check_usage(sb.cli({"render", "--db", path, "--layer", "elevation", "--year", "5"}),
                "--year applies to --composite territory only");
    check_usage(sb.cli({"render", "--db", path, "--composite", "regions", "--year", "5"}),
                "--year applies to --composite territory only");
    // checked before the file is opened
    check_usage(sb.cli({"render", "--db", u8(sb.file("missing.db")), "--composite", "map", "--year", "5"}),
                "--year applies to --composite territory only");
    // malformed values: the shared integer parser's message
    for (const char* bad : {"01", "-1", "x", "+1", "1.5", "00"}) {
        CAPTURE(std::string(bad));
        check_usage(sb.cli({"render", "--db", path, "--composite", "territory", "--year", bad}),
                    "invalid --year '" + std::string(bad) + "'; expected a decimal integer without sign or leading zeros");
    }
    // malformed before the file is opened
    check_usage(sb.cli({"render", "--db", u8(sb.file("missing.db")), "--composite", "territory", "--year", "x"}),
                "invalid --year 'x'; expected a decimal integer without sign or leading zeros");
    // the range, judged after opening the file: exit 2, nothing written
    for (const char* big : {"2001", "9223372036854775807", "9223372036854775808", "18446744073709551615"}) {
        CAPTURE(std::string(big));
        const ProcessResult r = sb.cli({"render", "--db", path, "--composite", "territory", "--year", big});
        check_usage(r, "--year " + std::string(big) + " is outside the history of world file '" + path +
                           "' (0 to 2000)");
        CHECK_FALSE(file_exists(sb.out_dir() / ("world.territory-" + std::string(big) + ".png")));
    }
    // the bounds themselves are accepted
    CHECK(sb.cli({"render", "--db", path, "--composite", "territory", "--year", "2000"}).exit_code == EXIT_OK);
    CHECK(file_exists(sb.out_dir() / "world.territory-2000.png"));
    // an unknown composite lists territory
    check_usage(sb.cli({"render", "--db", path, "--composite", "realms"}),
                "invalid --composite 'realms'; expected map, biomes, regions, territory");
    // render --help: the lines of section 7, exactly
    const ProcessResult h = sb.cli({"render", "--help"});
    CHECK(h.exit_code == EXIT_OK);
    CHECK(contains(h.out, "usage: worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME "
                          "[--year Y]) [--out PATH]\n"
                          "                           [--scale N] [--force] [--verbose]\n"));
    CHECK(contains(h.out, "\n                      regions: the biomes composite with the borders of layer region "
                          "in black;\n"
                          "                      territory: the regions composite with each region held in the "
                          "year in the\n"
                          "                      colour of its holder (needs the history)\n"));
    CHECK(contains(h.out, "\n  --year Y            with --composite territory: the year, 0 to the history's end "
                          "year\n"
                          "                      (default the end year, the present)\n"));
    CHECK(contains(h.out, "\n  --out PATH          output PNG (default <build root>/out/<db file stem>.<layer>.png,\n"
                          "                      <db file stem>.<composite>.png for --composite, or\n"
                          "                      <db file stem>.territory-<year>.png for territory)\n"));
}

TEST_CASE("cli P4-01 13.6 item 12: worlds without the layers or the history, and a corrupt history, exit 3") {
    Sandbox sb;
    {
        // the geography checks come first: the test pattern has no elevation
        const fs::path db = generate(sb, "pattern", {"--seed", "1", "--test-pattern", "--map-size", "512"});
        const ProcessResult r = sb.cli({"render", "--db", u8(db), "--composite", "territory"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: world file '" + u8(db) + "' has no layer 'elevation'; "));
        CHECK(contains(r.err, "; available layers: "));
        CHECK_FALSE(file_exists(sb.out_dir() / "pattern.territory-0.png"));
        // also with --year: the history is never reached
        const ProcessResult y = sb.cli({"render", "--db", u8(db), "--composite", "territory", "--year", "5"});
        CHECK(y.exit_code == EXIT_INPUT);
        CHECK(y.err == r.err);
    }
    {
        // no region layer (--until biomes): the regions composite's message, the reader's missing-layer error
        const fs::path db = generate(sb, "biomes", {"--seed", "1", "--map-size", "512", "--until", "biomes"});
        const ProcessResult r = sb.cli({"render", "--db", u8(db), "--composite", "territory"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: world file '" + u8(db) + "' has no layer 'region'; available "
                                 "layers: "));
        const ProcessResult reg = sb.cli({"render", "--db", u8(db), "--composite", "regions"});
        CHECK(reg.exit_code == EXIT_INPUT);
        CHECK(reg.err == r.err);
    }
    for (const char* until : {"regions", "chronology"}) {
        CAPTURE(std::string(until));
        const std::string name = std::string("until-") + until;
        const fs::path db = generate(sb, name, {"--seed", "1", "--map-size", "512", "--until", until});
        const ProcessResult r = sb.cli({"render", "--db", u8(db), "--composite", "territory"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        const std::string stages = std::string("elevation,hydrology,climate,biomes,resources,regions") +
                                   (std::string(until) == "chronology" ? ",chronology" : "");
        CHECK(r.err == "worldgen-cli: error: world file '" + u8(db) + "' has no history (stages: " + stages +
                           "); generate it with the history stage\n");
        // the message of report --history, exactly
        const ProcessResult rep = sb.cli({"report", "--db", u8(db), "--history"});
        CHECK(rep.exit_code == EXIT_INPUT);
        CHECK(rep.err == r.err);
        // the history check comes before the year range
        const ProcessResult y = sb.cli({"render", "--db", u8(db), "--composite", "territory", "--year", "999999"});
        CHECK(y.exit_code == EXIT_INPUT);
        CHECK(y.err == r.err);
    }
    {
        // a corrupt history: the report's message
        const fs::path db = generate(sb, "corrupt", {"--seed", "7", "--map-size", "512", "--history-span", "2000"});
        REQUIRE(db::testing::execute_on_file(u8(db), "UPDATE civilization SET home_region = 99999 WHERE id = 1").ok());
        const ProcessResult r = sb.cli({"render", "--db", u8(db), "--composite", "territory"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(r.err == "worldgen-cli: error: corrupt table 'civilization': row 1: home_region 99999 is not a region\n");
        const ProcessResult rep = sb.cli({"report", "--db", u8(db), "--history"});
        CHECK(rep.exit_code == EXIT_INPUT);
        CHECK(rep.err == r.err);
        CHECK_FALSE(file_exists(sb.out_dir() / "corrupt.territory-2000.png"));
    }
}

}  // TEST_SUITE
