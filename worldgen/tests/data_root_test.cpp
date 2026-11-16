// Data root resolution (spec P0-03 section 7.6 and amendments 9 and 15;
// P1-00 section 6.3): --data, then UNMAPPED_DATA_DIR, then the root
// recorded at configure time; and, while no kind is registered, a data root
// that does not exist changes nothing in a world.
//
// Written from the specs and the public headers worldgen/data/data_root.hpp,
// worldgen/data/content.hpp and worldgen/generate.hpp only.

#include <doctest/doctest.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "data_loader_support.hpp"
#include "db_test_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/data.hpp"
#include "worldgen/generate.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must give the repository root (worldgen/tests/CMakeLists.txt)"
#endif

namespace wd = worldgen::data;
using wd::DataRootSource;

namespace {

std::string expected_configured_root() { return std::string(UNMAPPED_TEST_SOURCE_ROOT) + "/game/data"; }

}  // namespace

TEST_CASE("data root 7.6: the environment variable name") { CHECK(wd::DATA_DIR_ENV == "UNMAPPED_DATA_DIR"); }

TEST_CASE("data root 7.6: the configured root is game/data in the source tree, '/' separated, no trailing separator") {
    const std::string_view configured = wd::configured_data_root();
    INFO("configured_data_root() = ", std::string(configured));
    REQUIRE_FALSE(configured.empty());
    CHECK(configured == expected_configured_root());
    CHECK(configured.find('\\') == std::string_view::npos);
    CHECK(configured.back() != '/');
}

TEST_CASE("data root 7.6: --data wins over the environment and the configured root") {
    const wd::DataRoot a = wd::resolve_data_root(std::string_view("/from/flag"), std::string_view("/from/env"));
    CHECK(a.source == DataRootSource::flag);
    CHECK(a.path == "/from/flag");
    const wd::DataRoot b = wd::resolve_data_root(std::string_view("relative/dir"), std::nullopt);
    CHECK(b.source == DataRootSource::flag);
    CHECK(b.path == "relative/dir");
    // The flag text is taken as given (UTF-8, not normalized).
    const wd::DataRoot c = wd::resolve_data_root(std::string_view("d\xC3\xA4ta/./x/"), std::string_view("/e"));
    CHECK(c.source == DataRootSource::flag);
    CHECK(c.path == "d\xC3\xA4ta/./x/");
}

TEST_CASE("data root 7.6: without --data, UNMAPPED_DATA_DIR if set and non-empty") {
    const wd::DataRoot a = wd::resolve_data_root(std::nullopt, std::string_view("/from/env"));
    CHECK(a.source == DataRootSource::environment);
    CHECK(a.path == "/from/env");
    // Set but empty: as if unset.
    const wd::DataRoot b = wd::resolve_data_root(std::nullopt, std::string_view(""));
    CHECK(b.source == DataRootSource::configured);
    CHECK(b.path == std::string(wd::configured_data_root()));
}

TEST_CASE("data root 7.6: otherwise the configured root") {
    const wd::DataRoot r = wd::resolve_data_root(std::nullopt, std::nullopt);
    CHECK(r.source == DataRootSource::configured);
    CHECK(r.path == expected_configured_root());
}

TEST_CASE("data root 7.6, appendix C 16: an empty --data value is no data root") {
    // data_root.hpp: a flag given but empty resolves to no data root (path
    // empty, source none), never falling back to the environment or the
    // configured root, whether or not the environment value is set.
    for (const std::optional<std::string_view> env :
         {std::optional<std::string_view>(std::string_view("/from/env")),
          std::optional<std::string_view>(std::string_view("")), std::optional<std::string_view>()}) {
        CAPTURE(env.has_value());
        const wd::DataRoot r = wd::resolve_data_root(std::string_view(""), env);
        CHECK(r.source == DataRootSource::none);
        CHECK(r.path.empty());
    }
}

TEST_CASE("data root 7.6: the path is empty exactly when the source is none") {
    using O = std::optional<std::string_view>;
    for (const O flag : {O(), O(std::string_view("")), O(std::string_view("/f")), O(std::string_view("rel"))}) {
        for (const O env : {O(), O(std::string_view("")), O(std::string_view("/e"))}) {
            const wd::DataRoot r = wd::resolve_data_root(flag, env);
            CAPTURE(flag.has_value());
            CAPTURE(env.has_value());
            CHECK(r.path.empty() == (r.source == DataRootSource::none));
        }
    }
}

TEST_CASE("data root, amendment 9: with no kind registered a nonexistent root is not opened and changes nothing") {
    const wd::KindRegistry none;
    loader_test::ScratchDir scratch;
    const auto missing = wd::load_content(none, scratch.path() / "does-not-exist");
    INFO(loader_test::error_dump(missing));
    REQUIRE(missing.ok());
    CHECK(missing.value->kinds.empty());
    const auto file_root = wd::load_content(none, scratch.write("a-file", "not a directory"));
    REQUIRE(file_root.ok());
    CHECK(*file_root.value == *missing.value);
    CHECK(wd::content_hash_hex(*missing.value) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // A world generated with that content is byte-identical to the world
    // generated without any content (the P0 empty world).
    dbtest::TempDir dir("p1-data-root-nonexistent");
    worldgen::WorldParams p;
    p.seed = 0;
    p.content_hash = wd::content_hash(*missing.value);
    p.until = std::string("none");
    REQUIRE(worldgen::generate_world(p, *missing.value, dir.file("with.db"), worldgen::db::Overwrite::refuse).ok());
    worldgen::WorldParams q;
    q.seed = 0;
    q.content_hash = worldgen::sha256(std::string_view{});
    q.until = std::string("none");
    REQUIRE(worldgen::generate_world(q, dir.file("without.db"), worldgen::db::Overwrite::refuse).ok());
    CHECK(dbtest::read_file(dir.file("with.db")) == dbtest::read_file(dir.file("without.db")));
}
