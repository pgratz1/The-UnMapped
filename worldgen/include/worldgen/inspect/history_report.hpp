#pragma once

// The history report of worldgen-cli report --history, format
// "# worldgen history v3" (spec P3-07 sections 4 to 6, amendments 21 to 26).
// The text is part of the cross-platform contract: ASCII and LF only,
// integers only, no locale.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::inspect {

// First line of format v1, without its line feed (P3-07 6.2); no longer
// written from generator 0.21.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V1 = "# worldgen history v1";
// First line of format v2 (P3-07 r19 25.1); no longer written from
// generator 0.22.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V2 = "# worldgen history v2";
// First line of format v3 (P3-07 r21 26.1); no longer written from
// generator 0.23.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V3 = "# worldgen history v3";
// First line of format v4 (P5-02 12.1), the one written.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V4 = "# worldgen history v4";

enum class HistoryLevel : std::uint8_t { summary = 1, full = 2 };

// P3-07 r21 26.7 (audit A1): the row of settlement `id` in a vector indexed
// by settlement id - 1 (the digest's last_held_by and refuge_hidden_end), or
// null when id is below 1 or above the span's size.
template <class T>
[[nodiscard]] const T* settlement_row(std::span<const T> rows, std::int64_t id) noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > rows.size()) {
        return nullptr;
    }
    return &rows[static_cast<std::size_t>(id - 1)];
}

// "summary" or "full"; std::nullopt for anything else.
[[nodiscard]] std::optional<HistoryLevel> parse_history_level(std::string_view text);

// Text from the world escaped for the history report (P3-07 6.1): a
// backslash becomes "\\", every byte below 0x20, 0x7F and every byte from
// 0x80 becomes "\xHH" with upper-case hex, every other byte is copied.
[[nodiscard]] std::string escape_history_text(std::string_view text);

// The whole history report of an open world (P3-07 section 6), or the first
// error of P3-07 5.2: history_missing for a world without history,
// corrupt_layer for a corrupt table, reader errors as they come,
// out_of_memory.
[[nodiscard]] db::Result<std::string> build_history_report(const db::WorldReader& world, HistoryLevel level);

}  // namespace worldgen::inspect
