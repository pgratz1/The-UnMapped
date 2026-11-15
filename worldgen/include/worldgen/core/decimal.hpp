#pragma once

// Decimal parsing and canonical decimal text (spec P0-01 section 5.6).
// Formatting is Fx::to_decimal / to_decimal in fx.hpp (section 5.7).
//
// Parsing takes user data (TOML, the command line), so errors are returned
// as values, never raised as FatalError.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "worldgen/core/fx.hpp"

namespace worldgen {

// Why parse_decimal rejected its input (P0-01 5.6).
enum class DecimalError : std::uint8_t {
    none = 0,       // success
    syntax,         // does not match the grammar (empty, sign only, leading
                    // zero, leading or trailing '.', exponent, '_',
                    // whitespace, any other character, ...)
    int_digits,     // more than 10 integer digits
    frac_digits,    // more than 18 fractional digits
    out_of_range,   // the rounded magnitude exceeds 2^63 - 1
};

// Result of parse_decimal. When ok(), value holds the parsed Fx; otherwise
// value is raw 0 and error says why.
struct DecimalParse {
    Fx value;
    DecimalError error = DecimalError::none;

    [[nodiscard]] constexpr bool ok() const noexcept { return error == DecimalError::none; }
};

// Parses the decimal grammar of P0-01 5.6 (whole string, ASCII):
//   decimal   = [ "-" | "+" ] int-part [ "." frac-part ]
//   int-part  = "0" | nonzero-digit { digit }   ; at most 10 digits
//   frac-part = digit { digit }                 ; 1 to 18 digits
// raw magnitude = RHA(N * 2^32 / 10^f), sign applied after rounding.
// Never throws.
[[nodiscard]] DecimalParse parse_decimal(std::string_view text) noexcept;

// Canonical text of a decimal (P0-01 5.6): leading '+' removed, trailing
// fractional zeros removed (and the '.' if the fraction becomes empty),
// "-0" becomes "0". Returns std::nullopt exactly when parse_decimal(text)
// fails. The canonical text parses to the same raw value as text.
[[nodiscard]] std::optional<std::string> canonical_decimal(std::string_view text);

}  // namespace worldgen
