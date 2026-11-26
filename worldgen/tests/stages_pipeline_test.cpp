// The stage pipeline and stage interface (spec P1-00 sections 3, 5.1 and
// 10.1; P0-04 amendment 16 (a); P0-02 section 6 step 3), through the test
// hook generate_world_with_stages, and the stage-limit functions behind
// `generate --until`.
//
// Written from the specs and the public headers worldgen/stages/stage.hpp,
// worldgen/stages/testing.hpp and worldgen/generate.hpp only. Resuming is
// not implemented and not tested.
//
// Stage entry points are plain function pointers, so the stages below talk
// to the test through one file-local record that each test resets.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "data_loader_support.hpp"
#include "db_test_support.hpp"
#include "p3_01_history_fixture.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/data.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/stages/stage.hpp"
#include "worldgen/stages/testing.hpp"

namespace st = worldgen::stages;
namespace wdb = worldgen::db;
namespace wd = worldgen::data;
using st::StageContext;
using st::StageInfo;

namespace {

// --- Grids the stages write ---------------------------------------------------------

wdb::LayerGrid grid_a() {
    wdb::LayerGrid g = wdb::make_layer_grid(wdb::ElementType::u8, 3, 2);
    std::get<std::vector<std::uint8_t>>(g.values) = {0, 1, 255, 7, 128, 3};
    return g;
}

wdb::LayerGrid grid_b() {
    wdb::LayerGrid g = wdb::make_layer_grid(wdb::ElementType::i16, 4, 4);
    auto& v = std::get<std::vector<std::int16_t>>(g.values);
    for (std::size_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<std::int16_t>(static_cast<int>(i * 4099U % 65535U) - 32767);
    }
    return g;
}

wdb::LayerGrid grid_c() {
    wdb::LayerGrid g = wdb::make_layer_grid(wdb::ElementType::u32, 5, 1);
    std::get<std::vector<std::uint32_t>>(g.values) = {0U, 1U, 4194304U, 4294967295U, 17U};
    return g;
}

bool same_grid(const wdb::LayerGrid* got, const wdb::LayerGrid& want) {
    return got != nullptr && got->width == want.width && got->height == want.height && got->values == want.values;
}

// --- What the stages observe ------------------------------------------------------

struct Observed {
    std::vector<std::string> ran;  // stage names, in run order
    std::uint64_t seed = 0;
    std::int64_t map_size = 0;
    std::uint32_t threads = 0;
    bool first_cannot_see_own = false;
    bool first_sees_nothing_earlier = false;
    bool second_sees_first = false;
    bool second_cannot_see_own = false;
    bool third_sees_all = false;
    bool third_missing_is_fatal = false;
    bool third_find_missing_is_null = false;
    std::size_t content_kinds = 0;
    std::size_t tune_definitions = 0;
    int fail_in = 0;  // 1, 2, 3: that stage raises FatalError (after writing, for 1 and 2)
    bool duplicate_in_second = false;
};

Observed obs;

void stage_first(StageContext& ctx) {
    obs.ran.emplace_back(ctx.stage_name());
    obs.seed = ctx.seed();
    obs.map_size = ctx.map_size();
    obs.threads = ctx.threads();
    obs.content_kinds = ctx.content().kinds.size();
    const wd::KindContent* tune = ctx.content().find_kind("tune");
    obs.tune_definitions = tune == nullptr ? 0U : tune->definitions.size();
    obs.first_sees_nothing_earlier =
        ctx.find_layer("test.a") == nullptr && ctx.find_layer("test.b") == nullptr && ctx.find_layer("test.c") == nullptr;
    ctx.write_layer("test.a", grid_a());
    ctx.write_layer("test.b", grid_b());
    obs.first_cannot_see_own = ctx.find_layer("test.a") == nullptr && ctx.find_layer("test.b") == nullptr;
    if (obs.fail_in == 1) {
        worldgen::fatal("forced failure in stage one");
    }
}

void stage_second(StageContext& ctx) {
    obs.ran.emplace_back(ctx.stage_name());
    obs.second_sees_first = same_grid(ctx.find_layer("test.a"), grid_a()) && same_grid(ctx.find_layer("test.b"), grid_b()) &&
                            ctx.find_layer("test.a")->type() == wdb::ElementType::u8 &&
                            ctx.find_layer("test.b")->type() == wdb::ElementType::i16 &&
                            same_grid(&ctx.layer("test.b"), grid_b());
    if (obs.duplicate_in_second) {
        ctx.write_layer("test.a", grid_c());  // a name an earlier stage used: fatal
    }
    ctx.write_layer("test.c", grid_c());
    obs.second_cannot_see_own = ctx.find_layer("test.c") == nullptr;
    if (obs.fail_in == 2) {
        worldgen::fatal("forced failure in stage two");
    }
}

void stage_third(StageContext& ctx) {
    obs.ran.emplace_back(ctx.stage_name());
    obs.third_sees_all = same_grid(ctx.find_layer("test.a"), grid_a()) && same_grid(ctx.find_layer("test.b"), grid_b()) &&
                         same_grid(ctx.find_layer("test.c"), grid_c());
    obs.third_find_missing_is_null = ctx.find_layer("elevation") == nullptr;
    try {
        static_cast<void>(ctx.layer("elevation"));
    } catch (const worldgen::FatalError&) {
        obs.third_missing_is_fatal = true;
    }
    if (obs.fail_in == 3) {
        worldgen::fatal("forced failure in stage three");
    }
}

const std::vector<StageInfo>& three_stages() {
    static const std::vector<StageInfo> s = {
        {"test_first", &stage_first},
        {"test_second", &stage_second},
        {"test_third", &stage_third},
    };
    return s;
}

worldgen::WorldParams params(std::uint64_t seed, const wd::ContentSet& content, std::uint32_t threads = 1U) {
    worldgen::WorldParams p;
    p.seed = seed;
    p.map_size = 512;
    p.content_hash = wd::content_hash(content);
    p.threads = threads;
    return p;
}

// A content set with one fixed-id kind loaded (so content() is not trivial).
wd::ContentSet tune_content() {
    std::vector<wd::IdSchema> ids;
    std::vector<wd::Field> f;
    f.push_back(wd::Field::required("x", wd::Type::integer(0, 10)));
    ids.push_back(wd::IdSchema{"only", wd::Schema(std::move(f))});
    wd::KindRegistry reg;
    reg.add(wd::Kind::with_fixed_ids("tune", "tunes", true, std::move(ids)));
    wd::MemoryDataRoot root;
    root.directories.push_back(loader_test::dir("tunes", {loader_test::file("t.toml", "[tune.only]\nx = 4\n")}));
    history_fixture::add_to(root);  // P3-01: the required history kinds
    auto r = wd::load_content_from_memory(reg, root);
    REQUIRE(r.ok());
    return *r.value;
}

void reset(int fail_in = 0, bool duplicate = false) {
    obs = Observed{};
    obs.fail_in = fail_in;
    obs.duplicate_in_second = duplicate;
}

std::set<std::string> left_in(const dbtest::TempDir& dir) { return dbtest::dir_entries(dir.path()); }

}  // namespace

TEST_CASE("pipeline P1-00 3, 5.1: stages run in order, each sees exactly the layers committed before it") {
    const wd::ContentSet content = tune_content();
    dbtest::TempDir dir("p1-pipeline-order");
    reset();
    const auto status = st::testing::generate_world_with_stages(params(42, content, 3U), content, three_stages(),
                                                                dir.file("w.db"));
    REQUIRE(status.ok());
    CHECK(obs.ran == std::vector<std::string>{"test_first", "test_second", "test_third"});
    CHECK(obs.seed == 42U);
    CHECK(obs.map_size == 512);
    CHECK(obs.threads == 3U);
    CHECK(obs.content_kinds == 1U);
    CHECK(obs.tune_definitions == 1U);
    CHECK(obs.first_sees_nothing_earlier);
    CHECK(obs.first_cannot_see_own);
    CHECK(obs.second_sees_first);
    CHECK(obs.second_cannot_see_own);
    CHECK(obs.third_sees_all);
    CHECK(obs.third_find_missing_is_null);
    CHECK(obs.third_missing_is_fatal);

    // The world: layer ids 1, 2, 3 in write order, codec 1; meta.stages in run order.
    auto opened = wdb::WorldReader::open(dir.file("w.db"));
    REQUIRE(opened.ok());
    const wdb::WorldReader& reader = opened.value();
    CHECK(reader.stages() == std::vector<std::string>{"test_first", "test_second", "test_third"});
    CHECK(reader.seed() == 42U);
    CHECK(reader.map_size() == 512U);
    CHECK(reader.content_hash() == wd::content_hash_hex(content));
    const auto layers = reader.layers();
    REQUIRE(layers.ok());
    REQUIRE(layers.value().size() == 3U);
    const char* names[] = {"test.a", "test.b", "test.c"};
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(layers.value()[i].id == static_cast<std::int64_t>(i + 1U));
        CHECK(layers.value()[i].name == names[i]);
        CHECK(layers.value()[i].codec == wdb::Codec::delta_varint);
    }
    // What later stages saw is exactly what is stored.
    const auto a = reader.read_layer("test.a");
    const auto b = reader.read_layer("test.b");
    const auto c = reader.read_layer("test.c");
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    REQUIRE(c.ok());
    CHECK(same_grid(&a.value().grid, grid_a()));
    CHECK(same_grid(&b.value().grid, grid_b()));
    CHECK(same_grid(&c.value().grid, grid_c()));
    // The thread count is not recorded.
    for (const auto& m : reader.meta_entries()) {
        CHECK(m.key.find("thread") == std::string::npos);
    }
}

TEST_CASE("pipeline D10: the thread count never changes a byte") {
    const wd::ContentSet content = tune_content();
    dbtest::TempDir dir("p1-pipeline-threads");
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        reset();
        REQUIRE(st::testing::generate_world_with_stages(params(7, content, threads), content, three_stages(),
                                                        dir.file("t" + std::to_string(threads) + ".db"))
                    .ok());
        CHECK(obs.threads == threads);
    }
    const auto one = dbtest::read_file(dir.file("t1.db"));
    REQUIRE_FALSE(one.empty());
    CHECK(dbtest::read_file(dir.file("t2.db")) == one);
    CHECK(dbtest::read_file(dir.file("t5.db")) == one);
}

TEST_CASE("pipeline: a failing stage leaves no output and no .partial") {
    const wd::ContentSet content = tune_content();
    for (const int fail_in : {1, 2, 3}) {
        CAPTURE(fail_in);
        dbtest::TempDir dir("p1-pipeline-fail-" + std::to_string(fail_in));
        reset(fail_in);
        CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(params(1, content), content,
                                                                                  three_stages(), dir.file("w.db"))),
                        worldgen::FatalError);
        CHECK(obs.ran.size() == static_cast<std::size_t>(fail_in));  // later stages never run
        CHECK(left_in(dir).empty());
    }
    // An existing output is left untouched.
    dbtest::TempDir dir("p1-pipeline-fail-existing");
    dbtest::write_file(dir.file("w.db"), {1, 2, 3});
    reset(2);
    CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(
                        params(1, content), content, three_stages(), dir.file("w.db"), wdb::Overwrite::replace)),
                    worldgen::FatalError);
    CHECK(dbtest::read_file(dir.file("w.db")) == std::vector<std::uint8_t>{1, 2, 3});
    CHECK(left_in(dir) == std::set<std::string>{"w.db"});
}

TEST_CASE("pipeline P1-00 4 rule 1: a layer name already written by an earlier stage is fatal") {
    const wd::ContentSet content = tune_content();
    dbtest::TempDir dir("p1-pipeline-duplicate");
    reset(0, true);
    CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(params(1, content), content,
                                                                              three_stages(), dir.file("w.db"))),
                    worldgen::FatalError);
    CHECK(left_in(dir).empty());
}

TEST_CASE("pipeline: the test hook's preconditions") {
    const wd::ContentSet content = tune_content();
    dbtest::TempDir dir("p1-pipeline-preconditions");
    reset();
    {
        worldgen::WorldParams p = params(1, content);
        p.test_pattern = true;
        CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(p, content, three_stages(),
                                                                                  dir.file("w.db"))),
                        worldgen::FatalError);
    }
    {
        worldgen::WorldParams p = params(1, content);
        p.until = std::string("none");
        CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(p, content, three_stages(),
                                                                                  dir.file("w.db"))),
                        worldgen::FatalError);
    }
    {
        worldgen::WorldParams p = params(1, content);
        p.content_hash = worldgen::sha256(std::string_view{});  // not the hash of `content`
        CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(p, content, three_stages(),
                                                                                  dir.file("w.db"))),
                        worldgen::FatalError);
    }
    {
        const std::vector<StageInfo> bad = {{"Not A Stage", &stage_first}};
        CHECK_THROWS_AS(static_cast<void>(st::testing::generate_world_with_stages(params(1, content), content, bad,
                                                                                  dir.file("w.db"))),
                        worldgen::FatalError);
    }
    CHECK(obs.ran.empty());
    CHECK(left_in(dir).empty());
}

TEST_CASE("pipeline: no stages gives the P0 empty world; a stage that writes nothing is still recorded") {
    dbtest::TempDir dir("p1-pipeline-empty");
    const wd::ContentSet none;
    worldgen::WorldParams p;
    p.seed = 9;
    p.content_hash = wd::content_hash(none);
    REQUIRE(st::testing::generate_world_with_stages(p, none, {}, dir.file("hook.db")).ok());
    p.until = std::string("none");
    REQUIRE(worldgen::generate_world(p, dir.file("plain.db"), wdb::Overwrite::refuse).ok());
    p.until = std::nullopt;
    CHECK(dbtest::read_file(dir.file("hook.db")) == dbtest::read_file(dir.file("plain.db")));

    reset();
    const std::vector<StageInfo> one = {{"test_third", &stage_third}};
    REQUIRE(st::testing::generate_world_with_stages(p, none, one, dir.file("quiet.db")).ok());
    auto opened = wdb::WorldReader::open(dir.file("quiet.db"));
    REQUIRE(opened.ok());
    CHECK(opened.value().stages() == std::vector<std::string>{"test_third"});
    const auto layers = opened.value().layers();
    REQUIRE(layers.ok());
    CHECK(layers.value().empty());
}

// --- Stage list and limits (P1-00 3, 10.1) ---------------------------------------------

TEST_CASE("stages P1-00 3, P3-03 4.3: the stage names, in run order") {
    const std::vector<std::string_view> names(st::STAGE_NAMES.begin(), st::STAGE_NAMES.end());
    CHECK(names == std::vector<std::string_view>{"elevation", "hydrology", "climate", "biomes", "resources", "regions",
                                                 "chronology", "history"});
    CHECK(st::UNTIL_NONE == "none");
    // Implemented: elevation (P1-01), hydrology (P1-02), climate (P1-03),
    // biomes (P1-04), resources (P1-05), regions (P1-06), chronology
    // (P3-03) and history (P3-04).
    const auto implemented = st::implemented_stages();
    REQUIRE(implemented.size() == 8U);
    CHECK(implemented[0].name == "elevation");
    CHECK(implemented[0].run != nullptr);
    CHECK(implemented[1].name == "hydrology");
    CHECK(implemented[1].run != nullptr);
    CHECK(implemented[2].name == "climate");
    CHECK(implemented[2].run != nullptr);
    CHECK(implemented[3].name == "biomes");
    CHECK(implemented[3].run != nullptr);
    CHECK(implemented[4].name == "resources");
    CHECK(implemented[4].run != nullptr);
    CHECK(implemented[5].name == "regions");
    CHECK(implemented[5].run != nullptr);
    CHECK(implemented[6].name == "chronology");
    CHECK(implemented[6].run != nullptr);
    CHECK(implemented[7].name == "history");
    CHECK(implemented[7].run != nullptr);
}

TEST_CASE("stages P1-00 10.1: check_stage_limit") {
    CHECK(st::check_stage_limit("none") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("elevation") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("hydrology") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("climate") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("biomes") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("resources") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("regions") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("chronology") == st::StageLimitError::none);
    CHECK(st::check_stage_limit("history") == st::StageLimitError::none);  // P3-04 9.5
    for (const std::string_view name : st::STAGE_NAMES) {
        CAPTURE(std::string(name));
        CHECK(st::check_stage_limit(name) == st::StageLimitError::none);  // every stage is implemented
    }
    for (const std::string_view bad : {std::string_view("bogus"), std::string_view(""), std::string_view("Elevation"),
                                       std::string_view("NONE"), std::string_view("elevation "),
                                       std::string_view("test_pattern"), std::string_view("elevation,hydrology")}) {
        CAPTURE(std::string(bad));
        CHECK(st::check_stage_limit(bad) == st::StageLimitError::unknown);
    }
}

TEST_CASE("stages P1-00 10.1: stages_until") {
    const auto all = st::stages_until(std::nullopt);
    REQUIRE(all.size() == 8U);
    CHECK(all[0].name == "elevation");
    CHECK(all[1].name == "hydrology");
    CHECK(all[2].name == "climate");
    CHECK(all[3].name == "biomes");
    CHECK(all[4].name == "resources");
    CHECK(all[5].name == "regions");
    CHECK(all[6].name == "chronology");
    CHECK(all[7].name == "history");
    CHECK(st::stages_until(std::string("none")).empty());
    const auto upto = st::stages_until(std::string("elevation"));
    REQUIRE(upto.size() == 1U);
    CHECK(upto[0].name == "elevation");
    const auto both = st::stages_until(std::string("hydrology"));
    REQUIRE(both.size() == 2U);
    CHECK(both[0].name == "elevation");
    CHECK(both[1].name == "hydrology");
    const auto three = st::stages_until(std::string("climate"));
    REQUIRE(three.size() == 3U);
    CHECK(three[2].name == "climate");
    const auto four = st::stages_until(std::string("biomes"));
    REQUIRE(four.size() == 4U);
    CHECK(four[3].name == "biomes");
    const auto five = st::stages_until(std::string("resources"));
    REQUIRE(five.size() == 5U);
    CHECK(five[4].name == "resources");
    const auto six = st::stages_until(std::string("regions"));
    REQUIRE(six.size() == 6U);
    CHECK(six[5].name == "regions");
    const auto seven = st::stages_until(std::string("chronology"));
    REQUIRE(seven.size() == 7U);
    CHECK(seven[6].name == "chronology");
    CHECK_THROWS_AS(static_cast<void>(st::stages_until(std::string("bogus"))), worldgen::FatalError);
    const auto eight = st::stages_until(std::string("history"));
    REQUIRE(eight.size() == 8U);
    CHECK(eight[7].name == "history");
}

TEST_CASE("generate P0-04 5.1, P1-00 10.1: planned_stages") {
    worldgen::WorldParams p;
    p.content_hash = worldgen::sha256(std::string_view{});
    CHECK(worldgen::planned_stages(p) == std::vector<std::string_view>{"elevation", "hydrology", "climate", "biomes",
                                                                        "resources", "regions", "chronology",
                                                                        "history"});  // every stage
    p.until = std::string("regions");
    CHECK(worldgen::planned_stages(p) ==
          std::vector<std::string_view>{"elevation", "hydrology", "climate", "biomes", "resources", "regions"});
    p.until = std::string("resources");
    CHECK(worldgen::planned_stages(p) ==
          std::vector<std::string_view>{"elevation", "hydrology", "climate", "biomes", "resources"});
    p.until = std::string("biomes");
    CHECK(worldgen::planned_stages(p) == std::vector<std::string_view>{"elevation", "hydrology", "climate", "biomes"});
    p.until = std::string("climate");
    CHECK(worldgen::planned_stages(p) == std::vector<std::string_view>{"elevation", "hydrology", "climate"});
    p.until = std::string("hydrology");
    CHECK(worldgen::planned_stages(p) == std::vector<std::string_view>{"elevation", "hydrology"});
    p.until = std::string("elevation");
    CHECK(worldgen::planned_stages(p) == std::vector<std::string_view>{"elevation"});
    p.until = std::string("none");
    CHECK(worldgen::planned_stages(p).empty());
    p.until = std::nullopt;
    p.test_pattern = true;
    CHECK(worldgen::planned_stages(p) == std::vector<std::string_view>{"test_pattern"});
    // --test-pattern with --until is refused, whatever the limit.
    for (const char* until : {"none", "elevation", "bogus"}) {
        CAPTURE(std::string_view(until));
        p.until = std::string(until);
        CHECK_THROWS_AS(static_cast<void>(worldgen::planned_stages(p)), worldgen::FatalError);
    }
    p.test_pattern = false;
    p.until = std::string("bogus");
    CHECK_THROWS_AS(static_cast<void>(worldgen::planned_stages(p)), worldgen::FatalError);
}

TEST_CASE("generate P1-00 10.1: --until none and every thread count give the P0 empty world") {
    dbtest::TempDir dir("p1-generate-until-none");
    worldgen::WorldParams p;
    p.seed = 0;
    p.content_hash = worldgen::sha256(std::string_view{});
    // The default runs every implemented stage (elevation), which needs its
    // tuning: without content it is fatal and leaves nothing.
    CHECK_THROWS_AS(static_cast<void>(worldgen::generate_world(p, dir.file("default.db"), wdb::Overwrite::refuse)),
                    worldgen::FatalError);
    CHECK(left_in(dir).empty());
    p.until = std::string("none");
    REQUIRE(worldgen::generate_world(p, dir.file("none.db"), wdb::Overwrite::refuse).ok());
    const wd::ContentSet empty;
    p.threads = 256;
    REQUIRE(worldgen::generate_world(p, empty, dir.file("content.db"), wdb::Overwrite::refuse).ok());
    const auto bytes = dbtest::read_file(dir.file("none.db"));
    REQUIRE_FALSE(bytes.empty());
    CHECK(dbtest::read_file(dir.file("content.db")) == bytes);
    // The empty world of seed 0 with the content hash of no data, generator
    // 0.26.0 (schema version 11). The CLI case empty-seed-0 of P0-04 section 9 carries the content
    // hash of the shipped data since the `worldgen` kind was registered
    // (P0-03 amendment 17), so it differs from this value.
    CHECK(worldgen::sha256_hex(bytes) == "a0843b754c134201d758c26191cf1d81e36ec46067253e1c92c3d40f6ae4288a");
    // The test pattern at several thread counts: identical bytes.
    worldgen::WorldParams t;
    t.seed = 0;
    t.content_hash = worldgen::sha256(std::string_view{});
    t.test_pattern = true;
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        t.threads = threads;
        REQUIRE(worldgen::generate_world(t, dir.file("tp" + std::to_string(threads) + ".db"), wdb::Overwrite::refuse).ok());
    }
    const auto tp = dbtest::read_file(dir.file("tp1.db"));
    CHECK(dbtest::read_file(dir.file("tp2.db")) == tp);
    CHECK(dbtest::read_file(dir.file("tp5.db")) == tp);
    CHECK(worldgen::sha256_hex(tp) == "cfb7305f8ff0881a638bbfbe6d1ad412efe5f76b107b6ca746e0d7fcf6db1a24");  // content hash of no data, generator 0.27.0
}

TEST_CASE("generate: an unusable stage limit, or a content hash that does not match, is fatal and leaves nothing") {
    dbtest::TempDir dir("p1-generate-bad-params");
    worldgen::WorldParams p;
    p.seed = 0;
    p.content_hash = worldgen::sha256(std::string_view{});
    for (const char* until : {"Regions", "bogus"}) {  // every geography stage is implemented (P1-06)
        CAPTURE(std::string_view(until));
        p.until = std::string(until);
        CHECK_THROWS_AS(static_cast<void>(worldgen::generate_world(p, dir.file("w.db"), wdb::Overwrite::refuse)),
                        worldgen::FatalError);
    }
    p.until = std::string("none");
    p.test_pattern = true;
    CHECK_THROWS_AS(static_cast<void>(worldgen::generate_world(p, dir.file("w.db"), wdb::Overwrite::refuse)),
                    worldgen::FatalError);
    p.test_pattern = false;
    const wd::ContentSet content = tune_content();
    CHECK_THROWS_AS(static_cast<void>(worldgen::generate_world(p, content, dir.file("w.db"), wdb::Overwrite::refuse)),
                    worldgen::FatalError);
    CHECK(left_in(dir).empty());
}
