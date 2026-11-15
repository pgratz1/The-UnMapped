// P0-03 settings file (spec P0-03 sections 4 to 6, 13 test 9, amendments 1,
// 4, 6 and 14; seed phrases are scrubbed per P0-01 section 7.3).

#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "data_loader_support.hpp"
#include "worldgen/core/hash.hpp"

using namespace loader_test;

namespace {

constexpr std::string_view PATH = "my-settings.toml";

wd::LoadResult<wd::Settings> parse(const std::string& text) {
    auto r = wd::parse_settings(PATH, text);
    check_result_shape(r);
    for (const auto& e : r.errors) {
        CHECK(e.path == PATH);
    }
    return r;
}

wd::Settings parsed(const std::string& text) {
    const auto r = parse(text);
    INFO(text << "\n" << error_dump(r));
    REQUIRE(r.ok());
    return *r.value;
}

std::uint64_t seed_of(const std::string& value_literal) {
    const auto s = parsed("[world]\nseed = " + value_literal + "\n");
    REQUIRE(s.seed.has_value());
    CHECK_FALSE(s.seed_phrase.has_value());
    return *s.seed;
}

// One error, on `line`.
wd::LoadError only_error(const std::string& text, std::uint32_t line) {
    const auto r = parse(text);
    INFO(text << "\n" << error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].line == line);
    return r.errors[0];
}

constexpr std::uint64_t U64_MAX = std::numeric_limits<std::uint64_t>::max();

}  // namespace

TEST_CASE("settings: seed forms (test 9)") {
    CHECK(seed_of("42") == 42);
    CHECK(seed_of("\"42\"") == 42);
    CHECK(seed_of("\"0x2A\"") == 42);
    CHECK(seed_of("\"0x2a\"") == 42);
    CHECK(seed_of("0x2A") == 42);  // TOML hex integer
    CHECK(seed_of("0") == 0);
    CHECK(seed_of("\"0\"") == 0);
    CHECK(seed_of("\"0x0\"") == 0);
    CHECK(seed_of("\"0x0000000000000001\"") == 1);
    CHECK(seed_of("9223372036854775807") == 9223372036854775807u);
    CHECK(seed_of("\"18446744073709551615\"") == U64_MAX);
    CHECK(seed_of("\"0xFFFFFFFFFFFFFFFF\"") == U64_MAX);
    CHECK(seed_of("\"0xffffffffffffffff\"") == U64_MAX);
    CHECK(seed_of("'42'") == 42);
}

TEST_CASE("settings: invalid seeds are errors at the value") {
    for (const char* lit : {"\"18446744073709551616\"", "\"99999999999999999999\"", "\"-1\"", "\"042\"", "\"00\"",
                            "\"0x\"", "\"0x1_0\"", "\"0X2A\"", "\"0x00000000000000001\"", "\"+42\"", "\" 42\"",
                            "\"42 \"", "\"\"", "\"4_2\"", "\"0x2G\"", "\"42.0\"", "-1", "-9223372036854775808",
                            "true", "[42]", "{ v = 42 }"}) {
        INFO(std::string(lit));
        const auto e = only_error(std::string("[world]\nseed = ") + lit + "\n", 2);
        CHECK(e.column == 8);
        CHECK(contains(e.message, "world.seed"));
    }
    const auto ef = only_error("[world]\nseed = 42.0\n", 2);
    CHECK(contains(ef.message, "floating-point"));
    const auto ed = only_error("[world]\nseed = 1979-05-27\n", 2);
    CHECK(contains(ed.message, "date and time values are not allowed"));
}

TEST_CASE("settings: parse_seed_text") {
    CHECK(wd::parse_seed_text("42") == std::optional<std::uint64_t>{42});
    CHECK(wd::parse_seed_text("0x2A") == std::optional<std::uint64_t>{42});
    CHECK(wd::parse_seed_text("18446744073709551615") == std::optional<std::uint64_t>{U64_MAX});
    CHECK(wd::parse_seed_text("0") == std::optional<std::uint64_t>{0});
    for (const char* bad : {"18446744073709551616", "-1", "042", "0x", "0x1_0", "0X2A", "", " 1", "1 ", "+1",
                            "0x10000000000000000", "1e3", "0b1", "0o7"}) {
        INFO(std::string(bad));
        CHECK_FALSE(wd::parse_seed_text(bad).has_value());
    }
}

TEST_CASE("settings: seed_phrase is scrubbed and gives the P0-01 phrase seed (amendment 14)") {
    const auto s = parsed("[world]\nseed_phrase = \"  The Unmapped\\t \"\n");
    REQUIRE(s.seed_phrase.has_value());
    CHECK(*s.seed_phrase == "the unmapped");
    const auto expected = worldgen::seed_from_phrase("The Unmapped");
    REQUIRE(expected.ok());
    REQUIRE(s.seed.has_value());
    CHECK(*s.seed == expected.seed);
    CHECK(*s.seed == worldgen::phrase_seed("the unmapped"));

    // Inner TAB and LF (and CR LF, which becomes LF first) are separators.
    const auto inner = parsed("[world]\nseed_phrase = \"a\\tb\\r\\nc\"\n");
    REQUIRE(inner.seed_phrase.has_value());
    CHECK(*inner.seed_phrase == "a b c");
    CHECK(*inner.seed == worldgen::phrase_seed("a b c"));

    // UTF-8 in a TOML string: each byte of a multi-byte character is a space.
    const auto utf8 = parsed("[world]\nseed_phrase = \"\xC3\x9Cnmapped W\xC3\xB6rld\"\n");
    REQUIRE(utf8.seed_phrase.has_value());
    CHECK(*utf8.seed_phrase == "nmapped w rld");
    CHECK(utf8 == parsed("[world]\nseed_phrase = \"nmapped w rld\"\n"));
    // The same through \u escapes.
    CHECK(parsed("[world]\nseed_phrase = \"\\u00DCnmapped W\\u00F6rld\"\n") == utf8);
}

TEST_CASE("settings: spellings that scrub alike give equal Settings (amendment 14)") {
    const auto base = parsed("[world]\nseed_phrase = \"ashen marches\"\nmap_size = 2048\n");
    REQUIRE(base.seed_phrase.has_value());
    CHECK(*base.seed_phrase == "ashen marches");
    REQUIRE(base.seed.has_value());
    CHECK(*base.seed == worldgen::phrase_seed("ashen marches"));
    CHECK(base.map_size == 2048);
    for (const char* lit : {"\"Ashen Marches!\"", "\"ashen-marches\"", "\"ASHEN  MARCHES\"", "'Ashen Marches!'",
                            "\"  ashen\\tmarches\\n\"", "\"\"\"\nAshen\nMarches\n\"\"\"",
                            "'''...ASHEN marches'''"}) {
        INFO(std::string(lit));
        const auto s = parsed(std::string("[world]\nseed_phrase = ") + lit + "\nmap_size = 2048\n");
        CHECK(s.seed_phrase == base.seed_phrase);
        CHECK(s.seed == base.seed);
        CHECK(s == base);
    }
    // A different scrubbed phrase is a different world.
    CHECK_FALSE(parsed("[world]\nseed_phrase = \"ashen marche\"\nmap_size = 2048\n") == base);
}

TEST_CASE("settings: seed_phrase = \"42\" is the phrase 42, not the number (amendment 14)") {
    const auto p = parsed("[world]\nseed_phrase = \"42\"\n");
    REQUIRE(p.seed_phrase.has_value());
    CHECK(*p.seed_phrase == "42");
    REQUIRE(p.seed.has_value());
    CHECK(*p.seed == worldgen::phrase_seed("42"));
    CHECK(*p.seed != 42);
    const auto n = parsed("[world]\nseed = \"42\"\n");
    CHECK_FALSE(n.seed_phrase.has_value());
    CHECK(n.seed == std::optional<std::uint64_t>{42});
    CHECK_FALSE(p == n);
    // A hex-looking phrase is scrubbed text too.
    const auto h = parsed("[world]\nseed_phrase = \"0x2A\"\n");
    CHECK(h.seed_phrase == std::optional<std::string>{"0x2a"});
    CHECK(h.seed == std::optional<std::uint64_t>{worldgen::phrase_seed("0x2a")});
}

TEST_CASE("settings: a phrase that scrubs to nothing or to more than 256 bytes is an error at the value") {
    // "seed_phrase = " is 14 bytes, so the value is at column 15.
    const std::vector<std::string> literals = {
        "\"\"",
        "\" \\t\\n \"",
        "\"!!!\"",
        "'-_-'",
        "\"\xE6\x9D\xB1\xE4\xBA\xAC\"",  // only a non-Latin script ("東京")
        "\"\\u00E9\\u00E8\"",            // only accented letters
        "\"" + std::string(257, 'p') + "\"",
        "\"" + std::string(257, 'P') + "\"",
        "\"a" + std::string(254, 'b') + "-c!\"",  // "ab...b c": 257 bytes after scrubbing
    };
    for (const std::string& lit : literals) {
        INFO(lit);
        const auto e = only_error("[world]\nseed_phrase = " + lit + "\n", 2);
        CHECK(e.column == 15);
    }
    // The limit is on the scrubbed phrase: 256 bytes are fine, also when the
    // text as written is longer.
    const auto long_ok = parsed("[world]\nseed_phrase = \" " + std::string(256, 'p') + " \"\n");
    REQUIRE(long_ok.seed_phrase.has_value());
    CHECK(*long_ok.seed_phrase == std::string(256, 'p'));
    const auto padded =
        parsed("[world]\nseed_phrase = \"" + std::string(1000, '!') + "Short" + std::string(1000, '.') + "\"\n");
    CHECK(padded.seed_phrase == std::optional<std::string>{"short"});
    // Not a string.
    only_error("[world]\nseed_phrase = 5\n", 2);
    only_error("[world]\nseed_phrase = 42\n", 2);
}

TEST_CASE("settings: the string rule applies before scrubbing (amendments 4 and 14)") {
    // A control character other than TAB or LF is an error in a settings file,
    // though the command line would scrub it to a space.
    for (const char* lit : {"\"a\\u0001b\"", "\"\\u0000x\"", "\"a\\rb\"", "\"a\\u007Fb\"", "\"a\\u001Fb\"",
                            "\"a\\bb\"", "\"a\\fb\""}) {
        INFO(std::string(lit));
        only_error(std::string("[world]\nseed_phrase = ") + lit + "\n", 2);
    }
    // TAB and LF are allowed and scrub to spaces.
    CHECK(parsed("[world]\nseed_phrase = \"a\\tb\\nc\"\n").seed_phrase == std::optional<std::string>{"a b c"});
}

TEST_CASE("settings: seed and seed_phrase together are an error at seed_phrase (amendment 6)") {
    const auto e = only_error("[world]\nseed = 42\nseed_phrase = \"x\"\n", 3);
    CHECK(e.path == PATH);
    // Written in the other order: still at seed_phrase.
    only_error("[world]\nseed_phrase = \"x\"\nseed = 42\n", 2);

    // Each value is still validated.
    const auto r = parse("[world]\nseed = \"bad\"\nseed_phrase = \"a\\u0001\"\n");
    INFO(error_dump(r));
    REQUIRE_FALSE(r.ok());
    bool seed_line = false;
    bool phrase_line = false;
    for (const auto& err : r.errors) {
        seed_line = seed_line || err.line == 2;
        phrase_line = phrase_line || err.line == 3;
    }
    CHECK(seed_line);
    CHECK(phrase_line);
}

TEST_CASE("settings: map_size") {
    CHECK(parsed("[world]\nmap_size = 512\n").map_size == 512);
    CHECK(parsed("[world]\nmap_size = 1024\n").map_size == 1024);
    CHECK(parsed("[world]\nmap_size = 2048\n").map_size == 2048);
    CHECK(parsed("[world]\nmap_size = 0x800\n").map_size == 2048);
    for (const char* lit : {"1000", "0", "-1024", "4096", "256", "\"1024\"", "true"}) {
        INFO(std::string(lit));
        const auto e = only_error(std::string("[world]\nmap_size = ") + lit + "\n", 2);
        CHECK(e.column == 12);
        CHECK(contains(e.message, "world.map_size"));
    }
    const auto ef = only_error("[world]\nmap_size = 1024.0\n", 2);
    CHECK(contains(ef.message, "floating-point"));
    CHECK(wd::is_valid_map_size(512));
    CHECK(wd::is_valid_map_size(1024));
    CHECK(wd::is_valid_map_size(2048));
    CHECK_FALSE(wd::is_valid_map_size(1000));
    CHECK_FALSE(wd::is_valid_map_size(0));
    CHECK_FALSE(wd::is_valid_map_size(-512));
}

TEST_CASE("settings: unknown keys and tables are errors") {
    const auto e = only_error("[world]\nsize = 1\n", 2);
    CHECK(e.column == 8);
    CHECK(contains(e.message, "world.size"));
    for (const char* text : {"[other]\nx = 1\n", "seed = 42\n", "[world.sub]\nx = 1\n", "world = 3\n",
                             "[World]\nseed = 42\n", "[world]\nSeed = 42\n", "[[world]]\nseed = 42\n"}) {
        INFO(std::string(text));
        const auto r = parse(text);
        INFO(error_dump(r));
        CHECK_FALSE(r.ok());
    }
    // A float under an unknown key is reported as floating-point too.
    const auto rf = parse("[world]\nratio = 0.5\n");
    bool fp = false;
    for (const auto& err : rf.errors) {
        fp = fp || contains(err.message, "floating-point");
    }
    CHECK(fp);
}

TEST_CASE("settings: defaults") {
    // No settings file: the defaults.
    const wd::Settings defaults;
    CHECK(defaults.map_size == 1024);
    CHECK(wd::DEFAULT_MAP_SIZE == 1024);
    CHECK_FALSE(defaults.seed.has_value());
    CHECK_FALSE(defaults.seed_phrase.has_value());
    // An empty file, a comment, an empty [world]: the same.
    for (const char* text : {"", "# nothing\n", "[world]\n", "\xEF\xBB\xBF[world]\r\n"}) {
        INFO(std::string(text));
        CHECK(parsed(text) == defaults);
    }
    const auto all = parsed("[world]\nseed = 7\nmap_size = 512\n");
    CHECK(all.seed == std::optional<std::uint64_t>{7});
    CHECK(all.map_size == 512);
}

TEST_CASE("settings: syntax errors and limits") {
    const auto r = parse("[world]\nseed = = 1\n");
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column >= 1);
    CHECK(r.errors[0].column <= 11);

    const std::size_t mib = std::size_t{1} << 20;
    const std::string head = "[world]\nseed = 1\n";
    const std::string exact = head + "#" + std::string(mib - head.size() - 2, 'x') + "\n";
    REQUIRE(exact.size() == mib);
    CHECK(parse(exact).ok());
    const std::string over = head + "#" + std::string(mib - head.size() - 1, 'x') + "\n";
    REQUIRE(over.size() == mib + 1);
    CHECK(parse(over).errors.size() == 1);
}

TEST_CASE("settings: files on disk") {
    ScratchDir scratch;
    SUBCASE("CRLF and a byte-order mark") {
        const auto p = scratch.write("s.toml", "\xEF\xBB\xBF[world]\r\nseed = \"0x2A\"\r\nmap_size = 2048\r\n");
        const auto r = wd::load_settings_file(p);
        check_result_shape(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(r.value->seed == std::optional<std::uint64_t>{42});
        CHECK(r.value->map_size == 2048);
    }
    SUBCASE("errors carry the path as given") {
        const auto p = scratch.write("bad.toml", "[world]\nmap_size = 1000\n");
        const auto r = wd::load_settings_file(p);
        check_result_shape(r);
        REQUIRE(r.errors.size() == 1);
        CHECK(contains(r.errors[0].path, "bad.toml"));
        CHECK(r.errors[0].line == 2);
    }
    SUBCASE("a file that cannot be opened is an error") {
        const auto r = wd::load_settings_file(scratch.path() / utf8_path("missing.toml"));
        check_result_shape(r);
        REQUIRE_FALSE(r.ok());
        CHECK(contains(r.errors[0].path, "missing.toml"));
    }
    SUBCASE("above 1 MiB") {
        const std::size_t mib = std::size_t{1} << 20;
        const auto p = scratch.write("big.toml", "[world]\n#" + std::string(mib - 8, 'x') + "\n");
        const auto r = wd::load_settings_file(p);
        check_result_shape(r);
        CHECK(r.errors.size() == 1);
    }
}

// --- Extreme float literals (determinism audit; toml++ number conversion
// pinned to std::from_chars). Line 2 holds the literal, line 3 an unknown
// key. An overflowing or underflowing literal is a syntax error, which stops
// the file (amendment 11: path, line, sole error, column within the line;
// never the parser's text). A float that converts is the floating-point
// error, and validation continues to the unknown key.

namespace {

std::string extreme_settings(std::string_view literal) {
    return "[world]\nmap_size = " + std::string(literal) + "\nsize = 1\n";
}

void check_extreme_settings_float(std::string_view literal) {
    const auto r = parse(extreme_settings(literal));
    INFO(std::string(literal) << "\n" << error_dump(r));
    REQUIRE(r.errors.size() == 2);
    CHECK(r.errors[0].path == PATH);
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column == 12);
    CHECK(contains(r.errors[0].message, "floating-point"));
    CHECK(r.errors[1].path == PATH);
    CHECK(r.errors[1].line == 3);
    CHECK(r.errors[1].column == 8);
    CHECK(contains(r.errors[1].message, "world.size"));
}

}  // namespace

TEST_CASE("settings: float literals out of double range are syntax errors that stop the file") {
    for (const char* lit : {"1e999", "1e-400"}) {
        const std::string literal = lit;
        const auto r = parse(extreme_settings(literal));
        INFO(literal << "\n" << error_dump(r));
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].path == PATH);
        CHECK(r.errors[0].line == 2);
        CHECK(r.errors[0].column >= 1);
        CHECK(r.errors[0].column <= 12 + literal.size());  // "map_size = " + literal, plus one
        CHECK_FALSE(contains(r.errors[0].message, "world.size"));
    }
}

TEST_CASE("settings: an ordinary float literal is the floating-point error, then the unknown key") {
    check_extreme_settings_float("0.5");
}

// Probes whether MSVC's std::from_chars treats a subnormal literal the way
// libstdc++'s does (converted, so the loader reports it as a float rather
// than the parser rejecting it). If Windows CI disagrees, the lead decides
// whether to drop this case or amend the spec.
TEST_CASE("settings: a subnormal float literal (1e-310) is the floating-point error, then the unknown key") {
    check_extreme_settings_float("1e-310");
}
