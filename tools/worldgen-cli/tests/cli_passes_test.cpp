// worldgen-cli and the elevation revision "natural passes and final talus"
// (P1-01 fourth version, generator 0.6.0): schema version 2 in `report`,
// table `pass` in `hash --detail` and `[tables]`, and the pass keys of
// P1-00 10.3 with the walkability measure, checked against the test's own
// implementation (worldgen/tests/p1_01_passes_support.hpp) and the
// reference's golden rows (worldgen/tests/golden/p1-01-elevation.txt, case
// shipped-512-s42).
//
// Written from the specs only.

#include <doctest/doctest.h>

#include <fstream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "p3_01_history_fixture.hpp"
#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "p1_01_passes_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/version.hpp"

namespace {

using namespace clitest;

fs::path write_data_root(const Sandbox& sb) {
    const fs::path root = sb.file("data");
    fs::create_directories(root / "worldgen");
    write_text(root / "worldgen" / "elevation.toml", elev_test::elevation_toml());
    write_text(root / "worldgen" / "hydrology.toml",
               "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = "
               "\"0.5\" }\n");
    write_text(root / "worldgen" / "climate.toml", std::string(elev_api::CLIMATE_TOML));  // P1-03 4
    write_text(root / "worldgen" / "biomes.toml", std::string(elev_api::BIOMES_TOML));  // P1-04 4.2
    write_text(root / "worldgen" / "resources.toml", std::string(elev_api::RESOURCES_TOML));  // P1-05 4.2
    write_text(root / "worldgen" / "regions.toml", std::string(elev_api::REGIONS_TOML));      // P1-06 4
    fs::create_directories(root / "biomes");
    write_text(root / "biomes" / "biomes.toml", std::string(elev_api::BIOME_DEFINITIONS_TOML));  // P1-04 4.1
    history_fixture::copy_to(root);  // P3-01: the required history kinds
    return root;
}

// The items of one case of the elevation golden file.
std::map<std::string, std::string> elevation_golden(const std::string& name) {
    std::ifstream in(UNMAPPED_P1_01_GOLDEN_PATH, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open the elevation golden file");
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
    REQUIRE_MESSAGE(!out.empty(), "no golden case " << name);
    return out;
}

std::string hex_of(std::string_view bytes) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (const char ch : bytes) {
        const auto b = static_cast<unsigned char>(ch);
        s += d[b >> 4U];
        s += d[b & 15U];
    }
    return s;
}

}  // namespace

TEST_CASE("cli P1-01 9.1, P1-00 10.3: a world with a pass (512, seed 42): rows, hash --detail and report pass keys") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    const fs::path world = sb.file("w.db");
    const ProcessResult g = sb.cli({"generate", "--seed", "42", "--map-size", "512", "--until", "elevation", "--data",
                                    u8(data), "--threads", "4", "--out", u8(world)});
    INFO("stderr: ", g.err);
    REQUIRE(g.exit_code == EXIT_OK);

    // The rows of table `pass` and the layer, read back.
    auto opened = worldgen::db::WorldReader::open(u8(world));
    REQUIRE(opened.ok());
    const worldgen::db::WorldReader& w = opened.value();
    CHECK(w.schema_version() == 13);   // P5-02 11.1
    CHECK(w.table_columns("pass").value() == std::vector<std::string>{"id", "range_id", "kind", "capped", "col_x",
                                                                       "col_y", "saddle_dm", "col_dm", "path"});
    const auto raw = w.table_rows("pass");
    REQUIRE(raw.ok());
    std::vector<elev_test::PassRow> rows;
    std::vector<p004::PassTableRow> trows;
    std::string text;
    for (const auto& r : raw.value()) {
        REQUIRE(r.size() == 9U);
        elev_test::PassRow p;
        p.id = r[0].integer;
        p.range_id = r[1].integer;
        p.kind = r[2].integer;
        p.capped = r[3].integer;
        p.col_x = r[4].integer;
        p.col_y = r[5].integer;
        p.saddle_dm = r[6].integer;
        p.col_dm = r[7].integer;
        for (std::size_t k = 0; k < 8; ++k) {
            CHECK(r[k].type == worldgen::db::ValueType::integer);
        }
        CHECK(r[8].type == worldgen::db::ValueType::blob);
        p.path.assign(r[8].bytes.begin(), r[8].bytes.end());
        rows.push_back(p);
        trows.push_back(p004::PassTableRow{p.id, p.range_id, p.kind, p.capped, p.col_x, p.col_y, p.saddle_dm, p.col_dm,
                                           p.path});
        text += std::to_string(p.id) + " " + std::to_string(p.range_id) + " " + std::to_string(p.kind) + " " +
                std::to_string(p.capped) + " " + std::to_string(p.col_x) + " " + std::to_string(p.col_y) + " " +
                std::to_string(p.saddle_dm) + " " + std::to_string(p.col_dm) + " " +
                worldgen::to_hex(worldgen::sha256(p.path)) + "\n";
    }
    // Rows in id order 1, 2, ...; equal to the reference (one site).
    REQUIRE(rows.size() == 1U);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        CHECK(rows[i].id == static_cast<std::int64_t>(i) + 1);
        REQUIRE(rows[i].path.size() % 4U == 0U);
        // The path is u32 little-endian cell indices (9.1), decoded here
        // byte by byte: every index is a cell of the 512 x 512 map, steps
        // are between 8-adjacent cells, and the col is on the path.
        std::vector<std::int64_t> cells_;
        for (std::size_t k = 0; k < rows[i].path.size(); k += 4) {
            const std::uint32_t c = static_cast<std::uint32_t>(rows[i].path[k]) |
                                    (static_cast<std::uint32_t>(rows[i].path[k + 1]) << 8U) |
                                    (static_cast<std::uint32_t>(rows[i].path[k + 2]) << 16U) |
                                    (static_cast<std::uint32_t>(rows[i].path[k + 3]) << 24U);
            REQUIRE(c < 512U * 512U);
            cells_.push_back(c);
        }
        bool col_on_path = false;
        for (std::size_t k = 0; k < cells_.size(); ++k) {
            col_on_path = col_on_path || cells_[k] == rows[i].col_y * 512 + rows[i].col_x;
            if (k > 0) {
                const std::int64_t dx = cells_[k] % 512 - cells_[k - 1] % 512;
                const std::int64_t dy = cells_[k] / 512 - cells_[k - 1] / 512;
                CHECK(dx * dx + dy * dy >= 1);
                CHECK(dx * dx + dy * dy <= 2);
            }
        }
        CHECK(col_on_path);
    }
    const std::map<std::string, std::string> gold = elevation_golden("shipped-512-s42");
    CHECK(worldgen::to_hex(worldgen::sha256(std::span<const std::uint8_t>(
              reinterpret_cast<const std::uint8_t*>(text.data()), text.size()))) == gold.at("pass_rows"));

    const auto layer = w.read_layer("elevation");
    REQUIRE(layer.ok());
    const std::vector<std::int16_t>* e = layer.value().grid.get_if<std::int16_t>();
    REQUIRE(e != nullptr);

    // hash --detail: the table pass line between table meta and the layers.
    const ProcessResult h = sb.cli({"hash", "--db", u8(world), "--detail"});
    REQUIRE(h.exit_code == EXIT_OK);
    const std::vector<std::string> hl = lines_of(h.out);
    REQUIRE(hl.size() == 39U);  // thirty-four tables since schema 10 (P5-03 16.1)
    const char* const names[] = {"age", "beast", "beast_population", "biome", "book", "civilization", "climate", "culture", "event", "event_cause",
                                 "event_kind", "event_kind_role", "event_kind_value", "event_participant",
                                 "event_value", "figure", "god", "layer", "meta", "monster", "monster_kind", "pass", "phase", "race", "race_biome",
                                 "race_relation", "region", "region_biome", "region_border", "region_control", "region_name",
                                 "relation", "settlement", "settlement_census", "settlement_race", "site"};   // schema 10
    for (std::size_t k = 0; k < 36; ++k) {
        CHECK(starts_with(hl[2 + k], std::string("table ") + names[k] + " "));
    }
    CHECK(hl[23] == "table pass " + worldgen::to_hex(p004::pass_table_hash(trows)));   // schema 9: four tables before
    CHECK(hl[38] == "layer elevation " + gold.at("layer"));   // schema 10: 34 tables
    CHECK(hl[38] == "layer elevation 590569e0db4d6bc84fd8e955ef77ab49bec2c82b5d383fd75d212800e31a3b91");  // 10.4.1

    // report: schema 2, the table, and the five pass keys right after
    // edge_parallel_run_m, equal to the test's own measure.
    const ProcessResult rep = sb.cli({"report", "--db", u8(world)});
    INFO("stderr: ", rep.err);
    REQUIRE(rep.exit_code == EXIT_OK);
    CHECK(contains(rep.out, "\nschema_version = 13\n"));
    CHECK(contains(rep.out, "\ngenerator_version = " + std::string(worldgen::GENERATOR_VERSION) + "\n"));
    CHECK(std::string(worldgen::GENERATOR_VERSION) == "0.29.0");  // P5-03; P5-03a; P5-04
    CHECK(contains(rep.out, "\n[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\nbook = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\nfigure = 0\ngod = 0\nlayer = 1\nmeta = 17\nmonster = 0\nmonster_kind = 0\npass = " + std::to_string(rows.size()) +
                                "\nphase = 0\nrace = 0\nrace_biome = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n[layers]\n"));
    const std::vector<std::string> want =
        elev_test::report_pass_lines(worldgen::geo::Grid{512, 512}, *e, rows);
    std::string block;
    for (const std::string& l : want) {
        block += l + "\n";
    }
    const std::size_t at = rep.out.find("\nedge_parallel_run_m = ");
    REQUIRE(at != std::string::npos);
    const std::size_t next = rep.out.find('\n', at + 1);
    REQUIRE(next != std::string::npos);
    CHECK(rep.out.substr(next + 1, block.size()) == block);
    CHECK(contains(block, "passes = 1\n"));
    CHECK(contains(block, "natural_gaps = 0\n"));
    MESSAGE("512 seed 42 pass lines:\n", block, "row path ", hex_of(std::string(rows[0].path.begin(), rows[0].path.end())).substr(0, 32), "...");
}

TEST_CASE("cli P1-00 10.3: the pass keys of a world without a site, and with no elevation stage") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    // 512 seed 0: no site (reference), the table exists and is empty.
    const fs::path world = sb.file("w0.db");
    const ProcessResult g = sb.cli({"generate", "--seed", "0", "--map-size", "512", "--until", "elevation", "--data",
                                    u8(data), "--out", u8(world)});
    REQUIRE(g.exit_code == EXIT_OK);
    const ProcessResult rep = sb.cli({"report", "--db", u8(world)});
    REQUIRE(rep.exit_code == EXIT_OK);
    CHECK(contains(rep.out, "\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n[layers]\n"));
    CHECK(contains(rep.out, "\npasses = 0\nnatural_gaps = 0\npasses_capped = 0\npass_col_m = n/a n/a n/a\n"
                            "passes_walkable = 0\n"));
    // --until none: no [geography] section, so no pass key, but the table.
    const fs::path none = sb.file("n.db");
    REQUIRE(sb.cli({"generate", "--seed", "0", "--until", "none", "--data", u8(data), "--out", u8(none)}).exit_code ==
            EXIT_OK);
    const ProcessResult rn = sb.cli({"report", "--db", u8(none)});
    REQUIRE(rn.exit_code == EXIT_OK);
    CHECK(contains(rn.out, "\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n[layers]\n"));
    CHECK_FALSE(contains(rn.out, "passes ="));
    CHECK_FALSE(contains(rn.out, "[geography]"));
    const ProcessResult hn = sb.cli({"hash", "--db", u8(none), "--detail"});
    REQUIRE(hn.exit_code == EXIT_OK);
    CHECK(contains(hn.out, "\ntable pass " + worldgen::to_hex(p004::pass_table_hash({})) + "\n"));
}

TEST_CASE("cli P1-00 10.3, P0-04 4.5: report refuses a malformed row of table pass: exit 3, nothing on stdout") {
    Sandbox sb;
    const fs::path data = write_data_root(sb);
    const fs::path world = sb.file("w.db");
    REQUIRE(sb.cli({"generate", "--seed", "42", "--map-size", "512", "--until", "elevation", "--data", u8(data),
                    "--threads", "4", "--out", u8(world)})
                .exit_code == EXIT_OK);
    // The unmodified world reports.
    REQUIRE(sb.cli({"report", "--db", u8(world)}).exit_code == EXIT_OK);
    struct Bad {
        const char* what;
        const char* sql;
    };
    const std::vector<Bad> bads = {
        {"kind 2", "UPDATE pass SET kind = 2"},
        {"kind -1", "UPDATE pass SET kind = -1"},
        {"capped 2", "UPDATE pass SET capped = 2"},
        {"capped 1 on a natural gap", "UPDATE pass SET kind = 0, capped = 1"},
        {"col_x outside", "UPDATE pass SET col_x = 512"},
        {"col_y negative", "UPDATE pass SET col_y = -1"},
        {"empty path", "UPDATE pass SET path = x''"},
        {"path length not a multiple of 4", "UPDATE pass SET path = x'010203'"},
        {"path index out of range", "UPDATE pass SET path = x'00000400'"},  // 262144 = 512 * 512
        {"a second, bad row", "INSERT INTO pass SELECT 2, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm, "
                              "x'ffffffff' FROM pass WHERE id = 1"},
    };
    int k = 0;
    for (const Bad& b : bads) {
        CAPTURE(std::string_view(b.what));
        const fs::path bad = sb.file("bad" + std::to_string(k++) + ".db");
        fs::copy_file(world, bad);
        REQUIRE(worldgen::db::testing::execute_on_file(u8(bad), b.sql).ok());
        const ProcessResult r = sb.cli({"report", "--db", u8(bad)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: corrupt table 'pass': "));
    }
}
