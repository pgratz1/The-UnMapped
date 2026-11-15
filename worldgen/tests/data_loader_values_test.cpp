// P0-03 TOML loader: value rules, validation, references and limits, from
// an in-memory data root (spec P0-03 sections 4, 5, 7.3 to 7.5, 9, 13 tests
// 1, 2, 5, 6 and 10, amendments 1 to 3, 5, 8, 10 to 12).
//
// Every case loads alphas/a.toml (the text under test) and betas/b.toml
// (beta.b1, weight 7) with the alpha and beta kinds of section 13.
// Positions: lines and columns are 1-based; columns are those of the
// offending value (section 9, amendment 10).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "data_loader_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"

using namespace loader_test;
using worldgen::FatalError;
using worldgen::Fx;

namespace {

// "[alpha.x]\ncount = 1\npartner = \"b1\"\n" followed by `extra`, so the
// first extra line is line 4.
std::string x_with(std::string_view extra) {
    return std::string("[alpha.x]\ncount = 1\npartner = \"b1\"\n") + std::string(extra);
}

// A load that must fail with exactly one error; returns it.
wd::LoadError only_error(const std::string& text) {
    const auto r = load_alpha_text(text);
    check_content_result(r);
    INFO("text:\n" << text << "\nerrors:\n" << error_dump(r));
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.errors.size() == 1);
    return r.errors.front();
}

// A load that must succeed; returns alpha.x.
wd::Definition loaded_x(const std::string& text) {
    const auto r = load_alpha_text(text);
    check_content_result(r);
    INFO("text:\n" << text << "\nerrors:\n" << error_dump(r));
    REQUIRE(r.ok());
    return alpha_def(r, "x");
}

// The error is in alphas/a.toml at (line, column).
void check_at(const wd::LoadError& e, std::uint32_t line, std::uint32_t column) {
    INFO(e.to_string());
    CHECK(e.path == "alphas/a.toml");
    CHECK(e.line == line);
    CHECK(e.column == column);
}

bool any_error_contains(const wd::LoadResult<wd::ContentSet>& r, std::string_view words) {
    for (const auto& e : r.errors) {
        if (contains(e.message, words)) {
            return true;
        }
    }
    return false;
}

}  // namespace

// --- Test 1: the numeric rule (section 4, amendment 1) ----------------------

TEST_CASE("loader numeric rule: float literals are rejected with 'floating-point'") {
    // ratio = 0.5 on line 4; the value starts at column 9.
    const auto e = only_error(x_with("ratio = 0.5\n"));
    check_at(e, 4, 9);
    CHECK(contains(e.message, "floating-point"));
    CHECK(contains(e.message, "alpha.x.ratio"));
    // Amendment 1: the fixed example "0.6", whatever the literal was.
    CHECK(contains(e.message, "\"0.6\""));

    SUBCASE("inf and nan, any sign") {
        for (const char* lit : {"inf", "+inf", "-inf", "nan", "+nan", "-nan"}) {
            const auto ei = only_error(x_with(std::string("ratio = ") + lit + "\n"));
            INFO(std::string(lit));
            check_at(ei, 4, 9);
            CHECK(contains(ei.message, "floating-point"));
        }
    }
    SUBCASE("exponent and other float forms") {
        for (const char* lit : {"1e0", "1.0", "0.0", "5e-1", "1_0.0"}) {
            const auto ei = only_error(x_with(std::string("ratio = ") + lit + "\n"));
            INFO(std::string(lit));
            CHECK(contains(ei.message, "floating-point"));
        }
    }
    SUBCASE("a float in an int field is a floating-point error too") {
        const auto ei = only_error("[alpha.x]\ncount = 3.0\npartner = \"b1\"\n");
        check_at(ei, 2, 9);
        CHECK(contains(ei.message, "floating-point"));
    }
}

TEST_CASE("loader numeric rule: fx from decimal strings and integers") {
    CHECK(loaded_x(x_with("ratio = \"0.5\"\n")).record.get_fx("ratio").raw() == 2147483648);
    CHECK(loaded_x(x_with("ratio = 1\n")).record.get_fx("ratio") == Fx::ONE);
    CHECK(loaded_x(x_with("ratio = 0\n")).record.get_fx("ratio").raw() == 0);
    CHECK(loaded_x(x_with("ratio = \"1\"\n")).record.get_fx("ratio") == Fx::ONE);
    CHECK(loaded_x(x_with("ratio = \"1.0\"\n")).record.get_fx("ratio") == Fx::ONE);
    CHECK(loaded_x(x_with("ratio = \"+0.25\"\n")).record.get_fx("ratio").raw() == 1073741824);
    CHECK(loaded_x(x_with("ratio = \"-0\"\n")).record.get_fx("ratio").raw() == 0);
    // 18 fractional digits: RHA(123456789012345678 * 2^32 / 10^18).
    CHECK(loaded_x(x_with("ratio = \"0.123456789012345678\"\n")).record.get_fx("ratio").raw() == 530242871);
    // Absent: the default "0.5".
    CHECK(loaded_x(x_with("")).record.get_fx("ratio").raw() == 2147483648);
    // A literal string is a string too.
    CHECK(loaded_x(x_with("ratio = '0.5'\n")).record.get_fx("ratio").raw() == 2147483648);
}

TEST_CASE("loader numeric rule: fx errors") {
    // Out of [0, 1]; 19 fractional digits; strings outside the grammar;
    // integers out of range; other types.
    for (const char* lit : {"\"1.5\"", "\"0.1234567890123456789\"", "\"-0.1\"", "\"1.000000001\"", "2",
                            "-1", "3000000000", "\".5\"", "\"5.\"", "\"1e-1\"", "\" 0.5\"", "\"0.5 \"",
                            "\"0,5\"", "\"\"", "\"00.5\"", "\"0_5\"", "\"0x1\"", "true", "[1]", "{ a = 1 }"}) {
        INFO(std::string(lit));
        const auto e = only_error(x_with(std::string("ratio = ") + lit + "\n"));
        check_at(e, 4, 9);
        CHECK(contains(e.message, "alpha.x.ratio"));
        CHECK_FALSE(contains(e.message, "floating-point"));
    }
}

TEST_CASE("loader numeric rule: int fields accept every TOML integer syntax and nothing else") {
    struct Case {
        const char* literal;
        std::int64_t value;
    };
    for (const Case c : {Case{"1_0", 10}, Case{"0x10", 16}, Case{"0o20", 16}, Case{"0b10000", 16}, Case{"+7", 7},
                         Case{"0", 0}, Case{"100", 100}, Case{"0x6_4", 100}, Case{"-0", 0}}) {
        INFO(std::string(c.literal));
        const auto d = loaded_x(std::string("[alpha.x]\ncount = ") + c.literal + "\npartner = \"b1\"\n");
        CHECK(d.record.get_int("count") == c.value);
    }
    for (const char* lit : {"\"5\"", "101", "-1", "0x65", "9223372036854775807", "-9223372036854775808",
                            "true", "[1]"}) {
        INFO(std::string(lit));
        const auto e = only_error(std::string("[alpha.x]\ncount = ") + lit + "\npartner = \"b1\"\n");
        check_at(e, 2, 9);
        CHECK(contains(e.message, "alpha.x.count"));
    }
}

TEST_CASE("loader numeric rule: date and time values are rejected") {
    for (const char* lit : {"1979-05-27T07:32:00Z", "1979-05-27T00:32:00-07:00", "1979-05-27T07:32:00",
                            "1979-05-27", "07:32:00", "07:32:00.5"}) {
        INFO(std::string(lit));
        const auto e = only_error(x_with(std::string("ratio = ") + lit + "\n"));
        check_at(e, 4, 9);
        CHECK(contains(e.message, "date and time values are not allowed"));
    }
    const auto e = only_error(std::string("[alpha.x]\ncount = 1979-05-27\npartner = \"b1\"\n"));
    check_at(e, 2, 9);
    CHECK(contains(e.message, "date and time values are not allowed"));
}

TEST_CASE("loader: floats, dates and control characters are reported anywhere (amendment 2)") {
    SUBCASE("float under an unknown key") {
        const auto r = load_alpha_text(x_with("colour = 0.5\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(any_error_contains(r, "floating-point"));
    }
    SUBCASE("float deep under an unknown key") {
        const auto r = load_alpha_text(x_with("extra = { deep = [1, 2.5] }\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(any_error_contains(r, "floating-point"));
    }
    SUBCASE("float as an array element of a known field") {
        const auto r = load_alpha_text(x_with("tags = [\"a\", 0.5]\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(any_error_contains(r, "floating-point"));
    }
    SUBCASE("date under an unknown key") {
        const auto r = load_alpha_text(x_with("when = 1979-05-27\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(any_error_contains(r, "date and time values are not allowed"));
    }
    SUBCASE("float under a top-level key that is not the kind") {
        const auto r = load_alpha_text(x_with("[other]\nv = 0.5\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(any_error_contains(r, "floating-point"));
    }
    SUBCASE("control character under an unknown key: reported as well as the unknown key") {
        const auto r = load_alpha_text(x_with("note = \"a\\u0001b\"\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(r.errors.size() >= 2);
    }
}

// --- Test 2: strings (section 4, amendment 3) --------------------------------

TEST_CASE("loader strings: a multi-line string reads the same from CRLF and LF files") {
    const std::string lf = "[alpha.x]\ncount = 1\npartner = \"b1\"\nlabel = \"\"\"\nfirst\nsecond\"\"\"\n"
                           "tags = ['''\nab\ncd''']\n";
    const std::string crlf = to_crlf(lf);
    REQUIRE(crlf != lf);
    const auto rl = load_alpha_text(lf);
    const auto rc = load_alpha_text(crlf);
    check_content_result(rl);
    check_content_result(rc);
    INFO(error_dump(rl) << error_dump(rc));
    REQUIRE(rl.ok());
    REQUIRE(rc.ok());
    // The newline right after the opening delimiter is trimmed by TOML.
    CHECK(alpha_def(rl, "x").record.get_string("label") == "first\nsecond");
    CHECK(alpha_def(rc, "x").record.get_string("label") == "first\nsecond");
    REQUIRE(alpha_def(rc, "x").record.get_array("tags").size() == 1);
    CHECK(alpha_def(rc, "x").record.get_array("tags")[0].as_string() == "ab\ncd");
    CHECK(*rl.value == *rc.value);
    CHECK(wd::content_hash(*rl.value) == wd::content_hash(*rc.value));
}

TEST_CASE("loader strings: control characters and CR LF escapes") {
    // Forbidden: U+0000..U+001F except TAB and LF, and U+007F; a lone CR
    // however written (amendment 3).
    for (const char* lit : {"\"a\\u0001b\"", "\"\\u0000\"", "\"a\\u001Fb\"", "\"a\\u007Fb\"", "\"a\\rb\"",
                            "\"a\\r\"", "\"\\u000D\"", "\"a\\bb\"", "\"a\\fb\"", "\"\\u001B\""}) {
        INFO(std::string(lit));
        const auto e = only_error(x_with(std::string("label = ") + lit + "\n"));
        check_at(e, 4, 9);
        CHECK(contains(e.message, "alpha.x.label"));
    }
    // Allowed: TAB, LF, and CR LF written with escapes becomes LF.
    CHECK(loaded_x(x_with("label = \"a\\tb\"\n")).record.get_string("label") == "a\tb");
    CHECK(loaded_x(x_with("label = \"a\\nb\"\n")).record.get_string("label") == "a\nb");
    CHECK(loaded_x(x_with("label = \"a\\r\\nb\"\n")).record.get_string("label") == "a\nb");
    CHECK(loaded_x(x_with("label = \"a\\u000D\\u000Ab\"\n")).record.get_string("label") == "a\nb");
    CHECK(loaded_x(x_with("label = \"\\r\\n\\r\\n\"\n")).record.get_string("label") == "\n\n");
    // U+0080 and above are not control characters for this rule.
    CHECK(loaded_x(x_with("label = \"\\u00A0\"\n")).record.get_string("label") == "\xC2\xA0");

    SUBCASE("control characters in array elements") {
        const auto r = load_alpha_text(x_with("tags = [\"ok\", \"a\\u0001\"]\n"));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.errors.size() == 1);
        // Array elements are named key[i], 0-based (amendment 10).
        CHECK(contains(r.errors[0].message, "alpha.x.tags[1]"));
    }
}

TEST_CASE("loader strings: byte limits count bytes after the newline rule") {
    const std::string s32(32, 'a');
    const std::string s33(33, 'a');
    CHECK(loaded_x(x_with("label = \"" + s32 + "\"\n")).record.get_string("label") == s32);
    const auto e = only_error(x_with("label = \"" + s33 + "\"\n"));
    check_at(e, 4, 9);
    CHECK(contains(e.message, "alpha.x.label"));

    // Sixteen two-byte characters: 32 bytes, accepted; one more byte fails.
    std::string e16;
    for (int i = 0; i < 16; ++i) {
        e16 += "\xC3\xA9";
    }
    CHECK(loaded_x(x_with("label = \"" + e16 + "\"\n")).record.get_string("label") == e16);
    CHECK_FALSE(load_alpha_text(x_with("label = \"" + e16 + "a\"\n")).ok());

    // 16 CR LF pairs written with escapes are 16 bytes after the rule.
    std::string crlf16;
    for (int i = 0; i < 16; ++i) {
        crlf16 += "\\r\\n";
    }
    CHECK(loaded_x(x_with("label = \"" + crlf16 + "\"\n")).record.get_string("label") == std::string(16, '\n'));

    // Array elements: string(16).
    CHECK(loaded_x(x_with("tags = [\"" + std::string(16, 'b') + "\"]\n")).record.get_array("tags").size() == 1);
    const auto r = load_alpha_text(x_with("tags = [\"a\", \"" + std::string(17, 'b') + "\"]\n"));
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(contains(r.errors[0].message, "alpha.x.tags[1]"));
}

TEST_CASE("loader parsing: byte-order mark, CRLF and invalid UTF-8 (section 5)") {
    const std::string text = "[alpha.x]\ncount = 4\npartner = \"b1\"\nlabel = \"hi\"\n";
    const auto plain = load_alpha_text(text);
    REQUIRE(plain.ok());

    SUBCASE("a UTF-8 byte-order mark is ignored") {
        const auto r = load_alpha_text("\xEF\xBB\xBF" + text);
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(*r.value == *plain.value);
    }
    SUBCASE("CRLF line endings are accepted and change nothing") {
        const auto r = load_alpha_text(to_crlf(text));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(*r.value == *plain.value);
        const auto rb = load_alpha_text("\xEF\xBB\xBF" + to_crlf(text));
        REQUIRE(rb.ok());
        CHECK(*rb.value == *plain.value);
    }
    SUBCASE("the byte-order mark is not counted in columns (amendment 10)") {
        const auto e = only_error("\xEF\xBB\xBF[alpha.x]\npartner = \"b1\"\n");
        check_at(e, 1, 1);
        CHECK(contains(e.message, "alpha.x.count"));
        const auto e2 = only_error("\xEF\xBB\xBF" "alpha.x.count = 0.5\nalpha.x.partner = \"b1\"\n");
        check_at(e2, 1, 17);
        CHECK(contains(e2.message, "floating-point"));
    }
    SUBCASE("invalid UTF-8 is a parse error of that file") {
        for (const char* bad : {"\xC3\x28", "\xFF", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82"}) {
            const auto e = only_error("[alpha.x]\ncount = 1\nlabel = \"" + std::string(bad) +
                                      "\"\npartner = \"b1\"\n");
            CHECK(e.path == "alphas/a.toml");
            CHECK(e.line == 3);
            CHECK(e.column >= 1);
        }
        // In a comment, too: the whole file must be valid UTF-8.
        const auto e = only_error("[alpha.x]\ncount = 1\npartner = \"b1\"\n# \xFF\n");
        CHECK(e.path == "alphas/a.toml");
        CHECK(e.line == 4);
        CHECK(e.column >= 1);
    }
}

TEST_CASE("loader parsing: syntax errors carry path, line and column, never a partial set (amendment 11)") {
    struct Case {
        const char* text;
        std::uint32_t line;
        std::uint32_t max_column;  // the line's length + 1
    };
    for (const Case c : {Case{"[alpha.x]\ncount = = 3\npartner = \"b1\"\n", 2, 12},
                         Case{"[alpha.x]\ncount = 3\ncount = 4\npartner = \"b1\"\n", 3, 10},
                         Case{"[alpha.x]\ncount = 3\npartner = \"b1\"\n[alpha.x]\n", 4, 10},
                         Case{"[alpha.x\ncount = 3\n", 1, 9}}) {
        INFO(std::string(c.text));
        const auto e = only_error(c.text);
        CHECK(e.path == "alphas/a.toml");
        CHECK(e.line == c.line);
        CHECK(e.column >= 1);
        CHECK(e.column <= c.max_column);
        CHECK(contains(e.to_string(), ": error: "));
    }
}

TEST_CASE("loader positions: columns count code points (amendment 10)") {
    // "é" is two bytes but one column: the value 5 is at column 7.
    const auto e = only_error(x_with("\"\xC3\xA9\" = 5\n"));
    check_at(e, 4, 7);
}

// --- Test 5: duplicates, unknowns, missing fields, top-level keys ----------

TEST_CASE("loader validation: unknown key is reported at its value") {
    const auto e = only_error(x_with("colour = 5\n"));
    check_at(e, 4, 10);
    CHECK(contains(e.message, "alpha.x.colour"));

    const auto e2 = only_error(x_with("  Count   = 5\n"));
    check_at(e2, 4, 13);
}

TEST_CASE("loader validation: missing required field is reported at the definition header") {
    const auto e = only_error("# comment\n\n[alpha.x]\npartner = \"b1\"\n");
    check_at(e, 3, 1);
    CHECK(contains(e.message, "alpha.x.count"));

    const auto e2 = only_error("\n   [alpha.x]\ncount = 2\n");
    check_at(e2, 2, 4);
    CHECK(contains(e2.message, "alpha.x.partner"));

    // Both missing: two errors at the header.
    const auto r = load_alpha_text("[alpha.x]\nlabel = \"l\"\n");
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 2);
    for (const auto& err : r.errors) {
        check_at(err, 1, 1);
    }

    SUBCASE("a definition written as an inline table is positioned at the {") {
        const auto ei = only_error("[alpha]\nx = { partner = \"b1\" }\n");
        check_at(ei, 2, 5);
        CHECK(contains(ei.message, "alpha.x.count"));
    }
    SUBCASE("a definition written with dotted keys is positioned at the key") {
        const auto ed = only_error("[alpha]\nx.partner = \"b1\"\n");
        check_at(ed, 2, 1);
        CHECK(contains(ed.message, "alpha.x.count"));
    }
}

TEST_CASE("loader validation: definitions in every TOML table form load the same") {
    const auto r1 = load_alpha_text("[alpha.x]\ncount = 3\npartner = \"b1\"\n");
    const auto r2 = load_alpha_text("[alpha]\nx = { count = 3, partner = \"b1\" }\n");
    const auto r3 = load_alpha_text("alpha.x.count = 3\nalpha.x.partner = \"b1\"\n");
    const auto r4 = load_alpha_text("alpha = { x = { count = 3, partner = \"b1\" } }\n");
    for (const auto* r : {&r1, &r2, &r3, &r4}) {
        check_content_result(*r);
        INFO(error_dump(*r));
        REQUIRE(r->ok());
        CHECK(*r->value == *r1.value);
    }
}

TEST_CASE("loader validation: wrong types are errors at the value") {
    struct Case {
        const char* line;
        const char* key;
    };
    for (const Case c : {Case{"enabled = \"yes\"", "alpha.x.enabled"}, Case{"enabled = 1", "alpha.x.enabled"},
                         Case{"mode = \"medium\"", "alpha.x.mode"}, Case{"mode = \"Fast\"", "alpha.x.mode"},
                         Case{"mode = 1", "alpha.x.mode"}, Case{"label = 5", "alpha.x.label"},
                         Case{"label = [\"a\"]", "alpha.x.label"}, Case{"tags = \"a\"", "alpha.x.tags"},
                         Case{"tags = [\"a\", \"b\", \"c\", \"d\", \"e\"]", "alpha.x.tags"},
                         Case{"tags = { a = \"b\" }", "alpha.x.tags"}}) {
        INFO(std::string(c.line));
        const auto e = only_error(x_with(std::string(c.line) + "\n"));
        CHECK(e.path == "alphas/a.toml");
        CHECK(e.line == 4);
        CHECK(contains(e.message, c.key));
    }
    // An integer element of a string array: named key[i].
    const auto e = only_error(x_with("tags = [\"a\", 1]\n"));
    CHECK(contains(e.message, "alpha.x.tags[1]"));
    // Arrays of 0 and 4 elements are fine, and keep their order.
    CHECK(loaded_x(x_with("tags = []\n")).record.get_array("tags").empty());
    const auto d = loaded_x(x_with("tags = [\"d\", \"c\", \"b\", \"a\"]\n"));
    const auto& tags = d.record.get_array("tags");
    REQUIRE(tags.size() == 4);
    CHECK(tags[0].as_string() == "d");
    CHECK(tags[3].as_string() == "a");
    // Every value given explicitly.
    const auto full = loaded_x(x_with("ratio = \"0.25\"\nenabled = false\nlabel = \"L\"\nmode = \"slow\"\n"));
    CHECK(full.record.get_fx("ratio").raw() == 1073741824);
    CHECK_FALSE(full.record.get_bool("enabled"));
    CHECK(full.record.get_string("label") == "L");
    CHECK(full.record.get_string("mode") == "slow");
}

TEST_CASE("loader validation: defaults and declaration order of the record") {
    const auto d = loaded_x("[alpha.x]\npartner = \"b1\"\ncount = 3\n");
    const auto& rec = d.record;
    REQUIRE(rec.size() == 7);
    const char* names[] = {"count", "ratio", "enabled", "label", "mode", "tags", "partner"};
    for (std::size_t i = 0; i < 7; ++i) {
        CHECK(rec.name_at(i) == names[i]);
    }
    CHECK(rec.get_int("count") == 3);
    CHECK(rec.get_fx("ratio") == Fx::HALF);
    CHECK(rec.get_bool("enabled"));
    CHECK(rec.get_string("label").empty());
    CHECK(rec.get_string("mode") == "fast");
    CHECK(rec.get_array("tags").empty());
    CHECK(rec.get_string("partner") == "b1");
}

TEST_CASE("loader validation: top-level keys other than the kind name are errors") {
    for (const char* text : {"[beta.b2]\nweight = 3\n", "[gamma.x]\nv = 1\n", "count = 3\n", "alphas = {}\n",
                             "[Alpha.x]\ncount = 1\npartner = \"b1\"\n"}) {
        INFO(std::string(text));
        const auto r = load_alpha_text(text);
        check_content_result(r);
        INFO(error_dump(r));
        CHECK_FALSE(r.ok());
        for (const auto& e : r.errors) {
            CHECK(e.path == "alphas/a.toml");
        }
    }
    // Together with a valid alpha table: still an error.
    const auto r = load_alpha_text("[alpha.x]\ncount = 1\npartner = \"b1\"\n[beta.b2]\nweight = 3\n");
    CHECK_FALSE(r.ok());
}

TEST_CASE("loader validation: ids and definition shapes") {
    const std::string id64(64, 'a');
    const std::string id65(65, 'a');
    CHECK(load_alpha_text("[alpha." + id64 + "]\ncount = 1\npartner = \"b1\"\n").ok());
    CHECK(load_alpha_text("[alpha.a_1]\ncount = 1\npartner = \"b1\"\n").ok());
    for (const std::string& text :
         {"[alpha." + id65 + "]\ncount = 1\npartner = \"b1\"\n", std::string("[alpha.Bad]\ncount = 1\npartner = \"b1\"\n"),
          std::string("[alpha.1x]\ncount = 1\npartner = \"b1\"\n"),
          std::string("[alpha._x]\ncount = 1\npartner = \"b1\"\n"),
          std::string("[alpha.\"has space\"]\ncount = 1\npartner = \"b1\"\n"),
          std::string("[alpha.\"\"]\ncount = 1\npartner = \"b1\"\n"),
          std::string("[alpha.a-b]\ncount = 1\npartner = \"b1\"\n"), std::string("alpha.x = 3\n"),
          std::string("alpha = 3\n"), std::string("alpha = [1]\n"), std::string("[[alpha.x]]\ncount = 1\n")}) {
        INFO(std::string(text));
        const auto r = load_alpha_text(text);
        check_content_result(r);
        CHECK_FALSE(r.ok());
    }
}

TEST_CASE("loader validation: an empty file and an empty kind table hold zero definitions") {
    for (const char* text : {"", "\n", "# only a comment\n", "[alpha]\n"}) {
        INFO(std::string(text));
        const auto r = load_alpha_text(text);
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        REQUIRE(r.value->find_kind("alpha") != nullptr);
        CHECK(r.value->find_kind("alpha")->definitions.empty());
    }
}

TEST_CASE("loader validation: the same id in two files is one error at the second, naming both") {
    // Listed b before a: the file system order must not matter.
    auto root = alphas_root({file("b.toml", "# padding\n[alpha.x]\ncount = 2\npartner = \"b1\"\n"),
                             file("a.toml", "[alpha.x]\ncount = 1\npartner = \"b1\"\n")});
    const auto r = wd::load_content_from_memory(alpha_beta_registry(), root);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    const auto& e = r.errors[0];
    CHECK(e.path == "alphas/b.toml");
    CHECK(e.line == 2);
    CHECK(e.column == 1);
    CHECK(contains(e.message, "alphas/a.toml"));
    CHECK(contains(e.message, "x"));

    SUBCASE("three files: an error at each later occurrence") {
        auto root3 = alphas_root({file("c.toml", "[alpha.x]\ncount = 3\npartner = \"b1\"\n"),
                                  file("a.toml", "[alpha.x]\ncount = 1\npartner = \"b1\"\n"),
                                  file("b.toml", "[alpha.x]\ncount = 2\npartner = \"b1\"\n")});
        const auto r3 = wd::load_content_from_memory(alpha_beta_registry(), root3);
        check_content_result(r3);
        INFO(error_dump(r3));
        REQUIRE_FALSE(r3.ok());
        bool a_has_error = false;
        for (const auto& err : r3.errors) {
            a_has_error = a_has_error || err.path == "alphas/a.toml";
        }
        CHECK_FALSE(a_has_error);
    }
    SUBCASE("the same id in two kinds is not a duplicate") {
        auto rootk = alphas_root({file("a.toml", "[alpha.b1]\ncount = 1\npartner = \"b1\"\n")});
        CHECK(wd::load_content_from_memory(alpha_beta_registry(), rootk).ok());
    }
}

// --- Test 6: references (section 7.5, amendment 8) --------------------------

TEST_CASE("loader references: unknown id is an error, a valid one resolves") {
    const auto e = only_error("[alpha.x]\ncount = 1\npartner = \"nope\"\n");
    check_at(e, 3, 11);
    CHECK(contains(e.message, "alpha.x.partner"));
    CHECK(contains(e.message, "nope"));

    CHECK(loaded_x("[alpha.x]\ncount = 1\npartner = \"b1\"\n").record.get_string("partner") == "b1");

    // A ref value must also match the id grammar.
    for (const char* bad : {"\"B1\"", "\"\"", "\"b 1\"", "1"}) {
        INFO(std::string(bad));
        const auto eb = only_error(std::string("[alpha.x]\ncount = 1\npartner = ") + bad + "\n");
        check_at(eb, 3, 11);
    }
    // A ref names an id of the target kind, not of its own kind.
    const auto es = only_error("[alpha.x]\ncount = 1\npartner = \"x\"\n");
    check_at(es, 3, 11);
}

TEST_CASE("loader references: a target with its own errors still resolves (amendment 8)") {
    wd::MemoryDataRoot root;
    root.directories.push_back(dir("alphas", {file("a.toml", "[alpha.x]\ncount = 1\npartner = \"b1\"\n")}));
    root.directories.push_back(dir("betas", {file("b.toml", "[beta.b1]\nweight = 0\n")}));
    const auto r = wd::load_content_from_memory(alpha_beta_registry(), root);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].path == "betas/b.toml");
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column == 10);
    CHECK(contains(r.errors[0].message, "beta.b1.weight"));
}

TEST_CASE("loader references: defaults are checked, at the header (amendment 8)") {
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("n", wd::Type::integer(0, 9)));
    fields.push_back(wd::Field::optional("friend", wd::Type::reference("beta"), wd::Value::string("missing")));
    wd::KindRegistry reg = alpha_beta_registry();
    reg.add(wd::Kind{"gamma", "gammas", false, wd::Schema(std::move(fields))});

    wd::MemoryDataRoot root;
    root.directories.push_back(dir("gammas", {file("g.toml", "\n[gamma.g]\nn = 1\n")}));
    root.directories.push_back(dir("betas", {file("b.toml", std::string(BETA_B1))}));
    const auto r = wd::load_content_from_memory(reg, root);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].path == "gammas/g.toml");
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column == 1);

    // Given explicitly with a valid id, it resolves.
    root.directories[0].entries[0].bytes = "[gamma.g]\nn = 1\nfriend = \"b1\"\n";
    const auto ok = wd::load_content_from_memory(reg, root);
    INFO(error_dump(ok));
    CHECK(ok.ok());
}

TEST_CASE("loader references: a ref to an unregistered kind is fatal at load (amendment 12)") {
    wd::KindRegistry only_alpha;
    only_alpha.add(alpha_kind());
    CHECK_THROWS_AS((void)wd::load_content_from_memory(only_alpha, alphas_root({})), FatalError);
    CHECK_THROWS_AS((void)wd::load_content_without_data(only_alpha), FatalError);
}

// --- Named checks (section 7.5, amendment 8) ---------------------------------

TEST_CASE("loader checks: named checks run after type validation, reported at the header") {
    int calls = 0;
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("lo", wd::Type::integer(0, 100)));
    fields.push_back(wd::Field::required("hi", wd::Type::integer(0, 100)));
    std::vector<wd::Check> checks;
    checks.push_back(wd::Check{"lo_not_above_hi", [&calls](const wd::Record& rec) -> std::optional<std::string> {
                                   ++calls;
                                   if (rec.get_int("lo") > rec.get_int("hi")) {
                                       return std::string("lo must not exceed hi");
                                   }
                                   return std::nullopt;
                               }});
    wd::KindRegistry reg;
    reg.add(wd::Kind{"delta", "deltas", false, wd::Schema(std::move(fields), std::move(checks))});

    auto load = [&reg](std::string text) {
        wd::MemoryDataRoot root;
        root.directories.push_back(dir("deltas", {file("d.toml", std::move(text))}));
        return wd::load_content_from_memory(reg, root);
    };

    CHECK(load("[delta.d]\nlo = 1\nhi = 2\n").ok());
    const auto r = load("\n[delta.d]\nlo = 3\nhi = 2\n");
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].path == "deltas/d.toml");
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column == 1);

    // A definition that failed type validation is not checked: one error only.
    calls = 0;
    const auto rt = load("[delta.d]\nlo = 300\nhi = 2\n");
    INFO(error_dump(rt));
    REQUIRE(rt.errors.size() == 1);
    CHECK(rt.errors[0].line == 2);
    CHECK(calls == 0);
}

// --- Test 10: limits (section 5, amendment 5) -------------------------------

TEST_CASE("loader limits: files above 1 MiB are rejected") {
    const std::string head = "[alpha.x]\ncount = 1\npartner = \"b1\"\n";
    const std::size_t mib = std::size_t{1} << 20;
    // head + "#" + padding + "\n" = exactly 1 MiB.
    std::string exact = head + "#" + std::string(mib - head.size() - 2, 'x') + "\n";
    REQUIRE(exact.size() == mib);
    const auto r_ok = load_alpha_text(exact);
    INFO(error_dump(r_ok));
    CHECK(r_ok.ok());

    std::string over = head + "#" + std::string(mib - head.size() - 1, 'x') + "\n";
    REQUIRE(over.size() == mib + 1);
    const auto r = load_alpha_text(over);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].path == "alphas/a.toml");
}

namespace {

// array^levels(int(0, 9)), lengths 0..1.
wd::Type nested_int_array(int levels) {
    wd::Type t = wd::Type::integer(0, 9);
    for (int i = 0; i < levels; ++i) {
        t = wd::Type::array(t, 0, 1);
    }
    return t;
}

std::string nested_literal(int levels) {
    return std::string(static_cast<std::size_t>(levels), '[') + "1" +
           std::string(static_cast<std::size_t>(levels), ']');
}

}  // namespace

TEST_CASE("loader limits: nesting depth 9 is an error and stops that file only (amendment 5)") {
    // The kind table is depth 1, a definition depth 2, a field's array depth 3.
    // Six nested arrays reach depth 8 (allowed); seven reach depth 9.
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::optional("d8", nested_int_array(6), wd::Value::array({})));
    fields.push_back(wd::Field::optional("d9", nested_int_array(7), wd::Value::array({})));
    fields.push_back(wd::Field::optional("t", wd::Type::integer(0, 9), wd::Value::integer(0)));
    wd::KindRegistry reg;
    reg.add(wd::Kind{"gamma", "gammas", false, wd::Schema(std::move(fields))});

    auto load = [&reg](std::vector<wd::MemoryEntry> files) {
        wd::MemoryDataRoot root;
        root.directories.push_back(dir("gammas", std::move(files)));
        return wd::load_content_from_memory(reg, root);
    };

    const auto r8 = load({file("a.toml", "[gamma.g]\nd8 = " + nested_literal(6) + "\n")});
    INFO(error_dump(r8));
    REQUIRE(r8.ok());
    const wd::Value* v = &r8.value->find("gamma", "g")->record.get("d8");
    for (int i = 0; i < 6; ++i) {
        REQUIRE(v->kind() == wd::ValueKind::array);
        REQUIRE(v->as_array().size() == 1);
        v = &v->as_array()[0];
    }
    CHECK(v->as_int() == 1);

    // Depth 9: one error for a.toml (its other errors are not reported, as
    // for a syntax error), and b.toml is still validated.
    const auto r9 = load({file("a.toml", "[gamma.g]\nt = 99\nd9 = " + nested_literal(7) + "\n"),
                          file("b.toml", "[gamma.h]\nt = 99\n")});
    check_content_result(r9);
    INFO(error_dump(r9));
    REQUIRE(r9.errors.size() == 2);
    CHECK(r9.errors[0].path == "gammas/a.toml");
    CHECK(r9.errors[1].path == "gammas/b.toml");
    CHECK(r9.errors[1].line == 2);

    // Depth from tables counts too: seven nested inline tables under an
    // unknown key reach depth 3 + 6 = 9.
    const auto rt = load({file("a.toml", "[gamma.g]\nx = { a = { b = { c = { d = { e = { f = { g = 1 } } } } } } }\n")});
    INFO(error_dump(rt));
    CHECK(rt.errors.size() == 1);
    // Six nested inline tables (depth 8) under an unknown key: only the
    // unknown key error, at its value.
    const auto rt8 = load({file("a.toml", "[gamma.g]\nx = { a = { b = { c = { d = { e = { f = 1 } } } } } }\n")});
    INFO(error_dump(rt8));
    REQUIRE(rt8.errors.size() == 1);
    CHECK(rt8.errors[0].line == 2);
    CHECK(rt8.errors[0].column == 5);
}

// --- Registration errors (amendment 12) and the registry (7.1) --------------

TEST_CASE("loader registration: mistakes are fatal") {
    CHECK_THROWS_AS((void)wd::Type::integer(5, 4), FatalError);
    CHECK_THROWS_AS((void)wd::Type::fixed("1", "0"), FatalError);
    CHECK_THROWS_AS((void)wd::Type::fixed("0", "x"), FatalError);
    CHECK_THROWS_AS((void)wd::Type::string(0), FatalError);
    CHECK_THROWS_AS((void)wd::Type::enumeration({}), FatalError);
    CHECK_THROWS_AS((void)wd::Type::enumeration({"a", "a"}), FatalError);
    CHECK_THROWS_AS((void)wd::Type::reference("Beta"), FatalError);
    CHECK_THROWS_AS((void)wd::Type::array(wd::Type::boolean(), 3, 2), FatalError);
    CHECK_THROWS_AS((void)wd::decimal_literal("0.5.5"), FatalError);

    CHECK_THROWS_AS((void)wd::Field::required("Count", wd::Type::boolean()), FatalError);
    CHECK_THROWS_AS((void)wd::Field::required("", wd::Type::boolean()), FatalError);
    // Defaults that fail their own type.
    CHECK_THROWS_AS((void)wd::Field::optional("n", wd::Type::integer(0, 1), wd::Value::integer(2)), FatalError);
    CHECK_THROWS_AS((void)wd::Field::optional("n", wd::Type::integer(0, 1), wd::Value::boolean(true)), FatalError);
    CHECK_THROWS_AS((void)wd::Field::optional("s", wd::Type::string(2), wd::Value::string("abc")), FatalError);
    CHECK_THROWS_AS((void)wd::Field::optional("s", wd::Type::string(8), wd::Value::string("a\x01")), FatalError);
    CHECK_THROWS_AS((void)wd::Field::optional("m", wd::Type::enumeration({"a"}), wd::Value::string("b")),
                    FatalError);
    CHECK_THROWS_AS((void)wd::Field::optional("r", wd::Type::reference("beta"), wd::Value::string("Bad")),
                    FatalError);
    CHECK_THROWS_AS((void)wd::Field::optional("t", wd::Type::array(wd::Type::boolean(), 1, 2), wd::Value::array({})),
                    FatalError);

    {
        std::vector<wd::Field> dup;
        dup.push_back(wd::Field::required("a", wd::Type::boolean()));
        dup.push_back(wd::Field::required("a", wd::Type::boolean()));
        CHECK_THROWS_AS((void)wd::Schema(std::move(dup)), FatalError);
    }

    wd::KindRegistry reg;
    reg.add(alpha_kind());
    CHECK_THROWS_AS(reg.add(alpha_kind()), FatalError);  // same name
    wd::Kind same_dir = beta_kind();
    same_dir.directory = "alphas";
    CHECK_THROWS_AS(reg.add(same_dir), FatalError);  // same directory
    wd::Kind bad_name = beta_kind();
    bad_name.name = "Beta";
    CHECK_THROWS_AS(reg.add(bad_name), FatalError);
    wd::Kind bad_dir = beta_kind();
    bad_dir.directory = "be/tas";
    CHECK_THROWS_AS(reg.add(bad_dir), FatalError);
    wd::Kind empty_dir = beta_kind();
    empty_dir.directory = "";
    CHECK_THROWS_AS(reg.add(empty_dir), FatalError);
}

TEST_CASE("loader registry: ascending name order; production registry has unique names and directories") {
    const auto reg = alpha_beta_registry();
    REQUIRE(reg.kinds().size() == 2);
    CHECK(reg.kinds()[0].name == "alpha");
    CHECK(reg.kinds()[1].name == "beta");
    CHECK(reg.find("beta") != nullptr);
    CHECK(reg.find("gamma") == nullptr);

    // Section 7.1: a test checks the registry's names and directories are
    // unique and in the name grammar. P0 registers no production kinds.
    const auto builtin = wd::builtin_kinds();
    CHECK(builtin.kinds().empty());
    const auto& kinds = builtin.kinds();
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        CHECK(wd::is_valid_name(kinds[i].name));
        CHECK(wd::is_valid_name(kinds[i].directory));
        for (std::size_t j = i + 1; j < kinds.size(); ++j) {
            CHECK(kinds[i].name != kinds[j].name);
            CHECK(kinds[i].directory != kinds[j].directory);
        }
    }
}

TEST_CASE("loader grammars: names and ids") {
    CHECK(wd::is_valid_name("a"));
    CHECK(wd::is_valid_name("a_9"));
    CHECK_FALSE(wd::is_valid_name(""));
    CHECK_FALSE(wd::is_valid_name("9a"));
    CHECK_FALSE(wd::is_valid_name("_a"));
    CHECK_FALSE(wd::is_valid_name("aB"));
    CHECK_FALSE(wd::is_valid_name("a-b"));
    CHECK(wd::is_valid_id(std::string(64, 'z')));
    CHECK_FALSE(wd::is_valid_id(std::string(65, 'z')));
    CHECK_FALSE(wd::is_valid_id(""));
    CHECK_FALSE(wd::is_valid_id("a.b"));
}
