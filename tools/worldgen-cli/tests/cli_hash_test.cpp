// worldgen-cli hash, spec P0-04 sections 8, 10 and 12.5, with the
// amendments of section 14. Expected logical hashes are built by hand from
// the encodings of section 8 (p0_04_reference.hpp).

#include <doctest/doctest.h>

#include <cctype>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;
using worldgen::to_hex;

std::string upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

p004::LayerRow layer_row(std::int64_t id, const std::string& name, const char* format,
                         const worldgen::db::LayerGrid& grid) {
    return {id, name, 256, 256, format, worldgen::db::encode_layer_blob(grid, worldgen::db::Codec::delta_varint)};
}

}  // namespace

TEST_CASE("cli 12.5: hash equals SHA-256 of the file bytes") {
    Sandbox sb;
    const fs::path empty_world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("empty.db"))});
    const fs::path pattern_world = sb.generate({"--seed", "42", "--test-pattern", "--out", u8(sb.file("p.db"))});
    write_text(sb.file("zero.bin"), "");
    write_text(sb.file("abc.txt"), "abc");
    for (const fs::path& p : {empty_world, pattern_world, sb.file("zero.bin"), sb.file("abc.txt")}) {
        CAPTURE(u8(p));
        const ProcessResult r = sb.cli({"hash", "--db", u8(p)});
        CHECK(r.exit_code == EXIT_OK);
        CHECK(r.out == worldgen::sha256_hex(read_bytes(p)) + "\n");
        CHECK(r.err.empty());
        CHECK(r.out.find('\r') == std::string::npos);
#if !defined(_WIN32)
        // On Linux also against the standard tool, where installed.
        const ProcessResult tool = sb.tool("sha256sum", {u8(p)});
        if (tool.exit_code == 0) {
            CHECK(tool.out.substr(0, 65) == r.out.substr(0, 64) + " ");
        } else {
            MESSAGE("sha256sum not available; skipped the comparison");
        }
#endif
    }
    // FIPS 180-4 vectors through the file path.
    CHECK(sb.cli({"hash", "--db", u8(sb.file("zero.bin"))}).out == std::string(EMPTY_SHA256) + "\n");
    CHECK(sb.cli({"hash", "--db", u8(sb.file("abc.txt"))}).out ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n");
    // A directory is not a readable file.
    CHECK(sb.cli({"hash", "--db", u8(sb.files())}).exit_code == EXIT_INPUT);
}

TEST_CASE("cli 12.5: a non-world file hashes without --detail and fails with it") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern", "--out", u8(sb.file("w.db"))});
    const fs::path png = sb.file("ramp.png");
    REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--out", u8(png)}).exit_code == EXIT_OK);
    const ProcessResult plain = sb.cli({"hash", "--db", u8(png)});
    CHECK(plain.exit_code == EXIT_OK);
    CHECK(plain.out == file_sha256_hex(png) + "\n");
    const ProcessResult detail = sb.cli({"hash", "--db", u8(png), "--detail"});
    CHECK(detail.exit_code == EXIT_INPUT);
    CHECK(starts_with(detail.err, "worldgen-cli: error: "));
}

TEST_CASE("cli 12.5: --detail of the empty world") {
    Sandbox sb;
    const fs::path world = sb.generate({"--until", "none", "--seed", "0"});
    const ProcessResult r = sb.cli({"hash", "--db", u8(world), "--detail"});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    CHECK(r.out.find('\r') == std::string::npos);
    const std::vector<std::string> lines = lines_of(r.out);
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "file " + file_sha256_hex(world));
    CHECK(lines[1] == "schema " + to_hex(p004::p0_schema_hash()));
    CHECK(lines[2] == "table layer " + to_hex(p004::layer_table_hash({})));
    CHECK(lines[3] == "table meta " + to_hex(p004::meta_table_hash(cli_meta_rows(0))));
    CHECK(r.out == lines[0] + "\n" + lines[1] + "\n" + lines[2] + "\n" + lines[3] + "\n");
    MESSAGE("empty seed-0 --detail:\n", r.out);
}

TEST_CASE("cli 12.5: --detail of a test-pattern world") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "42", "--test-pattern"});
    const ProcessResult r = sb.cli({"hash", "--db", u8(world), "--detail"});
    REQUIRE(r.exit_code == EXIT_OK);
    const std::vector<std::string> lines = lines_of(r.out);
    REQUIRE(lines.size() == 7);
    const auto ramp = p004::ramp_cells();
    const auto hash = p004::hash_cells(42);
    const auto noise = p004::noise_cells(42);
    CHECK(lines[0] == "file " + file_sha256_hex(world));
    CHECK(lines[1] == "schema " + to_hex(p004::p0_schema_hash()));
    const std::vector<p004::LayerRow> rows = {
        layer_row(1, "test.ramp", "u8", p004::grid_of(256, 256, ramp)),
        layer_row(2, "test.hash", "u8", p004::grid_of(256, 256, hash)),
        layer_row(3, "test.noise", "i16", p004::grid_of(256, 256, noise)),
    };
    CHECK(lines[2] == "table layer " + to_hex(p004::layer_table_hash(rows)));
    CHECK(lines[3] == "table meta " + to_hex(p004::meta_table_hash(cli_meta_rows(42, "", "test_pattern"))));
    CHECK(lines[4] == "layer test.ramp " + to_hex(p004::layer_hash_u8(256, 256, ramp)));
    CHECK(lines[5] == "layer test.hash " + to_hex(p004::layer_hash_u8(256, 256, hash)));
    CHECK(lines[6] == "layer test.noise " + to_hex(p004::layer_hash_i16(256, 256, noise)));
}

TEST_CASE("cli 12.5: layer hash is codec-independent") {
    Sandbox sb;
    using worldgen::stages::testing::FixtureLayer;
    std::vector<std::int16_t> cells(7 * 5);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        cells[i] = static_cast<std::int16_t>(static_cast<int>(i * 997 % 2001) - 1000);
    }
    std::vector<FixtureLayer> layers;
    layers.push_back({"test.raw", p004::grid_of(7, 5, cells), worldgen::db::Codec::raw});
    layers.push_back({"test.varint", p004::grid_of(7, 5, cells), worldgen::db::Codec::delta_varint});
    const fs::path world = sb.file("fixture.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 9, layers).ok());
    const ProcessResult r = sb.cli({"hash", "--db", u8(world), "--detail"});
    REQUIRE(r.exit_code == EXIT_OK);
    const std::vector<std::string> lines = lines_of(r.out);
    REQUIRE(lines.size() == 6);
    const std::string expected = to_hex(p004::layer_hash_i16(7, 5, cells));
    CHECK(lines[4] == "layer test.raw " + expected);
    CHECK(lines[5] == "layer test.varint " + expected);
    CHECK(lines[3] == "table meta " + to_hex(p004::meta_table_hash(p004::meta_rows(9, "", "test_fixture"))));
}

TEST_CASE("cli 12.5: --expect") {
    Sandbox sb;
    const fs::path world = sb.generate({"--until", "none", "--seed", "1"});
    const std::string hex = file_sha256_hex(world);
    const std::string plain = hex + "\n";

    const ProcessResult lower = sb.cli({"hash", "--db", u8(world), "--expect", hex});
    CHECK(lower.exit_code == EXIT_OK);
    CHECK(lower.out == plain);
    CHECK(lower.err.empty());
    const ProcessResult up = sb.cli({"hash", "--db", u8(world), "--expect", upper(hex)});
    CHECK(up.exit_code == EXIT_OK);
    CHECK(up.out == plain);

    std::string wrong = hex;
    wrong[0] = wrong[0] == 'a' ? 'b' : 'a';
    const ProcessResult mismatch = sb.cli({"hash", "--db", u8(world), "--expect", upper(wrong)});
    CHECK(mismatch.exit_code == EXIT_MISMATCH);
    CHECK(mismatch.out == plain);
    // The expected value is printed in lower case (amendment 10).
    CHECK(mismatch.err == "worldgen-cli: error: hash mismatch: expected " + wrong + ", got " + hex + "\n");

    // With --detail, the detail lines are printed in both cases.
    const ProcessResult detail = sb.cli({"hash", "--db", u8(world), "--detail"});
    REQUIRE(detail.exit_code == EXIT_OK);
    const ProcessResult dm = sb.cli({"hash", "--db", u8(world), "--detail", "--expect", wrong});
    CHECK(dm.exit_code == EXIT_MISMATCH);
    CHECK(dm.out == detail.out);
    const ProcessResult dok = sb.cli({"hash", "--db", u8(world), "--detail", "--expect", hex});
    CHECK(dok.exit_code == EXIT_OK);
    CHECK(dok.out == detail.out);

    // --expect on a non-world file works without --detail.
    write_text(sb.file("x.txt"), "abc");
    CHECK(sb.cli({"hash", "--db", u8(sb.file("x.txt")), "--expect",
                  "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"})
              .exit_code == EXIT_OK);

    // Not 64 hex digits: a usage error.
    for (const std::string& bad : {hex.substr(0, 63), hex + "0", "g" + hex.substr(1), std::string(), " " + hex.substr(1),
                                   "0x" + hex.substr(2)}) {
        CAPTURE(bad);
        CHECK(sb.cli({"hash", "--db", u8(world), "--expect", bad}).exit_code == EXIT_USAGE);
    }
}

TEST_CASE("cli 12.5: --detail refuses REAL values and tables without a primary key") {
    Sandbox sb;
    SUBCASE("REAL value") {
        const fs::path world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("real.db"))});
        REQUIRE(worldgen::db::testing::execute_on_file(
                    u8(world), "CREATE TABLE extra (id INTEGER PRIMARY KEY, v ANY) STRICT; INSERT INTO extra VALUES (1, 1.5)")
                    .ok());
        CHECK(sb.cli({"hash", "--db", u8(world)}).exit_code == EXIT_OK);
        const ProcessResult r = sb.cli({"hash", "--db", u8(world), "--detail"});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
    }
    SUBCASE("no primary key") {
        const fs::path world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("nopk.db"))});
        REQUIRE(worldgen::db::testing::execute_on_file(u8(world), "CREATE TABLE extra (v INTEGER NOT NULL) STRICT; "
                                                                  "INSERT INTO extra VALUES (1)")
                    .ok());
        const ProcessResult r = sb.cli({"hash", "--db", u8(world), "--detail"});
        CHECK(r.exit_code == EXIT_INPUT);
    }
    SUBCASE("sqlite_* tables are excluded") {
        const fs::path world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("stat.db"))});
        REQUIRE(worldgen::db::testing::execute_on_file(u8(world), "ANALYZE").ok());
        const ProcessResult r = sb.cli({"hash", "--db", u8(world), "--detail"});
        REQUIRE(r.exit_code == EXIT_OK);
        const std::vector<std::string> lines = lines_of(r.out);
        REQUIRE(lines.size() == 4);
        CHECK(lines[2] == "table layer " + to_hex(p004::layer_table_hash({})));
        CHECK(lines[3] == "table meta " + to_hex(p004::meta_table_hash(cli_meta_rows(0))));
    }
}
