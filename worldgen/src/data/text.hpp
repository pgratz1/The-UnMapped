#pragma once
// Internal text helpers of the loader (P0-03). Byte-wise, ASCII-only
// classification; no locale, no <cctype>.
#include <cstdint>
#include <string>
#include <string_view>

#include "worldgen/core/fx.hpp"

namespace worldgen::data::detail {

// a < b comparing bytes as unsigned values, shorter prefix first (P0-03 5,
// 7.2, 9: "sorted by bytes").
[[nodiscard]] bool bytes_less(std::string_view a, std::string_view b) noexcept;

// Strict weak order object for std::map / std::sort.
struct BytesLess {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept { return bytes_less(a, b); }
};

[[nodiscard]] constexpr bool is_lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
[[nodiscard]] constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// [a-z0-9][a-z0-9_-]*\.toml (P0-03 7.2).
[[nodiscard]] bool is_valid_data_file_name(std::string_view name) noexcept;

[[nodiscard]] bool ends_with(std::string_view text, std::string_view suffix) noexcept;

// Decimal text of integers (std::to_chars, locale-independent).
[[nodiscard]] std::string int_text(std::int64_t v);
[[nodiscard]] std::string uint_text(std::uint64_t v);

// Shortest decimal text (0 to 18 fractional digits) that parse_decimal reads
// back as exactly x, for messages. Integer arithmetic only.
[[nodiscard]] std::string fx_text(Fx x);

// The first forbidden control byte in s (U+0000..U+001F except TAB and LF,
// and U+007F; P0-03 section 4), or -1 if there is none. These code points
// are single bytes in UTF-8 and never occur inside a multi-byte sequence.
[[nodiscard]] std::int32_t first_forbidden_control(std::string_view s) noexcept;

// "U+0001" style text for a code point below 0x10000.
[[nodiscard]] std::string code_point_text(std::uint32_t cp);

// 'text' with single quotes, for messages.
[[nodiscard]] std::string in_quotes(std::string_view text);

}  // namespace worldgen::data::detail
