// World file finalization tests after the determinism audit: the rename into
// place under simulated filesystem faults, stale <out>.partial-journal
// files, and invalid UTF-8 paths (spec P0-02 section 6 steps 6 and 9, and
// the lead's decisions recorded in the P0-02 follow-up; hooks in
// worldgen/db/testing.hpp).

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>  // doctest needs the full type to print strings with the MSVC STL
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "worldgen/db/db.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using worldgen::db::ErrorCode;
using worldgen::db::Overwrite;
using worldgen::db::Status;
using worldgen::db::WorldReader;
using worldgen::db::WorldWriter;
using worldgen::db::testing::FsFaults;
using worldgen::db::testing::WriterAccess;
using Bytes = std::vector<std::uint8_t>;

constexpr std::uint64_t SEED = 2024;

// Finalizes a world for SEED (seed, content hash, map size set) into out,
// with the given faults.
Status finalize_with(const std::string& out, Overwrite overwrite, const FsFaults& faults) {
    WorldWriter writer;
    dbtest::set_standard_meta(writer, SEED);
    WriterAccess::set_fs_faults(writer, faults);
    return writer.finalize(out, overwrite);
}

// The bytes of a normally written world for SEED.
Bytes normal_world_bytes() {
    dbtest::TempDir dir("finalize-reference");
    const std::string out = dir.file("world.db");
    const Status status = dbtest::write_empty_world(out, SEED);
    REQUIRE(status.ok());
    return dbtest::read_file(out);
}

// An old output: a valid world with a different seed, so it differs from the
// new one.
Bytes write_old_output(const std::string& out) {
    REQUIRE(dbtest::write_empty_world(out, 1).ok());
    return dbtest::read_file(out);
}

std::string code_name(const Status& s) {
    return s.ok() ? std::string("ok") : std::string(worldgen::db::error_code_name(s.code()));
}
std::string code_name(ErrorCode c) { return worldgen::db::error_code_name(c); }

void check_no_temporaries(const std::string& out) {
    CHECK_FALSE(dbtest::exists(out + ".partial"));
    CHECK_FALSE(dbtest::exists(out + ".partial-journal"));
    CHECK_FALSE(dbtest::exists(out + "-journal"));
    CHECK_FALSE(dbtest::exists(out + "-wal"));
}

}  // namespace

TEST_SUITE("db.finalize") {

TEST_CASE("replace, first rename fails, no existing output: output_rename_failed, nothing left (audit blocker)") {
    dbtest::TempDir dir("fin-replace-noout");
    const std::string out = dir.file("world.db");
    FsFaults faults;
    faults.failing_renames = 1;
    const Status status = finalize_with(out, Overwrite::replace, faults);
    CHECK(code_name(status) == code_name(ErrorCode::output_rename_failed));
    CHECK_FALSE(dbtest::exists(out));
    check_no_temporaries(out);
    CHECK(dbtest::dir_entries(dir.path()).empty());
}

TEST_CASE("replace, first rename fails, old output present: the fallback succeeds") {
    const Bytes expected = normal_world_bytes();
    dbtest::TempDir dir("fin-replace-fallback");
    const std::string out = dir.file("world.db");
    const Bytes old = write_old_output(out);
    REQUIRE(old != expected);
    FsFaults faults;
    faults.failing_renames = 1;
    const Status status = finalize_with(out, Overwrite::replace, faults);
    CHECK(code_name(status) == "ok");
    CHECK(dbtest::read_file(out) == expected);
    check_no_temporaries(out);
    CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"world.db"});
}

TEST_CASE("replace, first rename fails and removing the old output fails: old output unchanged") {
    dbtest::TempDir dir("fin-replace-noremove");
    const std::string out = dir.file("world.db");
    const Bytes old = write_old_output(out);
    FsFaults faults;
    faults.failing_renames = 1;
    faults.remove_output_error = true;
    const Status status = finalize_with(out, Overwrite::replace, faults);
    CHECK(code_name(status) == code_name(ErrorCode::output_rename_failed));
    CHECK(dbtest::read_file(out) == old);
    check_no_temporaries(out);
    CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"world.db"});
}

TEST_CASE("replace, both renames fail, old output present: output_rename_failed, no .partial") {
    const Bytes expected = normal_world_bytes();
    dbtest::TempDir dir("fin-replace-tworenames");
    const std::string out = dir.file("world.db");
    const Bytes old = write_old_output(out);
    FsFaults faults;
    faults.failing_renames = 2;
    const Status status = finalize_with(out, Overwrite::replace, faults);
    CHECK(code_name(status) == code_name(ErrorCode::output_rename_failed));
    check_no_temporaries(out);
    // testing.hpp: an error means <out>.partial is removed. The fallback has
    // removed the old <out> before the second rename failed, so <out> may be
    // gone; it is never the new world or anything but the old bytes.
    // (writer.hpp still says an existing <out> "is never touched unless the
    // final rename replaces it", which this case contradicts; reported.)
    if (dbtest::exists(out)) {
        CHECK(dbtest::read_file(out) == old);
    }
    const auto entries = dbtest::dir_entries(dir.path());
    CHECK((entries.empty() || entries == std::set<std::string>{"world.db"}));
    const std::string note = dbtest::exists(out) ? "old output still present after two failed renames"
                                                 : "old output removed after two failed renames";
    MESSAGE(note);
}

TEST_CASE("refuse, rename fails: output_rename_failed, no output, no .partial") {
    dbtest::TempDir dir("fin-refuse-rename");
    const std::string out = dir.file("world.db");
    FsFaults faults;
    faults.failing_renames = 1;
    const Status status = finalize_with(out, Overwrite::refuse, faults);
    CHECK(code_name(status) == code_name(ErrorCode::output_rename_failed));
    CHECK_FALSE(dbtest::exists(out));
    check_no_temporaries(out);
    CHECK(dbtest::dir_entries(dir.path()).empty());
}

TEST_CASE("refuse, the existence check of the output fails: output_write_failed, nothing left") {
    dbtest::TempDir dir("fin-refuse-status");
    const std::string out = dir.file("world.db");
    FsFaults faults;
    faults.output_status_error = true;
    const Status status = finalize_with(out, Overwrite::refuse, faults);
    CHECK(code_name(status) == code_name(ErrorCode::output_write_failed));
    CHECK_FALSE(dbtest::exists(out));
    check_no_temporaries(out);
    CHECK(dbtest::dir_entries(dir.path()).empty());
}

TEST_CASE("stale <out>.partial and <out>.partial-journal are removed and do not affect the output") {
    const Bytes expected = normal_world_bytes();
    for (const Overwrite overwrite : {Overwrite::refuse, Overwrite::replace}) {
        CAPTURE(static_cast<int>(overwrite));
        dbtest::TempDir dir("fin-stale-both");
        const std::string out = dir.file("world.db");
        // A stale partial world (another seed) and a junk journal beside it.
        REQUIRE(dbtest::write_empty_world(out + ".partial", 77).ok());
        dbtest::write_file(out + ".partial-journal", Bytes(1536, 0x5A));
        const Status status = finalize_with(out, overwrite, FsFaults{});
        CHECK(code_name(status) == "ok");
        CHECK(dbtest::read_file(out) == expected);
        check_no_temporaries(out);
        CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"world.db"});
    }
}

TEST_CASE("refuse with an existing output removes a stale <out>.partial-journal and keeps <out>") {
    dbtest::TempDir dir("fin-stale-journal-exists");
    const std::string out = dir.file("world.db");
    const Bytes old = write_old_output(out);
    dbtest::write_file(out + ".partial-journal", Bytes(512, 0x11));
    const Status status = finalize_with(out, Overwrite::refuse, FsFaults{});
    CHECK(code_name(status) == code_name(ErrorCode::output_exists));
    CHECK_FALSE(dbtest::exists(out + ".partial-journal"));
    CHECK(dbtest::read_file(out) == old);
    CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"world.db"});
}

TEST_CASE("invalid output and input paths are errors, nothing is written, nothing throws") {
    dbtest::TempDir dir("fin-invalid-paths");
    const std::string base = worldgen::db::path_to_utf8(dir.path());

    SUBCASE("finalize") {
        const std::string invalid_utf8 = base + "/\xFF.db";
        const std::string with_nul = base + "/a" + std::string(1, '\0') + "b.db";
        const std::string paths[] = {invalid_utf8, with_nul, std::string()};
        int index = 0;
        for (const std::string& path : paths) {
            CAPTURE(index++);
            for (const Overwrite overwrite : {Overwrite::refuse, Overwrite::replace}) {
                Status status = Status(worldgen::db::Ok{});
                CHECK_NOTHROW(status = finalize_with(path, overwrite, FsFaults{}));
                CHECK(code_name(status) == code_name(ErrorCode::output_write_failed));
                Status check = Status(worldgen::db::Ok{});
                CHECK_NOTHROW(check = WorldWriter::check_output(path, overwrite));
                CHECK(code_name(check) == code_name(ErrorCode::output_write_failed));
            }
        }
        CHECK(dbtest::dir_entries(dir.path()).empty());
    }
    SUBCASE("check_output with an overlong NUL") {
        Status check = Status(worldgen::db::Ok{});
        CHECK_NOTHROW(check = WorldWriter::check_output("\xC0\x80", Overwrite::refuse));
        CHECK(code_name(check) == code_name(ErrorCode::output_write_failed));
    }
    SUBCASE("WorldReader::open") {
        const std::string paths[] = {"\xED\xA0\x80.db", base + "/\xFF.db",
                                     base + "/a" + std::string(1, '\0') + "b.db", std::string()};
        int index = 0;
        for (const std::string& path : paths) {
            CAPTURE(index++);
            bool threw = false;
            std::string code;
            try {
                const auto r = WorldReader::open(path);
                code = r.ok() ? std::string("ok") : code_name(r.code());
            } catch (...) {
                threw = true;
            }
            CHECK_FALSE(threw);
            CHECK(code == code_name(ErrorCode::file_unreadable));
        }
        CHECK(dbtest::dir_entries(dir.path()).empty());
    }
}

TEST_CASE("is_valid_utf8_path") {
    using worldgen::db::is_valid_utf8_path;
    CHECK(is_valid_utf8_path("world.db"));
    CHECK(is_valid_utf8_path("w\xC3\xB6rld.db"));             // 2-byte: U+00F6
    CHECK(is_valid_utf8_path("\xE5\x9C\xB0\xE5\x9B\xB3.db"));  // 3-byte: U+5730 U+56F3
    CHECK(is_valid_utf8_path("\xF0\x9F\x97\xBA.db"));         // 4-byte: U+1F5FA
    CHECK(is_valid_utf8_path("\xF4\x8F\xBF\xBF"));            // U+10FFFF, the largest
    CHECK(is_valid_utf8_path("\xEF\xBF\xBF"));                // U+FFFF
    CHECK_FALSE(is_valid_utf8_path("\xF4\x90\x80\x80"));      // above U+10FFFF
    CHECK_FALSE(is_valid_utf8_path("\xE0\x80\x80"));          // overlong
    CHECK_FALSE(is_valid_utf8_path("\xC0\x80"));              // overlong NUL
    CHECK_FALSE(is_valid_utf8_path("\xC3"));                  // truncated
    CHECK_FALSE(is_valid_utf8_path("a\xC3"));                 // truncated at the end
    CHECK_FALSE(is_valid_utf8_path("\xED\xA0\x80"));          // surrogate U+D800
    CHECK_FALSE(is_valid_utf8_path("\xFF.db"));
    CHECK_FALSE(is_valid_utf8_path("\x80"));                  // lone continuation byte
    CHECK_FALSE(is_valid_utf8_path(std::string_view("a\0b", 3)));  // embedded NUL
    CHECK_FALSE(is_valid_utf8_path(std::string_view("\0", 1)));
    CHECK_FALSE(is_valid_utf8_path(""));
}

}  // TEST_SUITE
