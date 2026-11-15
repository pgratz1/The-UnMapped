// P0-03 TOML loader: error collection, format and order (spec P0-03
// sections 7.5 and 9, 13 test 7, amendment 10).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "data_loader_support.hpp"

using namespace loader_test;

namespace {

wd::LoadError err(std::string path, std::uint32_t line, std::uint32_t column, std::string message) {
    wd::LoadError e;
    e.path = std::move(path);
    e.line = line;
    e.column = column;
    e.message = std::move(message);
    return e;
}

// Errors in b.toml line 3, a.toml line 9 and a.toml line 2, the same text
// as the committed fixture worldgen/tests/data/loader/errors/.
const char* const ERRORS_A = "[alpha.a_one]\ncount = 101\npartner = \"b1\"\n\n[alpha.a_two]\ncount = 5\n"
                             "partner = \"b1\"\nmode = \"fast\"\nenabled = \"yes\"\n";
const char* const ERRORS_B = "[alpha.b_one]\npartner = \"b1\"\ncount = 0.5\n";

void check_test7_errors(const wd::LoadResult<wd::ContentSet>& r) {
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.errors.size() == 3);
    CHECK(r.errors[0].path == "alphas/a.toml");
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column == 9);
    CHECK(r.errors[1].path == "alphas/a.toml");
    CHECK(r.errors[1].line == 9);
    CHECK(r.errors[1].column == 11);
    CHECK(r.errors[2].path == "alphas/b.toml");
    CHECK(r.errors[2].line == 3);
    CHECK(r.errors[2].column == 9);
    CHECK(r.errors[0].to_string().rfind("alphas/a.toml:2:9: error: ", 0) == 0);
    CHECK(r.errors[1].to_string().rfind("alphas/a.toml:9:11: error: ", 0) == 0);
    CHECK(r.errors[2].to_string().rfind("alphas/b.toml:3:9: error: ", 0) == 0);
    CHECK(contains(r.errors[0].message, "alpha.a_one.count"));
    CHECK(contains(r.errors[1].message, "alpha.a_two.enabled"));
    CHECK(contains(r.errors[2].message, "alpha.b_one.count"));
    CHECK(contains(r.errors[2].message, "floating-point"));
    for (const auto& e : r.errors) {
        CHECK(e.to_string().find('\n') == std::string::npos);
        CHECK(e.path.find('\\') == std::string::npos);
    }
}

}  // namespace

TEST_CASE("loader errors: all errors collected and sorted a:2, a:9, b:3 (test 7)") {
    SUBCASE("committed fixture on disk") {
        check_test7_errors(wd::load_content(alpha_beta_registry(), fixture("errors")));
    }
    SUBCASE("in memory, listed b before a") {
        check_test7_errors(wd::load_content_from_memory(
            alpha_beta_registry(), alphas_root({file("b.toml", ERRORS_B), file("a.toml", ERRORS_A)})));
    }
}

TEST_CASE("loader errors: a syntax error stops only its own file; every kind is checked") {
    wd::MemoryDataRoot root;
    root.directories.push_back(dir("alphas", {file("a.toml", "[alpha.x]\ncount = = 1\nlabel = 5\n"),
                                              file("b.toml", "[alpha.y]\ncount = 1\npartner = \"b1\"\nmode = 3\n")}));
    root.directories.push_back(dir("betas", {file("b.toml", "[beta.b1]\nweight = 1001\n")}));
    const auto r = wd::load_content_from_memory(alpha_beta_registry(), root);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 3);
    CHECK(r.errors[0].path == "alphas/a.toml");
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[1].path == "alphas/b.toml");
    CHECK(r.errors[1].line == 4);
    CHECK(r.errors[2].path == "betas/b.toml");
    CHECK(r.errors[2].line == 2);
}

TEST_CASE("loader errors: one bad definition fails the whole load, no partial set") {
    const auto r = load_alpha_text("[alpha.good]\ncount = 1\npartner = \"b1\"\n[alpha.bad]\ncount = 1000\n"
                                   "partner = \"b1\"\n");
    check_content_result(r);
    CHECK_FALSE(r.ok());
    CHECK_FALSE(r.value.has_value());
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].line == 5);
}

TEST_CASE("loader errors: format with and without a position (section 9)") {
    CHECK(err("alphas/a.toml", 3, 7, "bad thing").to_string() == "alphas/a.toml:3:7: error: bad thing");
    CHECK(err("betas", 0, 0, "missing").to_string() == "betas: error: missing");
}

TEST_CASE("loader errors: sort by path bytes, line, column, message; positionless first; deduplicated") {
    std::vector<wd::LoadError> v = {
        err("b.toml", 1, 1, "m"),       err("a.toml", 9, 1, "m"),  err("a.toml", 2, 5, "z"),
        err("a.toml", 2, 5, "a"),       err("a.toml", 2, 1, "m"),  err("a.toml", 0, 0, "whole file"),
        err("a.toml", 2, 5, "a"),       err("B.toml", 5, 5, "m"),  err("a.toml", 10, 1, "m"),
        err("a.toml", 0, 0, "whole file"),
    };
    wd::sort_errors(v);
    const std::vector<wd::LoadError> expected = {
        err("B.toml", 5, 5, "m"),  // 'B' (0x42) sorts before 'a' (0x61)
        err("a.toml", 0, 0, "whole file"), err("a.toml", 2, 1, "m"), err("a.toml", 2, 5, "a"),
        err("a.toml", 2, 5, "z"), err("a.toml", 9, 1, "m"),
        err("a.toml", 10, 1, "m"),  // numeric, not text, order of lines
        err("b.toml", 1, 1, "m"),
    };
    CHECK(v == expected);

    CHECK(wd::error_less(err("a", 0, 0, "z"), err("a", 1, 1, "a")));
    CHECK_FALSE(wd::error_less(err("a", 1, 1, "a"), err("a", 1, 1, "a")));
    CHECK(wd::error_less(err("a", 1, 2, "a"), err("a", 1, 10, "a")));
    CHECK(wd::error_less(err("a/b", 1, 1, "a"), err("a0", 1, 1, "a")));  // '/' (0x2F) < '0' (0x30)
}
