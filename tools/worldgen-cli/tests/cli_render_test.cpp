// worldgen-cli render, spec P0-04 sections 6 and 12.3, with the amendments
// of section 14. The pixel buffer itself (render_rgb) is tested in the
// library tests (worldgen/tests/inspect_render_test.cpp); here the command:
// paths, options, errors and the PNG header.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;

void check_png(const fs::path& path, std::uint32_t width, std::uint32_t height) {
    const Bytes bytes = read_bytes(path);
    const PngHeader h = png_header(bytes);
    REQUIRE(h.valid);
    CHECK(h.width == width);
    CHECK(h.height == height);
    CHECK(h.bit_depth == 8);
    CHECK(h.colour_type == 2);  // RGB, no alpha
    // No time, text or gamma chunks (6).
    const std::vector<std::string> chunks = png_chunk_types(bytes);
    for (const char* banned : {"tIME", "tEXt", "zTXt", "iTXt", "gAMA"}) {
        CHECK_MESSAGE(std::find(chunks.begin(), chunks.end(), banned) == chunks.end(), "chunk ", banned);
    }
    REQUIRE(!chunks.empty());
    CHECK(chunks.front() == "IHDR");
    CHECK(chunks.back() == "IEND");
}

}  // namespace

TEST_CASE("cli 12.3: render a test-pattern layer") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern", "--out", u8(sb.file("pattern.db"))});

    SUBCASE("default output path <root>/out/<stem>.<layer>.png") {
        for (const char* layer : {"test.ramp", "test.hash", "test.noise"}) {
            CAPTURE(layer);
            const ProcessResult r = sb.cli({"render", "--db", u8(world), "--layer", layer});
            INFO("stderr: ", r.err);
            REQUIRE(r.exit_code == EXIT_OK);
            const fs::path expected = sb.out_dir() / (std::string("pattern.") + layer + ".png");
            CHECK(r.out == printed_path(expected));
            check_png(expected, 256, 256);
        }
    }
    SUBCASE("--scale 3 and explicit --out") {
        const fs::path out = sb.file("ramp3.png");
        const ProcessResult r =
            sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--scale", "3", "--out", u8(out)});
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(r.out == printed_path(out));
        check_png(out, 768, 768);
    }
    SUBCASE("--palette gray and auto give the same image") {
        const fs::path a = sb.file("auto.png");
        const fs::path g = sb.file("gray.png");
        REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.hash", "--out", u8(a)}).exit_code == EXIT_OK);
        REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.hash", "--palette", "gray", "--out", u8(g)})
                    .exit_code == EXIT_OK);
        REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.hash", "--palette", "auto", "--out",
                        u8(sb.file("auto2.png"))})
                    .exit_code == EXIT_OK);
        // Same writer, same pixels: the files are equal too.
        CHECK(read_bytes(a) == read_bytes(g));
        CHECK(read_bytes(a) == read_bytes(sb.file("auto2.png")));
    }
    SUBCASE("bad options") {
        for (const char* scale : {"0", "17", "03", "-1", "", "1.5", "x"}) {
            CAPTURE(scale);
            const ProcessResult r = sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--scale", scale});
            CHECK(r.exit_code == EXIT_USAGE);
        }
        CHECK(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--palette", "rainbow"}).exit_code ==
              EXIT_USAGE);
        CHECK(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--palette", "Gray"}).exit_code ==
              EXIT_USAGE);
        const bool nothing_written = !file_exists(sb.out_dir()) || fs::is_empty(sb.out_dir());
        CHECK(nothing_written);
    }
    SUBCASE("existing output without --force") {
        const fs::path out = sb.file("r.png");
        write_text(out, "old");
        CHECK(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--out", u8(out)}).exit_code ==
              EXIT_OUTPUT);
        CHECK(read_text(out) == "old");
        CHECK(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--out", u8(out), "--force"}).exit_code ==
              EXIT_OK);
        check_png(out, 256, 256);
        CHECK(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--out", u8(sb.file("no/dir/r.png"))})
                  .exit_code == EXIT_OUTPUT);
    }
}

TEST_CASE("cli 12.3: unknown layer is an input error listing the layer names") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern", "--out", u8(sb.file("pattern.db"))});
    const ProcessResult r = sb.cli({"render", "--db", u8(world), "--layer", "test.nothing"});
    CHECK(r.exit_code == EXIT_INPUT);
    CHECK(r.out.empty());
    CHECK(starts_with(r.err, "worldgen-cli: error: "));
    CHECK(contains(r.err, "test.nothing"));
    for (const char* name : {"test.ramp", "test.hash", "test.noise"}) {
        CHECK_MESSAGE(contains(r.err, name), name);
    }
    CHECK(!file_exists(sb.out_dir() / "pattern.test.nothing.png"));

    // An empty world has no layers at all.
    const fs::path empty = sb.generate({"--seed", "0", "--out", u8(sb.file("empty.db"))});
    CHECK(sb.cli({"render", "--db", u8(empty), "--layer", "test.ramp"}).exit_code == EXIT_INPUT);
    // A file that is not a world.
    write_text(sb.file("text.db"), "not a world\n");
    CHECK(sb.cli({"render", "--db", u8(sb.file("text.db")), "--layer", "test.ramp"}).exit_code == EXIT_INPUT);
}

TEST_CASE("cli 12.3: fixture layers render (constant and synthetic i16)") {
    Sandbox sb;
    using worldgen::stages::testing::FixtureLayer;
    std::vector<FixtureLayer> layers;
    layers.push_back({"test.constant", p004::grid_of<std::uint16_t>(5, 3, std::vector<std::uint16_t>(15, std::uint16_t{7})),
                      worldgen::db::Codec::delta_varint});
    layers.push_back({"test.signed", p004::grid_of<std::int16_t>(3, 1, {-32767, 0, 32767}), worldgen::db::Codec::raw});
    const fs::path world = sb.file("fixture.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 0, layers).ok());

    REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.constant", "--scale", "2"}).exit_code == EXIT_OK);
    check_png(sb.out_dir() / "fixture.test.constant.png", 10, 6);
    REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.signed"}).exit_code == EXIT_OK);
    check_png(sb.out_dir() / "fixture.test.signed.png", 3, 1);
    // The largest scale (6: 1 to 16).
    REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.signed", "--scale", "16", "--out",
                    u8(sb.file("s16.png"))})
                .exit_code == EXIT_OK);
    check_png(sb.file("s16.png"), 48, 16);
}
