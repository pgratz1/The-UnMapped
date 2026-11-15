// Output location handling of the generation pipeline after the P0-04 audit
// (decided by the lead): anything at the output path counts as existing,
// a dangling symbolic link included. WorldWriter::check_output on such a
// link under refuse returns output_exists; generate_world to it returns
// output_exists and leaves no .partial; under replace the link is replaced
// by a regular file and its target is not created.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>

#include "db_test_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/utf8_path.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"

namespace {

namespace fs = std::filesystem;
using worldgen::db::ErrorCode;
using worldgen::db::Overwrite;
using worldgen::db::WorldWriter;

worldgen::WorldParams params_for(std::uint64_t seed) {
    worldgen::WorldParams p;
    p.seed = seed;
    p.content_hash = worldgen::sha256(std::string_view{});
    return p;
}

}  // namespace

TEST_CASE("generate audit: a dangling symbolic link at the output path counts as existing") {
    dbtest::TempDir dir("p004-dangling-link");
    const fs::path target = dir.path() / "target.db";
    const fs::path link = dir.path() / "link.db";
    std::error_code ec;
    fs::create_symlink(target, link, ec);
    if (ec) {
        MESSAGE("skipped: symbolic links cannot be created here (", ec.message(), ")");
        return;
    }
    const std::string link_u8 = worldgen::db::path_to_utf8(link);

    const auto refuse = WorldWriter::check_output(link_u8, Overwrite::refuse);
    REQUIRE(!refuse.ok());
    CHECK(refuse.code() == ErrorCode::output_exists);

    const auto gen_refused = worldgen::generate_world(params_for(1), link_u8, Overwrite::refuse);
    REQUIRE(!gen_refused.ok());
    CHECK(gen_refused.code() == ErrorCode::output_exists);
    CHECK(fs::is_symlink(fs::symlink_status(link)));
    CHECK(fs::read_symlink(link) == target);
    CHECK(!fs::exists(target));
    CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"link.db"});  // no .partial

    CHECK(WorldWriter::check_output(link_u8, Overwrite::replace).ok());

    REQUIRE(worldgen::generate_world(params_for(1), link_u8, Overwrite::replace).ok());
    const fs::file_status st = fs::symlink_status(link);
    CHECK(!fs::is_symlink(st));
    CHECK(fs::is_regular_file(st));
    CHECK(!fs::exists(target));
    CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"link.db"});
    const std::string ref = dir.file("ref.db");
    REQUIRE(worldgen::generate_world(params_for(1), ref, Overwrite::refuse).ok());
    CHECK(dbtest::read_file(link_u8) == dbtest::read_file(ref));
}

TEST_CASE("generate audit: a symbolic link to an existing file also counts as existing") {
    dbtest::TempDir dir("p004-live-link");
    const std::string target = dir.file("target.db");
    REQUIRE(worldgen::generate_world(params_for(2), target, Overwrite::refuse).ok());
    const auto before = dbtest::read_file(target);
    const fs::path link = dir.path() / "link.db";
    std::error_code ec;
    fs::create_symlink(worldgen::db::path_from_utf8(target), link, ec);
    if (ec) {
        MESSAGE("skipped: symbolic links cannot be created here (", ec.message(), ")");
        return;
    }
    const auto r = worldgen::generate_world(params_for(3), worldgen::db::path_to_utf8(link), Overwrite::refuse);
    REQUIRE(!r.ok());
    CHECK(r.code() == ErrorCode::output_exists);
    CHECK(fs::is_symlink(fs::symlink_status(link)));
    CHECK(dbtest::read_file(target) == before);
}
