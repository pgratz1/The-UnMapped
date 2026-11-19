#pragma once

// The text report of worldgen-cli report, format v2 (spec P0-04 section 7,
// amendment 16c; P1-00 section 10.3).

#include <string>
#include <string_view>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::inspect {

// First line of format v1, without its line feed (P0-04 7). No longer
// emitted; kept for reference.
inline constexpr std::string_view REPORT_HEADER_V1 = "# worldgen report v1";

// First line of format v2, without its line feed (P1-00 10.3). This is the
// format build_report emits.
inline constexpr std::string_view REPORT_HEADER_V2 = "# worldgen report v2";

// Text escaped for a report line (P0-04 7): a backslash becomes "\\", each
// control byte (0x00-0x1F, 0x7F) becomes "\xHH" with upper-case hex, every
// other byte (UTF-8 sequences included) is copied unchanged.
[[nodiscard]] std::string escape_report_text(std::string_view text);

// The whole report of an open world, format v2: UTF-8, LF line endings, no
// trailing spaces, ending with a line feed. The header line
// REPORT_HEADER_V2, then the sections of v1 unchanged (P0-04 7): [meta]
// (every meta row in key order, "<key> = <value>", or "<key> =" for an
// empty value), [tables] (every non-sqlite_* table, ascending name,
// "<table> = <rows>") and [layers] (one line per layer in id order,
// "<name> <format> <w>x<h> codec=<c> bytes=<blob length> min=<min>
// max=<max>", or "(none)"). Keys, values, table and layer names all go
// through escape_report_text. Then, when the world has a layer named
// `elevation`, the section [geography] of P1-00 10.3 (geography_section in
// geography.hpp), reading the layers elevation, water, flow_dir,
// flow_accum, river and lake. Then, when table age has a row, the section
// [chronology] of P3-03 9.1 (chronology_section in chronology.hpp), after
// the checks of tables race and age (corrupt_layer, "corrupt table 'race':
// ..." or "corrupt table 'age': ...").
//
// Decodes every layer for min and max, so a corrupt layer is a
// corrupt_layer error; other reader errors are passed on. A geography layer
// that cannot be used (wrong element type or size, or a flow_dir that does
// not lead to the ocean; geography_layers_problem) is also reported as
// corrupt_layer, with a message naming the layer (reading chosen: an input
// error, exit 3 in the CLI).
[[nodiscard]] db::Result<std::string> build_report(const db::WorldReader& world);

}  // namespace worldgen::inspect
