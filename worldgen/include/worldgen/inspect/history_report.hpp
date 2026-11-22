#pragma once

// The history report of worldgen-cli report --history, format
// "# worldgen history v1" (spec P3-07 sections 4 to 6, amendments 21 to 23).
// The text is part of the cross-platform contract: ASCII and LF only,
// integers only, no locale.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::inspect {

// First line of format v1, without its line feed (P3-07 6.2).
inline constexpr std::string_view HISTORY_REPORT_HEADER_V1 = "# worldgen history v1";

enum class HistoryLevel : std::uint8_t { summary = 1, full = 2 };

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
