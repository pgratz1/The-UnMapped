// Spec P0-01 section 13.2: decimal parsing (5.6), canonical text (5.6) and
// formatting (5.7).

#include <string>
#include <string_view>

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

namespace {

void check_parses(std::string_view text, std::int64_t expected_raw) {
    const DecimalParse p = parse_decimal(text);
    INFO("parse_decimal(\"" << std::string(text) << "\")");
    CHECK(p.ok());
    CHECK(p.error == DecimalError::none);
    CHECK(p.value.raw() == expected_raw);
}

void check_rejects(std::string_view text) {
    const DecimalParse p = parse_decimal(text);
    INFO("parse_decimal(\"" << std::string(text) << "\")");
    CHECK_FALSE(p.ok());
    CHECK(p.error != DecimalError::none);
    CHECK(p.value.raw() == 0);  // header: value is raw 0 on error
    CHECK_FALSE(canonical_decimal(text).has_value());
}

void check_canonical(std::string_view text, std::string_view expected) {
    INFO("canonical_decimal(\"" << std::string(text) << "\")");
    const std::optional<std::string> c = canonical_decimal(text);
    REQUIRE(c.has_value());
    CHECK(*c == expected);
    // Canonical text parses to the same raw value as the original.
    CHECK(parse_decimal(*c).value == parse_decimal(text).value);
}

}  // namespace

TEST_CASE("P0-01 13.2: parse_decimal values") {
    check_parses("0", 0);
    check_parses("-0", 0);
    check_parses("+0.000", 0);
    check_parses("1", 4294967296);
    check_parses("0.5", 2147483648);
    check_parses("-1.25", -5368709120);
    check_parses("0.1", 429496730);
    check_parses("-0.1", -429496730);
    check_parses("0.35", 1503238554);
    check_parses("2147483647.9999999", 9223372036854775379);
    check_parses("0.000000000000000001", 0);
    check_parses("2147483647", std::int64_t{2147483647} * ONE_RAW);
    check_parses("-2147483647", std::int64_t{-2147483647} * ONE_RAW);
    check_parses("+3", 3 * ONE_RAW);
}

TEST_CASE("P0-01 13.2: parse_decimal errors") {
    // Rounds to 2^63.
    check_rejects("2147483647.99999999999999999");
    CHECK(parse_decimal("2147483647.99999999999999999").error == DecimalError::out_of_range);
    check_rejects("2147483648");
    CHECK(parse_decimal("-2147483648").error == DecimalError::out_of_range);
    // 19 fractional digits.
    check_rejects("0.0000000000000000001");
    CHECK(parse_decimal("0.0000000000000000001").error == DecimalError::frac_digits);
    // 11 integer digits.
    check_rejects("12345678901");
    CHECK(parse_decimal("12345678901").error == DecimalError::int_digits);
    for (const std::string_view bad : {".5", "5.", "05", "1e3", "1_000", " 1", "1 ", "", "-", "0x10", "1.2.3"}) {
        check_rejects(bad);
        INFO("input \"" << std::string(bad) << "\"");
        CHECK(parse_decimal(bad).error == DecimalError::syntax);
    }
    for (const std::string_view bad : {"+", "+-1", "--1", "-.5", "00", "-00.5", "1.", "1..2", "1,5", "\t1"}) {
        check_rejects(bad);
    }
}

TEST_CASE("P0-01 13.2: canonical text") {
    check_canonical("+0.500", "0.5");
    check_canonical("-0", "0");
    check_canonical("3.000", "3");
    check_canonical("10", "10");
    check_canonical("-2.50", "-2.5");
    // 5.6 prose.
    check_canonical("-0.000", "0");
    check_canonical("+0", "0");
    check_canonical("0.000000000000000001", "0.000000000000000001");
    check_canonical("-0.000000000000000001", "-0.000000000000000001");
    check_canonical("0", "0");
}

TEST_CASE("P0-01 13.2: to_decimal formatting") {
    CHECK(to_decimal(Fx::HALF, 3) == "0.500");
    CHECK(Fx::HALF.to_decimal(3) == "0.500");
    CHECK(to_decimal(raw(-5368709120), 1) == "-1.3");
    CHECK(to_decimal(raw(1), 9) == "0.000000000");
    CHECK(to_decimal(raw(-1), 2) == "0.00");
    CHECK(to_decimal(fxi(-7), 0) == "-7");
    CHECK(fxi(-7).to_decimal(0) == "-7");
    CHECK(to_decimal(Fx{}, 0) == "0");
    CHECK_FATAL(to_decimal(Fx::ONE, -1));
    CHECK_FATAL(to_decimal(Fx::ONE, 19));
    CHECK_FATAL(Fx::ONE.to_decimal(19));
}

TEST_CASE("P0-01 13.2: parse_decimal(to_decimal(x, 18)) == x") {
    auto round_trip = [](Fx x) {
        const std::string text = to_decimal(x, 18);
        const DecimalParse p = parse_decimal(text);
        INFO("raw " << x.raw() << " text " << text);
        REQUIRE(p.ok());
        REQUIRE(p.value == x);
    };
    round_trip(Fx::MIN);
    round_trip(Fx::MAX);
    round_trip(raw(1));
    round_trip(raw(-1));
    round_trip(Fx{});
    Rng rng(KEY_DECIMAL_PROPS);
    for (int i = 0; i < 100000; ++i) {
        const std::int64_t r = static_cast<std::int64_t>(rng.next());
        if (r == I64_MIN) {
            continue;  // not a valid Fx
        }
        round_trip(raw(r));
    }
}
