#pragma once

// The [chronology] section of the report and its corrupt-data checks (spec
// P3-03 9.1).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "worldgen/db/reader.hpp"

namespace worldgen::inspect {

// The result of chronology_section: the section text (empty when table age
// has no row), or the first problem, the whole message of P3-03 9.1
// ("corrupt table 'race': row <id>: ..." or "corrupt table 'age': ...").
struct ChronologySectionResult {
    std::string section;
    std::optional<std::string> problem;
};

// Checks the rows of tables race and age (each in primary-key order, as
// visited) and builds [chronology] (P3-03 9.1) when age has a row. span is
// param.history_span (E). No check runs when both tables are empty.
//
// Checks, first failure reported: (1) race rows in id order: the columns
// (reading chosen: "row <id>: has <n> columns, expected 12", "row <id>:
// column <k> is not an integer", "row <id>: column <k> is not text"),
// "present is <v>" (not 0 or 1), "awakening_year does not match present"
// (NULL exactly when present is 0); (2) age rows in id order: the columns
// (as for race, 5 columns), "id not consecutive", "race <r> is not a present
// race", "start_year <s>, expected <x>", "end_year below start_year", "race
// <r> awakens in <y>"; then "the last age ends in <e>, expected <E - 1>" and
// "<rows> rows for <p> present races".
//
// Section keys, in order: races_present, awakening_years, age_years,
// age_years_min, gods (god_rows).
[[nodiscard]] ChronologySectionResult chronology_section(std::span<const std::vector<db::ColumnValue>> race_rows,
                                                         std::span<const std::vector<db::ColumnValue>> age_rows,
                                                         std::uint64_t god_rows, std::int64_t span);

}  // namespace worldgen::inspect
