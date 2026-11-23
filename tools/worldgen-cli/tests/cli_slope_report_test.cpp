// worldgen-cli report with the step, slope and edge-run statistics of
// [geography] (spec P1-00 section 10.3, the keys added 2026-11-15; appendix
// C item 20), and two items owed from earlier work: generate --data "" is a
// usage error (P0-03 7.6, P1-00 appendix C item 16, data_root.hpp), and
// render --composite map refuses an image above 1 GiB (P0-04 amendment 7).
//
// Written from the specs, the public headers and the reference worlds of
// p1_00_geography_reference.hpp only.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
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

}  // namespace

TEST_CASE("cli 10.3: report on the slope world, byte for byte") {
    Sandbox sb;
    const auto g = p100::grid(p100::SLW, p100::SLH, p100::SLOPE_ELEVATION);
    std::vector<FixtureLayer> layers;
    layers.push_back({"elevation", g, Codec::delta_varint});
    const fs::path world = sb.file("slope.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 0, layers).ok());
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    std::string expected = "# worldgen report v2\n[meta]\n";
    for (const auto& [k, v] : p004::meta_rows(0, {}, "test_fixture")) {
        expected += v.empty() ? k + " =\n" : k + " = " + v + "\n";
    }
    expected += "[tables]\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 1\nmeta = 17\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\n[layers]\nelevation i16 7x6 codec=1 bytes=" +
                std::to_string(worldgen::db::encode_layer_blob(g).size()) + " min=-40 max=5400\n" +
                p100::with_empty_pass_lines(p100::SLOPE_GEOGRAPHY);
    CHECK(r.out == expected);
    CHECK(r.err.empty());
    // The format stays v2 (appendix C item 20).
    CHECK(starts_with(r.out, "# worldgen report v2\n"));
}

TEST_CASE("cli P0-03 7.6, appendix C 16: generate --data \"\" is a usage error") {
    Sandbox sb;
    auto refused = [&](const std::vector<std::string>& args, const RunOptions& o, const fs::path& would_write) {
        const ProcessResult r = sb.cli(args, o);
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
        CHECK(contains(r.err, "--data"));
        CHECK(!file_exists(would_write));
        CHECK(partial_files(sb.root()).empty());
        CHECK(partial_files(sb.files()).empty());
    };
    const RunOptions plain;
    // With --out, and with the default output path under root/out/.
    refused({"generate", "--seed", "0", "--until", "none", "--data", "", "--out", u8(sb.file("w.db"))}, plain,
            sb.file("w.db"));
    refused({"generate", "--seed", "3", "--data", ""}, plain, sb.out_dir() / "seed-3.db");
    // Whether or not UNMAPPED_DATA_DIR is set: an empty flag never falls back.
    RunOptions env;
    env.env.push_back({"UNMAPPED_DATA_DIR", u8(sb.file("data"))});
    refused({"generate", "--seed", "0", "--data", "", "--out", u8(sb.file("e.db"))}, env, sb.file("e.db"));
    RunOptions unset;
    unset.env.push_back({"UNMAPPED_DATA_DIR", std::nullopt});
    refused({"generate", "--seed", "0", "--data", "", "--out", u8(sb.file("u.db"))}, unset, sb.file("u.db"));
    // Also with --test-pattern and --force over an existing file, which stays.
    write_text(sb.file("keep.db"), "keep");
    refused({"generate", "--seed", "0", "--test-pattern", "--force", "--data", "", "--out", u8(sb.file("x.db"))},
            plain, sb.file("x.db"));
    const ProcessResult k =
        sb.cli({"generate", "--seed", "0", "--force", "--data", "", "--out", u8(sb.file("keep.db"))});
    CHECK(k.exit_code == EXIT_USAGE);
    CHECK(read_text(sb.file("keep.db")) == "keep");
    const bool out_empty = !file_exists(sb.out_dir()) || fs::is_empty(sb.out_dir());
    CHECK(out_empty);
}

TEST_CASE("cli P0-04 amendment 7: render --composite map refuses an image above 1 GiB") {
    // A wide, short elevation layer: 65536 x 22 = 1441792 cells (2.9 MB as
    // i16, a few bytes encoded: one value and a zero-delta run). At --scale 16
    // the RGB data would be 3 x (65536 x 16) x (22 x 16) = 1107296256 bytes >
    // 2^30 = 1073741824, so render must refuse before allocating it. (At
    // --scale 15 it would be 973209600 bytes, under the limit: not run, it
    // would allocate about 1 GB.)
    Sandbox sb;
    const fs::path world = sb.file("wide.db");
    {
        std::vector<FixtureLayer> layers;
        layers.push_back(
            {"elevation", p100::grid(65536, 22, std::vector<std::int16_t>(std::size_t{65536} * 22, -5)),
             Codec::delta_varint});
        REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 0, layers).ok());
    }
    {
        const ProcessResult r = sb.cli({"render", "--db", u8(world), "--composite", "map", "--scale", "16"});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
        CHECK(contains(r.err, "--scale"));
        CHECK(!file_exists(sb.out_dir() / "wide.map.png"));
    }
    const ProcessResult o = sb.cli(
        {"render", "--db", u8(world), "--composite", "map", "--scale", "16", "--force", "--out", u8(sb.file("m.png"))});
    CHECK(o.exit_code == EXIT_USAGE);
    CHECK(!file_exists(sb.file("m.png")));
    // The same layer at scale 1 is a small image (65536 x 22 x 3 = 4.3 MB):
    // the composite is drawn.
    const ProcessResult ok = sb.cli({"render", "--db", u8(world), "--composite", "map", "--out", u8(sb.file("s1.png"))});
    INFO("stderr: ", ok.err);
    CHECK(ok.exit_code == EXIT_OK);
    const PngHeader h = png_header(read_bytes(sb.file("s1.png")));
    CHECK(h.valid);
    CHECK(h.width == 65536U);
    CHECK(h.height == 22U);
}
