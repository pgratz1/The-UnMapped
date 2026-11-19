// worldgen-cli render and report on geography layers, before any geography
// stage exists: spec P1-00 sections 10.2 and 10.3 with appendix C, P0-04
// amendment 16. Worlds are fixture worlds (write_fixture_world) holding the
// P1-02 5.1 worked example or a square island. Pixel values are tested in
// the library tests; here the command: palette choice, options, errors,
// output paths, PNG headers and report text.
//
// PNG files written by the same binary from the same pixel buffer are
// byte-identical, so comparing two PNG files compares their buffers.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_00_geography_reference.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;
using worldgen::db::Codec;
using worldgen::stages::testing::FixtureLayer;

void check_png(const fs::path& path, std::uint32_t width, std::uint32_t height) {
    const Bytes bytes = read_bytes(path);
    const PngHeader h = png_header(bytes);
    REQUIRE(h.valid);
    CHECK(h.width == width);
    CHECK(h.height == height);
    CHECK(h.bit_depth == 8);
    CHECK(h.colour_type == 2);
}

fs::path worked_world(const Sandbox& sb, const std::string& name, Codec codec = Codec::delta_varint) {
    const fs::path world = sb.file(name);
    REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 0, p100::worked_example_layers(codec)).ok());
    return world;
}

fs::path world_of(const Sandbox& sb, const std::string& name, const std::vector<FixtureLayer>& layers) {
    const fs::path world = sb.file(name);
    REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 0, layers).ok());
    return world;
}

ProcessResult render(const Sandbox& sb, const fs::path& world, std::vector<std::string> extra) {
    std::vector<std::string> args = {"render", "--db", u8(world)};
    args.insert(args.end(), extra.begin(), extra.end());
    return sb.cli(args);
}

// The report without the codec= and bytes= fields of its [layers] lines.
std::string without_codec(const std::string& report) {
    std::string out;
    for (const std::string& l : lines_of(report)) {
        std::string s = l;
        const std::size_t c = s.find(" codec=");
        if (c != std::string::npos) {
            const std::size_t m = s.find(" min=", c);
            REQUIRE(m != std::string::npos);
            s.erase(c, m - c);
        }
        out += s + "\n";
    }
    return out;
}

}  // namespace

TEST_CASE("cli 10.2: render --layer under auto picks the registered palette") {
    Sandbox sb;
    const fs::path world = worked_world(sb, "worked.db");
    struct Case {
        const char* layer;
        const char* palette;
    };
    for (const Case& c : {Case{"elevation", "hypsometric"}, Case{"water", "water"}, Case{"flow_dir", "direction"},
                          Case{"flow_accum", "log2"}, Case{"river", "river"}, Case{"lake", "ids"},
                          Case{"coast_dist", "signed"}, Case{"water_depth", "gray"}}) {
        CAPTURE(c.layer);
        // Default output path and the default palette, auto.
        const ProcessResult r = render(sb, world, {"--layer", c.layer});
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        const fs::path auto_png = sb.out_dir() / (std::string("worked.") + c.layer + ".png");
        CHECK(r.out == printed_path(auto_png));
        check_png(auto_png, 8, 7);
        // The same as the named palette, and as --palette auto.
        const fs::path named = sb.file(std::string(c.layer) + ".named.png");
        REQUIRE(render(sb, world, {"--layer", c.layer, "--palette", c.palette, "--out", u8(named)}).exit_code ==
                EXIT_OK);
        CHECK(read_bytes(named) == read_bytes(auto_png));
        const fs::path explicit_auto = sb.file(std::string(c.layer) + ".auto.png");
        REQUIRE(render(sb, world, {"--layer", c.layer, "--palette", "auto", "--out", u8(explicit_auto)}).exit_code ==
                EXIT_OK);
        CHECK(read_bytes(explicit_auto) == read_bytes(auto_png));
        // And not gray (except for water_depth, whose palette is gray).
        const fs::path gray = sb.file(std::string(c.layer) + ".gray.png");
        REQUIRE(render(sb, world, {"--layer", c.layer, "--palette", "gray", "--out", u8(gray)}).exit_code == EXIT_OK);
        if (std::string_view(c.palette) != "gray") {
            CHECK(read_bytes(gray) != read_bytes(auto_png));
        } else {
            CHECK(read_bytes(gray) == read_bytes(auto_png));
        }
    }
    // gray stays available for every layer, and a named palette that suits
    // another layer works: log2 and ids on flow_dir (u8).
    CHECK(render(sb, world, {"--layer", "flow_dir", "--palette", "log2", "--out", u8(sb.file("a.png"))}).exit_code ==
          EXIT_OK);
    CHECK(render(sb, world, {"--layer", "elevation", "--palette", "signed", "--out", u8(sb.file("b.png"))})
              .exit_code == EXIT_OK);
    // --scale 3 with a non-gray palette.
    REQUIRE(render(sb, world, {"--layer", "elevation", "--scale", "3", "--out", u8(sb.file("e3.png"))}).exit_code ==
            EXIT_OK);
    check_png(sb.file("e3.png"), 24, 21);
}

TEST_CASE("cli appendix C 1: auto falls back to gray when the registered palette does not suit") {
    Sandbox sb;
    std::vector<FixtureLayer> layers;
    layers.push_back({"elevation", p100::grid<std::uint16_t>(3, 1, {1, 200, 3000}), Codec::delta_varint});
    layers.push_back({"river", p100::grid<std::uint8_t>(3, 1, {0, 16, 2}), Codec::delta_varint});
    const fs::path world = world_of(sb, "odd.db", layers);
    for (const char* layer : {"elevation", "river"}) {
        CAPTURE(layer);
        const fs::path a = sb.file(std::string(layer) + ".auto.png");
        const fs::path g = sb.file(std::string(layer) + ".gray.png");
        REQUIRE(render(sb, world, {"--layer", layer, "--out", u8(a)}).exit_code == EXIT_OK);
        REQUIRE(render(sb, world, {"--layer", layer, "--palette", "gray", "--out", u8(g)}).exit_code == EXIT_OK);
        CHECK(read_bytes(a) == read_bytes(g));
    }
}

TEST_CASE("cli appendix C 1: a named palette that does not suit the layer is a usage error") {
    Sandbox sb;
    const fs::path world = worked_world(sb, "worked.db");
    struct Case {
        const char* layer;
        const char* palette;
        const char* need;  // what the message must name
    };
    for (const Case& c : {Case{"water", "hypsometric", "i16"}, Case{"elevation", "water", "u8"},
                          Case{"elevation", "direction", "u8"}, Case{"elevation", "river", "u8"},
                          Case{"elevation", "log2", "unsigned"}, Case{"elevation", "ids", "unsigned"},
                          Case{"flow_accum", "signed", "signed"}, Case{"lake", "water", "u8"}}) {
        CAPTURE(c.layer);
        CAPTURE(c.palette);
        const ProcessResult r = render(sb, world, {"--layer", c.layer, "--palette", c.palette});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(contains(r.err, c.need));
        CHECK(!file_exists(sb.out_dir() / (std::string("worked.") + c.layer + ".png")));
    }
    // river needs every value at most 15.
    std::vector<FixtureLayer> layers;
    layers.push_back({"river", p100::grid<std::uint8_t>(3, 1, {0, 16, 2}), Codec::delta_varint});
    const fs::path odd = world_of(sb, "odd.db", layers);
    const ProcessResult r = render(sb, odd, {"--layer", "river", "--palette", "river"});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(contains(r.err, "15"));
    // Unknown palette names, and biome (deferred, appendix C item 5).
    CHECK(render(sb, world, {"--layer", "elevation", "--palette", "biome"}).exit_code == EXIT_USAGE);
    CHECK(render(sb, world, {"--layer", "elevation", "--palette", "Hypsometric"}).exit_code == EXIT_USAGE);
}

TEST_CASE("cli 10.2: render --composite map") {
    Sandbox sb;
    const fs::path world = worked_world(sb, "worked.db");
    SUBCASE("default output path <root>/out/<stem>.map.png") {
        const ProcessResult r = render(sb, world, {"--composite", "map"});
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        const fs::path expected = sb.out_dir() / "worked.map.png";
        CHECK(r.out == printed_path(expected));
        check_png(expected, 8, 7);
        // Exists now: refused without --force, replaced with it.
        CHECK(render(sb, world, {"--composite", "map"}).exit_code == EXIT_OUTPUT);
        CHECK(render(sb, world, {"--composite", "map", "--force"}).exit_code == EXIT_OK);
    }
    SUBCASE("--scale and --out") {
        const fs::path out = sb.file("m2.png");
        const ProcessResult r = render(sb, world, {"--composite", "map", "--scale", "2", "--out", u8(out)});
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(r.out == printed_path(out));
        check_png(out, 16, 14);
    }
    SUBCASE("elevation only, and with the optional layers") {
        std::vector<FixtureLayer> all = p100::worked_example_layers();
        std::vector<FixtureLayer> elev_only = {all[0]};
        std::vector<FixtureLayer> with_water = {all[0], all[1]};
        std::vector<FixtureLayer> with_river_only = {all[0], all[5]};
        const fs::path w0 = world_of(sb, "e.db", elev_only);
        const fs::path w1 = world_of(sb, "ew.db", with_water);
        const fs::path w2 = world_of(sb, "er.db", with_river_only);
        for (const fs::path& w : {w0, w1, w2}) {
            CAPTURE(u8(w));
            REQUIRE(render(sb, w, {"--composite", "map"}).exit_code == EXIT_OK);
        }
        const Bytes e = read_bytes(sb.out_dir() / "e.map.png");
        const Bytes ew = read_bytes(sb.out_dir() / "ew.map.png");
        const Bytes er = read_bytes(sb.out_dir() / "er.map.png");
        const Bytes full = [&] {
            REQUIRE(render(sb, world, {"--composite", "map"}).exit_code == EXIT_OK);
            return read_bytes(sb.out_dir() / "worked.map.png");
        }();
        // Without flow_dir, no corner closing (P1-00 10.2 step 4): the worked
        // example's river cell (3, 4) steps SE, so the full world paints
        // (4, 4) as well.
        std::vector<FixtureLayer> no_dir;
        for (const FixtureLayer& l : all) {
            if (l.name != "flow_dir") {
                no_dir.push_back(l);
            }
        }
        const fs::path w3 = world_of(sb, "nd.db", no_dir);
        REQUIRE(render(sb, w3, {"--composite", "map"}).exit_code == EXIT_OK);
        CHECK(e != ew);    // water draws the lake and the rivers
        CHECK(ew != full); // river gives the rivers their class colour
        CHECK(er == e);    // river without water is not used
        CHECK(read_bytes(sb.out_dir() / "nd.map.png") != full);  // flow_dir closes the diagonal's corner
    }
}

TEST_CASE("cli appendix C 4: composite and option errors") {
    Sandbox sb;
    const fs::path world = worked_world(sb, "worked.db");
    SUBCASE("exactly one of --layer and --composite") {
        CHECK(render(sb, world, {}).exit_code == EXIT_USAGE);
        CHECK(render(sb, world, {"--layer", "elevation", "--composite", "map"}).exit_code == EXIT_USAGE);
        CHECK(render(sb, world, {"--composite", "atlas"}).exit_code == EXIT_USAGE);
        CHECK(!file_exists(sb.out_dir() / "worked.map.png"));
        CHECK(!file_exists(sb.out_dir() / "worked.elevation.png"));
    }
    SUBCASE("--palette with --composite") {
        for (const char* p : {"auto", "gray", "hypsometric"}) {
            CAPTURE(p);
            const ProcessResult r = render(sb, world, {"--composite", "map", "--palette", p});
            CHECK(r.exit_code == EXIT_USAGE);
            CHECK(r.out.empty());
        }
        CHECK(!file_exists(sb.out_dir() / "worked.map.png"));
    }
    SUBCASE("no elevation: exit 3, listing the layers present") {
        std::vector<FixtureLayer> layers = p100::worked_example_layers();
        layers.erase(layers.begin());
        const fs::path w = world_of(sb, "noelev.db", layers);
        const ProcessResult r = render(sb, w, {"--composite", "map"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(contains(r.err, "elevation"));
        for (const char* name : {"water", "water_depth", "flow_dir", "flow_accum", "river", "lake", "coast_dist"}) {
            CHECK_MESSAGE(contains(r.err, name), name);
        }
        CHECK(!file_exists(sb.out_dir() / "noelev.map.png"));
        // An empty world has no layers at all.
        const fs::path empty = world_of(sb, "empty.db", {});
        CHECK(render(sb, empty, {"--composite", "map"}).exit_code == EXIT_INPUT);
    }
    SUBCASE("elevation of the wrong type, optional layers of the wrong type or size: exit 3") {
        std::vector<FixtureLayer> a;
        a.push_back({"elevation", p100::grid<std::uint16_t>(2, 1, {1, 2}), Codec::delta_varint});
        std::vector<FixtureLayer> b = p100::worked_example_layers();
        b[1].grid = p100::grid<std::uint8_t>(2, 1, {0, 3});  // water of the wrong size
        std::vector<FixtureLayer> c = p100::worked_example_layers();
        c[5].grid = worldgen::db::make_layer_grid(worldgen::db::ElementType::u16, 8, 7);  // river of the wrong type
        std::vector<FixtureLayer> d = p100::worked_example_layers();
        d[1].grid = worldgen::db::make_layer_grid(worldgen::db::ElementType::i16, 8, 7);  // water of the wrong type
        struct Case {
            const char* file;
            const std::vector<FixtureLayer>* layers;
            const char* names;
        };
        for (const Case& k : {Case{"a.db", &a, "elevation"}, Case{"b.db", &b, "water"}, Case{"c.db", &c, "river"},
                              Case{"d.db", &d, "water"}}) {
            CAPTURE(k.file);
            const fs::path w = world_of(sb, k.file, *k.layers);
            const ProcessResult r = render(sb, w, {"--composite", "map"});
            CHECK(r.exit_code == EXIT_INPUT);
            CHECK(contains(r.err, k.names));
        }
    }
}

TEST_CASE("cli 10.3: report on a world with elevation only") {
    Sandbox sb;
    std::vector<FixtureLayer> layers;
    const auto g = p100::grid(p100::SQ, p100::SQ, p100::SQUARE_ELEVATION);
    layers.push_back({"elevation", g, Codec::delta_varint});
    const fs::path world = world_of(sb, "square.db", layers);
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    std::string expected = "# worldgen report v2\n[meta]\n";
    for (const auto& [k, v] : p004::meta_rows(0, {}, "test_fixture")) {
        expected += v.empty() ? k + " =\n" : k + " = " + v + "\n";
    }
    expected += "[tables]\nbiome = 0\nclimate = 0\nlayer = 1\nmeta = 8\npass = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\n[layers]\nelevation i16 5x5 codec=1 bytes=" +
                std::to_string(worldgen::db::encode_layer_blob(g).size()) + " min=-300 max=20000\n";
    expected += p100::with_empty_pass_lines(p100::SQUARE_GEOGRAPHY);
    CHECK(r.out == expected);
    CHECK(!contains(r.out, "lakes ="));
}

TEST_CASE("cli 10.3: report on a world with full hydrology") {
    Sandbox sb;
    const fs::path world = worked_world(sb, "worked.db");
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    const std::string geo = p100::with_empty_pass_lines(p100::worked_geography());
    REQUIRE(r.out.size() > geo.size());
    CHECK(r.out.substr(r.out.size() - geo.size()) == geo);
    CHECK(starts_with(r.out, "# worldgen report v2\n[meta]\n"));
    // --out writes the same bytes.
    const fs::path out = sb.file("report.txt");
    const ProcessResult o = sb.cli({"report", "--db", u8(world), "--out", u8(out)});
    REQUIRE(o.exit_code == EXIT_OK);
    CHECK(o.out.empty());
    CHECK(read_text(out) == r.out);
}

TEST_CASE("cli appendix C 9: report exits 3 on unusable geography layers") {
    Sandbox sb;
    SUBCASE("a flow cycle") {
        std::vector<FixtureLayer> layers = p100::worked_example_layers();
        std::get<std::vector<std::uint8_t>>(layers[3].grid.values)[3 * 8 + 6] = 5;  // (6,3) NW -> (5,2) -> (6,3)
        const fs::path w = world_of(sb, "cycle.db", layers);
        const ProcessResult r = sb.cli({"report", "--db", u8(w)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(contains(r.err, "flow_dir"));
        CHECK(contains(r.err, "(5, 2)"));
    }
    SUBCASE("a step off the map") {
        // A land cell on the map edge: (0, 3) set to e 10 with flow_dir W.
        std::vector<FixtureLayer> layers = p100::worked_example_layers();
        std::get<std::vector<std::int16_t>>(layers[0].grid.values)[3 * 8 + 0] = 10;
        std::get<std::vector<std::uint8_t>>(layers[3].grid.values)[3 * 8 + 0] = 4;
        const fs::path w = world_of(sb, "off.db", layers);
        const ProcessResult r = sb.cli({"report", "--db", u8(w)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(contains(r.err, "flow_dir"));
        CHECK(contains(r.err, "(0, 3)"));
    }
    SUBCASE("code 8 on land") {
        std::vector<FixtureLayer> layers = p100::worked_example_layers();
        // (6,5) gets code 8. (5,4) drains SE into it, so the lowest cell that
        // does not reach the ocean is (5,4), index 37, not (6,5), index 46.
        std::get<std::vector<std::uint8_t>>(layers[3].grid.values)[5 * 8 + 6] = 8;
        const fs::path w = world_of(sb, "code8.db", layers);
        const ProcessResult r = sb.cli({"report", "--db", u8(w)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(contains(r.err, "flow_dir"));
        CHECK(contains(r.err, "(5, 4)"));
    }
    SUBCASE("wrong types") {
        std::vector<FixtureLayer> a;
        a.push_back({"elevation", p100::grid<std::int32_t>(2, 1, {1, -2}), Codec::delta_varint});
        std::vector<FixtureLayer> b = p100::worked_example_layers();
        b[4].grid = worldgen::db::make_layer_grid(worldgen::db::ElementType::u16, 8, 7);  // flow_accum
        const ProcessResult ra = sb.cli({"report", "--db", u8(world_of(sb, "a.db", a))});
        CHECK(ra.exit_code == EXIT_INPUT);
        CHECK(contains(ra.err, "elevation"));
        const ProcessResult rb = sb.cli({"report", "--db", u8(world_of(sb, "b.db", b))});
        CHECK(rb.exit_code == EXIT_INPUT);
        CHECK(contains(rb.err, "flow_accum"));
    }
    SUBCASE("a bad hydrology layer is not checked when one of the five is absent") {
        std::vector<FixtureLayer> layers = p100::worked_example_layers();
        layers[4].grid = worldgen::db::make_layer_grid(worldgen::db::ElementType::u16, 8, 7);  // flow_accum
        layers.erase(layers.begin() + 6);                                                      // no lake
        const ProcessResult r = sb.cli({"report", "--db", u8(world_of(sb, "nolake.db", layers))});
        CHECK(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, std::string(p100::WORKED_GEOGRAPHY_ELEVATION_LINES)));
        CHECK(!contains(r.out, "lakes ="));
    }
}

TEST_CASE("cli: report text and render buffers do not depend on the layers' codec") {
    Sandbox sb;
    const fs::path raw = worked_world(sb, "raw.db", Codec::raw);
    const fs::path varint = worked_world(sb, "varint.db", Codec::delta_varint);
    const ProcessResult a = sb.cli({"report", "--db", u8(raw)});
    const ProcessResult b = sb.cli({"report", "--db", u8(varint)});
    REQUIRE(a.exit_code == EXIT_OK);
    REQUIRE(b.exit_code == EXIT_OK);
    CHECK(contains(a.out, "\nelevation i16 8x7 codec=0 "));
    CHECK(contains(b.out, "\nelevation i16 8x7 codec=1 "));
    CHECK(without_codec(a.out) == without_codec(b.out));
    for (const char* layer : {"elevation", "water", "water_depth", "flow_dir", "flow_accum", "river", "lake",
                              "coast_dist"}) {
        CAPTURE(layer);
        REQUIRE(render(sb, raw, {"--layer", layer}).exit_code == EXIT_OK);
        REQUIRE(render(sb, varint, {"--layer", layer}).exit_code == EXIT_OK);
        CHECK(read_bytes(sb.out_dir() / (std::string("raw.") + layer + ".png")) ==
              read_bytes(sb.out_dir() / (std::string("varint.") + layer + ".png")));
    }
    REQUIRE(render(sb, raw, {"--composite", "map"}).exit_code == EXIT_OK);
    REQUIRE(render(sb, varint, {"--composite", "map"}).exit_code == EXIT_OK);
    CHECK(read_bytes(sb.out_dir() / "raw.map.png") == read_bytes(sb.out_dir() / "varint.map.png"));
}
