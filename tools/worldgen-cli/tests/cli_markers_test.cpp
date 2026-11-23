// worldgen-cli render --markers [--faded] (spec P4-04 section 7 and 10.5; P0-04 amendment 24): --markers with
// --composite territory, regions, map and --layer elevation, the default year and --year 0, --scale 2 and
// --faded: exit 0, the default names of section 7, and the PNG's pixels equal to the image without --markers
// (the CLI's own, itself compared with the library elsewhere; for territory the library's render_territory)
// with the library's draw_markers of MarkerIndex::markers_at(Y, {faded, true, true}) drawn over it at the same
// scale; the usage errors (exit 2) in their order; the no-history message on --until regions and --test-pattern
// worlds and a malformed pass row (exit 3, the messages of report); the order of the marker index before the
// year range; render --help.
//
// Written from the spec only. Readings: the wrapping of the new synopsis and of the changed --year and --out
// lines is not given by section 7, so those are compared with runs of spaces and line ends folded to one space;
// the two new option lines are given exactly and are compared exactly.

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
#include "worldgen/inspect/markers.hpp"
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

void check_usage(const ProcessResult& r, const std::string& message) {
    CAPTURE(r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
    const std::string line = "worldgen-cli: error: " + message + "\n";
    CHECK(starts_with(r.err, line));
    if (r.err.size() > line.size()) CHECK(r.err.substr(line.size()) == RENDER_HINT);
}

pngtest::Image png_of(const fs::path& p) {
    const pngtest::Image img = pngtest::decode(read_bytes(p));
    REQUIRE(img.ok);
    return img;
}

// Runs of spaces, tabs and line ends folded to one space.
std::string fold(std::string_view s) {
    std::string out;
    bool space = false;
    for (const char c : s) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            space = true;
            continue;
        }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

struct Opened {
    db::WorldReader reader;
    wh::LoadedHistory h;
};

Opened open(const fs::path& path) {
    auto r = db::WorldReader::open(u8(path));
    REQUIRE(r.ok());
    db::WorldReader reader = std::move(r).value();
    auto h = wh::load_history(reader);
    REQUIRE(h.ok());
    return Opened{std::move(reader), std::move(h).value()};
}

// `under` (an RGB buffer of width x height cells at scale k) with the library's markers of year y drawn over it.
std::vector<std::uint8_t> with_markers(const fs::path& path, std::vector<std::uint8_t> under, std::uint32_t width,
                                       std::uint32_t height, std::uint32_t k, std::int64_t y, bool faded) {
    const Opened o = open(path);
    auto ix = wi::MarkerIndex::load(o.reader, o.h);
    REQUIRE(ix.ok());
    const wi::YearMarkers m = ix.value().markers_at(o.h, y, wi::MarkerOptions{faded, true, true});
    wi::draw_markers(under, width, height, k, m, wi::civilization_colours(o.h.tables));
    return under;
}

// The library's territory image (P4-03 r4 9.2, the timeline form).
std::vector<std::uint8_t> library_territory(const fs::path& path, std::int64_t y, std::uint32_t k) {
    const Opened o = open(path);
    auto layer = [&](const char* name) -> std::optional<db::LayerGrid> {
        auto l = o.reader.read_layer(name);
        if (!l.ok()) return std::nullopt;
        return std::move(l.value().grid);
    };
    const std::optional<db::LayerGrid> elevation = layer("elevation"), biome = layer("biome"), region = layer("region"),
                                       water = layer("water"), river = layer("river"), flow_dir = layer("flow_dir");
    REQUIRE(elevation.has_value());
    REQUIRE(biome.has_value());
    REQUIRE(region.has_value());
    auto rows = db::read_biome_rows(o.reader);
    REQUIRE(rows.ok());
    std::vector<wi::BiomeColour> table;
    for (const db::BiomeRow& b : rows.value()) table.push_back({b.id, b.colour});
    const wi::TerritoryBase base =
        wi::TerritoryBase::build(*elevation, water ? &*water : nullptr, river ? &*river : nullptr,
                                 flow_dir ? &*flow_dir : nullptr, *biome, *region, table);
    return wi::render_territory(base, o.h.timeline, wi::civilization_colours(o.h.tables), y, k);
}

// One --markers render: exit 0, the path printed, the PNG `expect_name` in the output directory (or `out`).
pngtest::Image run_ok(const Sandbox& sb, std::vector<std::string> args, const fs::path& expect) {
    const ProcessResult r = sb.cli(args);
    CAPTURE(r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    CHECK(r.err.empty());
    CHECK(r.out == printed_path(expect));
    REQUIRE(file_exists(expect));
    return png_of(expect);
}

}  // namespace

TEST_SUITE("territory") {

TEST_CASE("cli P4-04 10.5 item 1: --markers over every image, default names, the library's pixels") {
    Sandbox sb;
    const fs::path db = generate(sb, "world", {"--seed", "42", "--map-size", "512"});
    const std::string path = u8(db);
    const fs::path out = sb.out_dir();
    const std::uint32_t N = 512;
    // territory, default year E = 6000
    {
        const pngtest::Image img = run_ok(sb, {"render", "--db", path, "--composite", "territory", "--markers"},
                                          out / "world.territory-6000.markers.png");
        CHECK(img.width == N);
        CHECK(img.height == N);
        CHECK(img.rgb == with_markers(db, library_territory(db, 6000, 1), N, N, 1, 6000, false));
        // the markers change the image
        CHECK(img.rgb != library_territory(db, 6000, 1));
    }
    // territory at --year 0 and --faded at E
    {
        const pngtest::Image img =
            run_ok(sb, {"render", "--db", path, "--composite", "territory", "--year", "0", "--markers"},
                   out / "world.territory-0.markers.png");
        CHECK(img.rgb == with_markers(db, library_territory(db, 0, 1), N, N, 1, 0, false));
        const pngtest::Image faded =
            run_ok(sb, {"render", "--db", path, "--composite", "territory", "--markers", "--faded"},
                   out / "world.territory-6000.markers-faded.png");
        CHECK(faded.rgb == with_markers(db, library_territory(db, 6000, 1), N, N, 1, 6000, true));
        // the seed 42 world has faded ruins at E (P4-04 preamble: 8), so the two images differ
        CHECK(faded.rgb != png_of(out / "world.territory-6000.markers.png").rgb);
    }
    // regions, map and --layer elevation: over the CLI's own image without --markers
    struct C {
        std::vector<std::string> what;
        std::string name;
    };
    const C cases[] = {{{"--composite", "regions"}, "regions"},
                       {{"--composite", "map"}, "map"},
                       {{"--composite", "biomes"}, "biomes"},
                       {{"--layer", "elevation"}, "elevation"}};
    for (const C& c : cases) {
        CAPTURE(c.name);
        std::vector<std::string> plain = {"render", "--db", path};
        plain.insert(plain.end(), c.what.begin(), c.what.end());
        const fs::path plain_png = out / ("world." + c.name + ".png");
        const pngtest::Image under = run_ok(sb, plain, plain_png);
        for (const std::int64_t y : {std::int64_t{6000}, std::int64_t{0}}) {
            CAPTURE(y);
            std::vector<std::string> args = plain;
            args.push_back("--markers");
            if (y == 0) {
                args.push_back("--year");
                args.push_back("0");
            }
            const fs::path png = out / ("world." + c.name + "-" + std::to_string(y) + ".markers.png");
            const pngtest::Image img = run_ok(sb, args, png);
            CHECK(img.rgb == with_markers(db, under.rgb, N, N, 1, y, false));
        }
    }
    // --scale 2 with --out, and --faded at scale 2 over regions at --year 3000
    {
        const fs::path o2 = sb.file("t-s2.png");
        const pngtest::Image img =
            run_ok(sb, {"render", "--db", path, "--composite", "territory", "--markers", "--scale", "2", "--out", u8(o2)},
                   o2);
        CHECK(img.width == 2 * N);
        CHECK(img.rgb == with_markers(db, library_territory(db, 6000, 2), N, N, 2, 6000, false));
        // equal to the scale-1 image scaled
        const pngtest::Image one = png_of(out / "world.territory-6000.markers.png");
        bool same = true;
        for (std::uint32_t y = 0; same && y < 2 * N; ++y) {
            for (std::uint32_t x = 0; same && x < 2 * N; ++x) {
                const std::size_t a = 3 * (std::size_t{y} * 2 * N + x), b = 3 * (std::size_t{y / 2} * N + x / 2);
                same = img.rgb[a] == one.rgb[b] && img.rgb[a + 1] == one.rgb[b + 1] && img.rgb[a + 2] == one.rgb[b + 2];
            }
        }
        CHECK(same);
        const fs::path rf = sb.file("r-s2.png");
        const fs::path rp = sb.file("r-plain-s2.png");
        const pngtest::Image under = run_ok(sb, {"render", "--db", path, "--composite", "regions", "--scale", "2",
                                                 "--out", u8(rp)}, rp);
        const pngtest::Image f2 = run_ok(sb, {"render", "--db", path, "--composite", "regions", "--year", "3000",
                                              "--markers", "--faded", "--scale", "2", "--out", u8(rf)}, rf);
        CHECK(f2.rgb == with_markers(db, under.rgb, N, N, 2, 3000, true));
        CHECK_FALSE(file_exists(out / "world.regions-3000.markers-faded.png"));
    }
    // an existing output without --force: exit 4, untouched; with --force: rewritten, the same bytes
    {
        const fs::path present = out / "world.territory-6000.markers.png";
        const Bytes before = read_bytes(present);
        const ProcessResult again = sb.cli({"render", "--db", path, "--composite", "territory", "--markers"});
        CHECK(again.exit_code == EXIT_OUTPUT);
        CHECK(again.out.empty());
        CHECK(read_bytes(present) == before);
        const ProcessResult forced =
            sb.cli({"render", "--db", path, "--composite", "territory", "--markers", "--year", "6000", "--force"});
        CHECK(forced.exit_code == EXIT_OK);
        CHECK(read_bytes(present) == before);
    }
    // without --markers the names are as before
    CHECK(file_exists(out / "world.regions.png"));
    CHECK(file_exists(out / "world.map.png"));
    CHECK(file_exists(out / "world.elevation.png"));
}

TEST_CASE("cli P4-04 10.5 item 2: usage errors and the help text") {
    Sandbox sb;
    const fs::path db = generate(sb, "world", {"--seed", "7", "--map-size", "512", "--history-span", "2000"});
    const std::string path = u8(db);
    const std::string missing = u8(sb.file("missing.db"));
    // --faded without --markers
    check_usage(sb.cli({"render", "--db", path, "--composite", "territory", "--faded"}), "--faded applies to --markers only");
    check_usage(sb.cli({"render", "--db", path, "--layer", "elevation", "--faded"}), "--faded applies to --markers only");
    check_usage(sb.cli({"render", "--db", missing, "--composite", "map", "--faded"}), "--faded applies to --markers only");
    // --faded is checked before --year
    check_usage(sb.cli({"render", "--db", path, "--composite", "regions", "--faded", "--year", "5"}),
                "--faded applies to --markers only");
    // --year without --markers on an image other than territory: the new message
    check_usage(sb.cli({"render", "--db", path, "--composite", "regions", "--year", "5"}),
                "--year applies to --composite territory or --markers only");
    check_usage(sb.cli({"render", "--db", path, "--layer", "elevation", "--year", "5"}),
                "--year applies to --composite territory or --markers only");
    check_usage(sb.cli({"render", "--db", missing, "--composite", "map", "--year", "5"}),
                "--year applies to --composite territory or --markers only");
    check_usage(sb.cli({"render", "--db", path, "--layer", "elevation", "--year", "x"}),
                "--year applies to --composite territory or --markers only");
    // with --markers --year applies to every image; malformed values: the shared message, before opening
    for (const char* bad : {"01", "-1", "x", "+1", "1.5"}) {
        CAPTURE(std::string(bad));
        check_usage(sb.cli({"render", "--db", missing, "--composite", "regions", "--markers", "--year", bad}),
                    "invalid --year '" + std::string(bad) + "'; expected a decimal integer without sign or leading zeros");
    }
    // the year range after opening (exit 2), with the message of amendment 23
    for (const char* big : {"2001", "9223372036854775808"}) {
        CAPTURE(std::string(big));
        check_usage(sb.cli({"render", "--db", path, "--layer", "elevation", "--markers", "--year", big}),
                    "--year " + std::string(big) + " is outside the history of world file '" + path + "' (0 to 2000)");
        CHECK_FALSE(file_exists(sb.out_dir() / ("world.elevation-" + std::string(big) + ".markers.png")));
    }
    CHECK(sb.cli({"render", "--db", path, "--layer", "elevation", "--markers", "--year", "2000"}).exit_code == EXIT_OK);
    CHECK(file_exists(sb.out_dir() / "world.elevation-2000.markers.png"));
    // render --help
    const ProcessResult h = sb.cli({"render", "--help"});
    CHECK(h.exit_code == EXIT_OK);
    CHECK(contains(h.out, "\n  --markers           draw the settlements, ruins, event places and passes of the year over the\n"
                          "                      image (needs the history)\n"
                          "  --faded             with --markers: also the faded ruins\n"));
    const std::string f = fold(h.out);
    CHECK(contains(f, "usage: worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME) [--year Y] "
                      "[--markers [--faded]] [--out PATH] [--scale N] [--force] [--verbose]"));
    CHECK(contains(f, "--year Y with --composite territory or --markers: the year, 0 to the history's end year"));
    CHECK(contains(f, ", with -<year>.markers.png for --markers"));
    // the new lines follow the --year lines
    const std::size_t year_at = h.out.find("\n  --year Y ");
    const std::size_t markers_at = h.out.find("\n  --markers ");
    REQUIRE(year_at != std::string::npos);
    REQUIRE(markers_at != std::string::npos);
    CHECK(year_at < markers_at);
    const std::size_t between = h.out.find("\n  --", year_at + 1);
    CHECK(between == markers_at);
}

TEST_CASE("cli P4-04 10.5 item 3: worlds without history and a malformed pass row, exit 3") {
    Sandbox sb;
    {
        const fs::path db = generate(sb, "until-regions", {"--seed", "1", "--map-size", "512", "--until", "regions"});
        const ProcessResult rep = sb.cli({"report", "--db", u8(db), "--history"});
        REQUIRE(rep.exit_code == EXIT_INPUT);
        for (const std::vector<std::string>& what : {std::vector<std::string>{"--composite", "regions"},
                                                     std::vector<std::string>{"--composite", "map"},
                                                     std::vector<std::string>{"--layer", "elevation"}}) {
            std::vector<std::string> args = {"render", "--db", u8(db)};
            args.insert(args.end(), what.begin(), what.end());
            args.push_back("--markers");
            const ProcessResult r = sb.cli(args);
            CHECK(r.exit_code == EXIT_INPUT);
            CHECK(r.out.empty());
            CHECK(r.err == rep.err);
            // the history before the year range
            args.push_back("--year");
            args.push_back("999999");
            CHECK(sb.cli(args).err == rep.err);
        }
        CHECK(rep.err == "worldgen-cli: error: world file '" + u8(db) +
                             "' has no history (stages: elevation,hydrology,climate,biomes,resources,regions); "
                             "generate it with the history stage\n");
        // without --markers the regions composite still renders
        CHECK(sb.cli({"render", "--db", u8(db), "--composite", "regions"}).exit_code == EXIT_OK);
    }
    {
        // the test pattern: a layer of its own renders, with --markers the history is missing
        const fs::path db = generate(sb, "pattern", {"--seed", "1", "--test-pattern"});
        const ProcessResult rep = sb.cli({"report", "--db", u8(db), "--history"});
        REQUIRE(rep.exit_code == EXIT_INPUT);
        const ProcessResult r = sb.cli({"render", "--db", u8(db), "--layer", "test.ramp", "--markers"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(r.err == rep.err);
        CHECK(starts_with(r.err, "worldgen-cli: error: world file '" + u8(db) + "' has no history"));
        // the composite's own checks come first: no elevation
        const ProcessResult c = sb.cli({"render", "--db", u8(db), "--composite", "map", "--markers"});
        CHECK(c.exit_code == EXIT_INPUT);
        CHECK(starts_with(c.err, "worldgen-cli: error: world file '" + u8(db) + "' has no layer 'elevation'; "));
    }
    {
        // a malformed pass row: the report's message; the marker index is checked before the year range
        const fs::path db = generate(sb, "bad-pass", {"--seed", "42", "--map-size", "512"});
        REQUIRE(db::testing::execute_on_file(u8(db), "UPDATE pass SET kind = 2 WHERE id = 1").ok());
        const ProcessResult rep = sb.cli({"report", "--db", u8(db)});
        REQUIRE(rep.exit_code == EXIT_INPUT);
        REQUIRE(starts_with(rep.err, "worldgen-cli: error: corrupt table 'pass': "));
        for (const std::vector<std::string>& what : {std::vector<std::string>{"--composite", "territory"},
                                                     std::vector<std::string>{"--layer", "elevation"}}) {
            std::vector<std::string> args = {"render", "--db", u8(db)};
            args.insert(args.end(), what.begin(), what.end());
            args.push_back("--markers");
            const ProcessResult r = sb.cli(args);
            CHECK(r.exit_code == EXIT_INPUT);
            CHECK(r.out.empty());
            CHECK(r.err == rep.err);
            args.push_back("--year");
            args.push_back("999999");
            CHECK(sb.cli(args).err == rep.err);
        }
        // the history alone does not read the pass table: territory without --markers renders
        CHECK(sb.cli({"render", "--db", u8(db), "--composite", "territory"}).exit_code == EXIT_OK);
        CHECK_FALSE(file_exists(sb.out_dir() / "bad-pass.territory-6000.markers.png"));
    }
}

}  // TEST_SUITE
