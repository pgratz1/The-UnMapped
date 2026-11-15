#pragma once

// The text report of worldgen-cli report, format v1 (spec P0-04 section 7).

#include <string>
#include <string_view>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::inspect {

// First line of format v1, without its line feed (P0-04 7).
inline constexpr std::string_view REPORT_HEADER_V1 = "# worldgen report v1";

// Text escaped for a report line (P0-04 7): a backslash becomes "\\", each
// control byte (0x00-0x1F, 0x7F) becomes "\xHH" with upper-case hex, every
// other byte (UTF-8 sequences included) is copied unchanged.
[[nodiscard]] std::string escape_report_text(std::string_view text);

// The whole report of an open world, format v1 (P0-04 7): UTF-8, LF line
// endings, no trailing spaces, ending with a line feed. Sections [meta]
// (every meta row in key order, "<key> = <value>", or "<key> =" for an
// empty value), [tables] (every non-sqlite_* table, ascending name,
// "<table> = <rows>") and [layers] (one line per layer in id order,
// "<name> <format> <w>x<h> codec=<c> bytes=<blob length> min=<min>
// max=<max>", or "(none)"). Keys, values, table and layer names all go
// through escape_report_text. Decodes every layer for min and max, so a
// corrupt layer is a corrupt_layer error; other reader errors are passed on.
[[nodiscard]] db::Result<std::string> build_report(const db::WorldReader& world);

}  // namespace worldgen::inspect
