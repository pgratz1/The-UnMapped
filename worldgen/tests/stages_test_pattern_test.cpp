// The P0 test-pattern stage and the generation pipeline, spec P0-04
// sections 5.1, 5.2 and 12.2, with the amendments of section 14. Every grid
// is compared with the formulas of 5.2 computed here from the core
// functions (p0_04_reference.hpp).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/stages/test_pattern.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using worldgen::FatalError;
using worldgen::db::Codec;
using worldgen::db::ElementType;
using worldgen::db::ErrorCode;
using worldgen::db::Overwrite;
using worldgen::db::WorldReader;
namespace stages = worldgen::stages;

WorldReader open_world(const std::string& path) {
    auto r = WorldReader::open(path);
    INFO("open: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return std::move(r).value();
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

constexpr std::uint64_t SEEDS[] = {0, 1, 42, std::numeric_limits<std::uint64_t>::max()};

}  // namespace

TEST_CASE("test pattern 5.2: names and sizes") {
    CHECK(stages::TEST_PATTERN_STAGE == "test_pattern");
    CHECK(stages::TEST_PATTERN_SIDE == 256U);
    CHECK(stages::TEST_RAMP_LAYER == "test.ramp");
    CHECK(stages::TEST_HASH_LAYER == "test.hash");
    CHECK(stages::TEST_NOISE_LAYER == "test.noise");
    // The stream names are the registered ones (P0-01 7.3).
    CHECK(worldgen::streams::test_hash == "test.hash");
    CHECK(worldgen::streams::test_noise == "test.noise");
}

TEST_CASE("test pattern 5.2: test.ramp = (x + 2y) mod 256") {
    const worldgen::db::LayerGrid g = stages::test_ramp_grid();
    CHECK(g.width == 256U);
    CHECK(g.height == 256U);
    REQUIRE(g.type() == ElementType::u8);
    const auto& cells = *g.get_if<std::uint8_t>();
    CHECK(cells == p004::ramp_cells());
    // A few cells by hand.
    CHECK(cells[0] == 0);
    CHECK(cells[255] == 255);             // (255, 0)
    CHECK(cells[256] == 2);               // (0, 1)
    CHECK(cells[128 * 256 + 0] == 0);     // (0, 128): 256 mod 256
    CHECK(cells[255 * 256 + 255] == 253);  // (255, 255): 765 mod 256
}

TEST_CASE("test pattern 5.2: test.hash = hash_at(stream_key(seed, \"test.hash\"), x, y) >> 56") {
    for (const std::uint64_t seed : SEEDS) {
        CAPTURE(seed);
        const worldgen::db::LayerGrid g = stages::test_hash_grid(seed);
        CHECK(g.width == 256U);
        CHECK(g.height == 256U);
        REQUIRE(g.type() == ElementType::u8);
        CHECK(*g.get_if<std::uint8_t>() == p004::hash_cells(seed));
    }
    CHECK(*stages::test_hash_grid(0).get_if<std::uint8_t>() != *stages::test_hash_grid(1).get_if<std::uint8_t>());
}

TEST_CASE("test pattern 5.2: test.noise = mul_div(fbm2(...).raw, 32767, 2^32)") {
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        CAPTURE(seed);
        const worldgen::db::LayerGrid g = stages::test_noise_grid(seed);
        CHECK(g.width == 256U);
        CHECK(g.height == 256U);
        REQUIRE(g.type() == ElementType::i16);
        const auto& cells = *g.get_if<std::int16_t>();
        CHECK(cells == p004::noise_cells(seed));
        const auto [lo, hi] = std::minmax_element(cells.begin(), cells.end());
        CHECK(*lo >= -32767);
        CHECK(*hi <= 32767);
        CHECK(*lo < *hi);  // not constant
        MESSAGE("seed ", seed, " test.noise min ", *lo, " max ", *hi);
    }
}

TEST_CASE("test pattern 5.2: grids are pure functions of the seed") {
    CHECK(*stages::test_noise_grid(7).get_if<std::int16_t>() == *stages::test_noise_grid(7).get_if<std::int16_t>());
    CHECK(*stages::test_hash_grid(7).get_if<std::uint8_t>() == *stages::test_hash_grid(7).get_if<std::uint8_t>());
}

TEST_CASE("test pattern 5.1, 5.2: run_test_pattern writes three codec-1 layers in one stage") {
    dbtest::TempDir dir("p004-run-pattern");
    for (const bool after_other_stage : {false, true}) {
        CAPTURE(after_other_stage);
        worldgen::db::WorldWriter writer;
        dbtest::set_standard_meta(writer, 42);
        std::int64_t first_id = 1;
        if (after_other_stage) {
            worldgen::db::StageWriter s = writer.begin_stage("earlier");
            s.write_layer(1, "test.before", p004::grid_of<std::uint8_t>(1, 1, {9}));
            s.commit();
            first_id = 2;
        }
        CHECK(writer.next_layer_id() == first_id);
        stages::run_test_pattern(writer);
        CHECK(writer.next_layer_id() == first_id + 3);
        const std::vector<std::string> expected_stages =
            after_other_stage ? std::vector<std::string>{"earlier", "test_pattern"} : std::vector<std::string>{"test_pattern"};
        CHECK(writer.stages() == expected_stages);
        const std::string out = dir.file(after_other_stage ? "b.db" : "a.db");
        REQUIRE(writer.finalize(out, Overwrite::refuse).ok());

        const WorldReader w = open_world(out);
        CHECK(w.stages() == expected_stages);
        const auto list = w.layers();
        REQUIRE(list.ok());
        const std::size_t offset = after_other_stage ? 1 : 0;
        REQUIRE(list.value().size() == 3 + offset);
        const char* names[] = {"test.ramp", "test.hash", "test.noise"};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& info = list.value()[i + offset];
            CHECK(info.id == first_id + static_cast<std::int64_t>(i));
            CHECK(info.name == names[i]);
            CHECK(info.codec == Codec::delta_varint);
            CHECK(info.width == 256U);
            CHECK(info.height == 256U);
        }
        CHECK(*w.read_layer("test.hash").value().grid.get_if<std::uint8_t>() == p004::hash_cells(42));
        CHECK(*w.read_layer("test.noise").value().grid.get_if<std::int16_t>() == p004::noise_cells(42));
        CHECK(*w.read_layer("test.ramp").value().grid.get_if<std::uint8_t>() == p004::ramp_cells());
    }
}

TEST_CASE("generate 5.1: planned stages") {
    CHECK(worldgen::planned_stages(params_for(0, false)).empty());
    const auto with = worldgen::planned_stages(params_for(0, true));
    REQUIRE(with.size() == 1);
    CHECK(with[0] == "test_pattern");
}

TEST_CASE("generate 5.1: generate_world writes the empty and the test-pattern world") {
    dbtest::TempDir dir("p004-generate-world");
    SUBCASE("empty world, seed 0") {
        const std::string out = dir.file("e.db");
        REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
        const WorldReader w = open_world(out);
        const auto meta = w.meta_entries();
        const p004::MetaRows expected = p004::meta_rows(0);
        REQUIRE(meta.size() == expected.size());
        for (std::size_t i = 0; i < meta.size(); ++i) {
            CHECK(meta[i].key == expected[i].first);
            CHECK(meta[i].value == expected[i].second);
        }
        CHECK(w.layers().value().empty());
        // The same as the writer's empty world (P0-02 8).
        const std::string ref = dir.file("ref.db");
        REQUIRE(dbtest::write_empty_world(ref, 0).ok());
        CHECK(dbtest::read_file(out) == dbtest::read_file(ref));
    }
    SUBCASE("test pattern, map size and phrase") {
        worldgen::WorldParams p = params_for(0, true);
        const worldgen::PhraseSeed ps = worldgen::seed_from_phrase("The Unmapped");
        REQUIRE(ps.ok());
        p.seed = ps.seed;
        p.seed_phrase = ps.phrase;
        p.map_size = 2048;
        const std::string out = dir.file("p.db");
        REQUIRE(worldgen::generate_world(p, out, Overwrite::refuse).ok());
        const WorldReader w = open_world(out);
        CHECK(w.seed() == ps.seed);
        CHECK(ps.phrase == "the unmapped");  // scrubbed (P0-01 7.3)
        CHECK(w.seed_phrase() == "the unmapped");
        CHECK(w.map_size() == 2048U);
        CHECK(w.stages() == std::vector<std::string>{"test_pattern"});
        CHECK(*w.read_layer("test.hash").value().grid.get_if<std::uint8_t>() == p004::hash_cells(ps.seed));
    }
    SUBCASE("output errors are returned") {
        const std::string out = dir.file("x.db");
        REQUIRE(worldgen::generate_world(params_for(1, false), out, Overwrite::refuse).ok());
        const auto again = worldgen::generate_world(params_for(1, false), out, Overwrite::refuse);
        REQUIRE(!again.ok());
        CHECK(again.code() == ErrorCode::output_exists);
        CHECK(worldgen::generate_world(params_for(2, false), out, Overwrite::replace).ok());
        CHECK(open_world(out).seed() == 2U);
        const auto missing = worldgen::generate_world(params_for(1, false), dir.file("no/dir/x.db"), Overwrite::refuse);
        REQUIRE(!missing.ok());
        CHECK(missing.code() == ErrorCode::output_parent_missing);
    }
    SUBCASE("a seed that does not match the phrase is a contract violation") {
        worldgen::WorldParams p = params_for(1, false);
        p.seed_phrase = "the unmapped";
        CHECK_THROWS_AS((void)worldgen::generate_world(p, dir.file("bad.db"), Overwrite::refuse), FatalError);
        CHECK(!dbtest::exists(dir.file("bad.db")));
        CHECK(!dbtest::exists(dir.file("bad.db.partial")));
    }
    SUBCASE("a phrase that is not scrubbed is a contract violation (P0-02 amendment 12)") {
        worldgen::WorldParams p = params_for(1, false);
        p.seed_phrase = "The Unmapped";
        p.seed = worldgen::phrase_seed("The Unmapped");  // matches the bytes, but they are not scrubbed
        CHECK_THROWS_AS((void)worldgen::generate_world(p, dir.file("raw.db"), Overwrite::refuse), FatalError);
        CHECK(!dbtest::exists(dir.file("raw.db")));
        CHECK(!dbtest::exists(dir.file("raw.db.partial")));
    }
}

TEST_CASE("generate 12.2: a fatal error inside the stage leaves no output and no .partial") {
    dbtest::TempDir dir("p004-fault");
    const std::string out = dir.file("f.db");
    CHECK_THROWS_AS((void)stages::testing::generate_world_with_fault(params_for(0, true), out, Overwrite::refuse,
                                                                     stages::testing::StageFault::fatal_inside_test_pattern),
                    FatalError);
    CHECK(dbtest::dir_entries(dir.path()).empty());

    // With an existing output and replace allowed, the old file survives.
    REQUIRE(worldgen::generate_world(params_for(5, false), out, Overwrite::refuse).ok());
    const auto before = dbtest::read_file(out);
    CHECK_THROWS_AS((void)stages::testing::generate_world_with_fault(params_for(0, true), out, Overwrite::replace,
                                                                     stages::testing::StageFault::fatal_inside_test_pattern),
                    FatalError);
    CHECK(dbtest::read_file(out) == before);
    CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"f.db"});

    // StageFault::none is normal operation.
    const std::string ok = dir.file("ok.db");
    REQUIRE(stages::testing::generate_world_with_fault(params_for(0, true), ok, Overwrite::refuse,
                                                       stages::testing::StageFault::none)
                .ok());
    const std::string ref = dir.file("ref.db");
    REQUIRE(worldgen::generate_world(params_for(0, true), ref, Overwrite::refuse).ok());
    CHECK(dbtest::read_file(ok) == dbtest::read_file(ref));
}
