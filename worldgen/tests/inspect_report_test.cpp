// The text report builder, spec P0-04 sections 7 and 12.4, with the
// amendments of section 14. Format v2 (P0-04 amendment 16, P1-00 10.3):
// the header line is "# worldgen report v2"; the v1 sections are unchanged.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using worldgen::db::Codec;
using worldgen::db::ErrorCode;
using worldgen::db::Overwrite;
using worldgen::db::WorldReader;
using worldgen::inspect::build_report;
using worldgen::inspect::escape_report_text;

WorldReader open_world(const std::string& path) {
    auto r = WorldReader::open(path);
    INFO("open: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return std::move(r).value();
}

std::string report_of(const std::string& path) {
    const WorldReader w = open_world(path);
    auto r = build_report(w);
    INFO("report: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return r.value();
}

worldgen::WorldParams params_for(std::uint64_t seed, bool pattern) {
    worldgen::WorldParams p;
    p.seed = seed;
    p.content_hash = worldgen::sha256(std::string_view{});
    p.test_pattern = pattern;
    if (!pattern) {
        p.until = std::string("none");  // the P0 empty world (P1-00 10.1)
    }
    return p;
}

bool contains_text(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("report 7: escaping") {
    using namespace std::string_view_literals;
    CHECK(worldgen::inspect::REPORT_HEADER_V1 == "# worldgen report v1");
    CHECK(worldgen::inspect::REPORT_HEADER_V2 == "# worldgen report v2");
    CHECK(escape_report_text("") == "");
    CHECK(escape_report_text("plain text 123") == "plain text 123");
    CHECK(escape_report_text("\\") == "\\\\");
    CHECK(escape_report_text("a\\b\tc") == "a\\\\b\\x09c");
    CHECK(escape_report_text("\x00"sv) == "\\x00");
    CHECK(escape_report_text("\n\r\x1F") == "\\x0A\\x0D\\x1F");
    CHECK(escape_report_text("\x7F") == "\\x7F");
    CHECK(escape_report_text("\x1B[0m") == "\\x1B[0m");
    // Every other byte, including UTF-8 sequences and lone high bytes, is
    // copied unchanged.
    CHECK(escape_report_text("\xC3\x9Cnmapped W\xC3\xB6rld") == "\xC3\x9Cnmapped W\xC3\xB6rld");
    CHECK(escape_report_text("\x80\xFF ~") == "\x80\xFF ~");
    // Every control byte, exhaustively.
    for (int b = 0; b < 256; ++b) {
        const std::string in(1, static_cast<char>(b));
        const std::string out = escape_report_text(in);
        if (b < 0x20 || b == 0x7F) {
            static constexpr char HEX[] = "0123456789ABCDEF";
            CHECK(out == std::string("\\x") + HEX[b >> 4] + HEX[b & 15]);
        } else if (b == '\\') {
            CHECK(out == "\\\\");
        } else {
            CHECK(out == in);
        }
    }
}

TEST_CASE("report 7, 12.4: empty world, seed 0, exact text") {
    dbtest::TempDir dir("p004-report-empty");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
    CHECK(report_of(out) == p004::REPORT_SEED_0);
}

// P0-04 12.4 and amendment 15: a scrubbed phrase holds no backslash or
// control byte, so the escaping is checked on a test-only world: the empty
// seed-0 world with one extra meta row added through the test hook. The
// reader tolerates unknown meta keys (P0-02 5.5), and the report prints every
// meta row in key order (P0-04 7).
namespace {

std::string report_with_extra_meta(const dbtest::TempDir& dir, const std::string& name, const std::string& sql_value) {
    const std::string out = dir.file(name);
    REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
    const auto s = worldgen::db::testing::execute_on_file(
        out, "INSERT INTO meta (key, value) VALUES ('zz.test_value', " + sql_value + ")");
    REQUIRE_MESSAGE(s.ok(), (s.ok() ? std::string() : s.error().message));
    return report_of(out);
}

std::string seed_0_report_with_line(const std::string& line) {
    return "# worldgen report v2\n[meta]\n"
           "content_hash = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
           "format = the-unmapped-world\n"
           "generator_version = 0.16.0\n"
           "history_end_year = 6000\n"
           "param.civs_per_race = auto\n"
           "param.divine_activity = normal\n"
           "param.divine_script =\n"
           "param.event_intensity = normal\n"
           "param.history_detail = normal\n"
           "param.history_span = 6000\n"
           "param.map_size = 1024\n"
           "param.monster_pressure = normal\n"
           "param.races = elf,goblin,dwarf,faerie,human,orc\n"
           "schema_version = 7\n"
           "seed = 0\n"
           "seed_phrase =\n"
           "stages =\n" +
           line +
           "[tables]\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 0\nmeta = 18\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\n[layers]\n(none)\n";
}

}  // namespace

TEST_CASE("report 12.4: a meta value with a backslash and a tab (test-only world, amendment 15)") {
    dbtest::TempDir dir("p004-report-escape");
    const std::string r = report_with_extra_meta(dir, "w.db", "'a\\b' || char(9) || 'c'");
    CHECK(contains_text(r, "\nzz.test_value = a\\\\b\\x09c\n"));
    CHECK(r == seed_0_report_with_line("zz.test_value = a\\\\b\\x09c\n"));
}

TEST_CASE("report 12.4: other control bytes and UTF-8 in a meta value (test-only world, amendment 15)") {
    dbtest::TempDir dir("p004-report-escape2");
    // x 0x01 y 0x1F 0x7F "Ünmapped Wörld" LF z CR
    const std::string r = report_with_extra_meta(
        dir, "w.db",
        "'x' || char(1) || 'y' || char(31) || char(127) || CAST(X'C39C6E6D61707065642057C3B6726C64' AS TEXT) || "
        "char(10) || 'z' || char(13)");
    const std::string escaped = "x\\x01y\\x1F\\x7F\xC3\x9Cnmapped W\xC3\xB6rld\\x0Az\\x0D";
    CHECK(r == seed_0_report_with_line("zz.test_value = " + escaped + "\n"));
    CHECK(r.find('\r') == std::string::npos);
}

TEST_CASE("report 7: test-pattern world") {
    dbtest::TempDir dir("p004-report-pattern");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(42, true), out, Overwrite::refuse).ok());
    auto line = [](const std::string& name, const char* format, const worldgen::db::LayerGrid& grid,
                   const std::string& lo, const std::string& hi) {
        return name + " " + format + " 256x256 codec=1 bytes=" +
               std::to_string(worldgen::db::encode_layer_blob(grid, Codec::delta_varint).size()) + " min=" + lo +
               " max=" + hi + "\n";
    };
    const auto hash = p004::hash_cells(42);
    const auto noise = p004::noise_cells(42);
    const auto [hlo, hhi] = std::minmax_element(hash.begin(), hash.end());
    const auto [nlo, nhi] = std::minmax_element(noise.begin(), noise.end());
    const std::string expected =
        "# worldgen report v2\n[meta]\n"
        "content_hash = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
        "format = the-unmapped-world\n"
        "generator_version = 0.16.0\n"
        "history_end_year = 6000\n"
        "param.civs_per_race = auto\n"
        "param.divine_activity = normal\n"
        "param.divine_script =\n"
        "param.event_intensity = normal\n"
        "param.history_detail = normal\n"
        "param.history_span = 6000\n"
        "param.map_size = 1024\n"
        "param.monster_pressure = normal\n"
        "param.races = elf,goblin,dwarf,faerie,human,orc\n"
        "schema_version = 7\n"
        "seed = 42\n"
        "seed_phrase =\n"
        "stages = test_pattern\n"
        "[tables]\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 3\nmeta = 17\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\n[layers]\n" +
        line("test.ramp", "u8", p004::grid_of(256, 256, p004::ramp_cells()), "0", "255") +
        line("test.hash", "u8", p004::grid_of(256, 256, hash), std::to_string(int{*hlo}), std::to_string(int{*hhi})) +
        line("test.noise", "i16", p004::grid_of(256, 256, noise), std::to_string(int{*nlo}), std::to_string(int{*nhi}));
    CHECK(report_of(out) == expected);
}

TEST_CASE("report 7: layer lines for every element type, codec 0, extreme values") {
    dbtest::TempDir dir("p004-report-types");
    using worldgen::stages::testing::FixtureLayer;
    std::vector<FixtureLayer> layers;
    layers.push_back({"test.u64", p004::grid_of<std::uint64_t>(2, 1, {0, std::numeric_limits<std::uint64_t>::max()}),
                      Codec::raw});
    layers.push_back({"test.i64",
                      p004::grid_of<std::int64_t>(1, 2, {std::numeric_limits<std::int64_t>::min(),
                                                         std::numeric_limits<std::int64_t>::max()}),
                      Codec::raw});
    layers.push_back({"test.i8", p004::grid_of<std::int8_t>(3, 1, {-5, -5, -5}), Codec::raw});
    layers.push_back({"test.u32", p004::grid_of<std::uint32_t>(1, 1, {4000000000U}), Codec::delta_varint});
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 3, layers).ok());
    const std::string report = report_of(out);
    const std::string expected_tail = "[tables]\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 4\nmeta = 17\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\n[layers]\n"
                                      "test.u64 u64 2x1 codec=0 bytes=40 min=0 max=18446744073709551615\n"
                                      "test.i64 i64 1x2 codec=0 bytes=40 min=-9223372036854775808 "
                                      "max=9223372036854775807\n"
                                      "test.i8 i8 3x1 codec=0 bytes=27 min=-5 max=-5\n"
                                      "test.u32 u32 1x1 codec=1 bytes=" +
                                      std::to_string(worldgen::db::encode_layer_blob(layers[3].grid).size()) +
                                      " min=4000000000 max=4000000000\n";
    REQUIRE(report.size() > expected_tail.size());
    CHECK(report.substr(report.size() - expected_tail.size()) == expected_tail);
    CHECK(report.find("\nstages = test_fixture\n") != std::string::npos);
}

TEST_CASE("report 7: sqlite_* tables are excluded, other tables listed by name") {
    dbtest::TempDir dir("p004-report-tables");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
    REQUIRE(worldgen::db::testing::execute_on_file(out, "ANALYZE").ok());
    CHECK(report_of(out) == p004::REPORT_SEED_0);
    REQUIRE(worldgen::db::testing::execute_on_file(
                out, "CREATE TABLE aaa (id INTEGER PRIMARY KEY) STRICT; INSERT INTO aaa VALUES (1), (2), (3)")
                .ok());
    const std::string report = report_of(out);
    CHECK(report.find("[tables]\naaa = 3\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 0\nmeta = 17\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\n[layers]\n") != std::string::npos);
}

TEST_CASE("report 7: a corrupt layer is a corrupt_layer error") {
    dbtest::TempDir dir("p004-report-corrupt");
    worldgen::db::WorldWriter writer;
    dbtest::set_standard_meta(writer, 0);
    {
        worldgen::db::StageWriter s = writer.begin_stage("broken");
        const std::vector<std::uint8_t> junk = {1, 2, 3};
        worldgen::db::testing::WriterAccess::insert_layer_row(s, 1, "test.junk", 1, 1, "u8", junk);
        s.commit();
    }
    const std::string out = dir.file("w.db");
    REQUIRE(writer.finalize(out, Overwrite::refuse).ok());
    const WorldReader w = open_world(out);
    const auto r = build_report(w);
    REQUIRE(!r.ok());
    CHECK(r.code() == ErrorCode::corrupt_layer);
}
