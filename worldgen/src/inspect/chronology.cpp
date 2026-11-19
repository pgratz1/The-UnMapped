// The [chronology] section of the report (spec P3-03 9.1).

#include "worldgen/inspect/chronology.hpp"

#include <algorithm>
#include <cstddef>

#include "worldgen/db/schema.hpp"

namespace worldgen::inspect {

namespace {

struct RaceRecord {
    std::int64_t id = 0;
    std::int64_t present = 0;
    std::optional<std::int64_t> awakening_year;
};

struct AgeRecord {
    std::int64_t id = 0;
    std::int64_t race = 0;
    std::int64_t start_year = 0;
    std::int64_t end_year = 0;
};

std::string row_at(const std::vector<db::ColumnValue>& row) {
    const std::string id =
        (!row.empty() && row[0].type == db::ValueType::integer) ? db::format_i64(row[0].integer) : std::string("?");
    return "row " + id + ": ";
}

// The column checks of one row: `text` lists the text columns, `nullable`
// the integer column that may be NULL (or COLUMNS for none); every other
// column is an integer. Returns the reason, or std::nullopt.
std::optional<std::string> column_problem(const std::vector<db::ColumnValue>& row, std::size_t columns,
                                          std::span<const std::size_t> text, std::size_t nullable) {
    if (row.size() != columns) {
        return "has " + db::format_u64(row.size()) + " columns, expected " + db::format_u64(columns);
    }
    for (std::size_t k = 0; k < columns; ++k) {
        const bool is_text = std::find(text.begin(), text.end(), k) != text.end();
        if (is_text) {
            if (row[k].type != db::ValueType::text) {
                return "column " + db::format_u64(k + 1U) + " is not text";
            }
        } else if (row[k].type != db::ValueType::integer && !(k == nullable && row[k].type == db::ValueType::null)) {
            return "column " + db::format_u64(k + 1U) + " is not an integer";
        }
    }
    return std::nullopt;
}

std::optional<std::string> read_races(std::span<const std::vector<db::ColumnValue>> rows,
                                      std::vector<RaceRecord>& out) {
    static constexpr std::size_t TEXT[] = {1, 2, 3, 4};  // key, name, plural, adjective
    constexpr std::size_t PRESENT = 9;
    constexpr std::size_t AWAKENING = 10;
    for (const std::vector<db::ColumnValue>& row : rows) {
        const std::string at = row_at(row);
        if (std::optional<std::string> why = column_problem(row, 12, TEXT, AWAKENING)) {
            return "corrupt table 'race': " + at + *why;
        }
        RaceRecord r;
        r.id = row[0].integer;
        r.present = row[PRESENT].integer;
        if (row[AWAKENING].type == db::ValueType::integer) {
            r.awakening_year = row[AWAKENING].integer;
        }
        if (r.present != 0 && r.present != 1) {
            return "corrupt table 'race': " + at + "present is " + db::format_i64(r.present);
        }
        if (r.awakening_year.has_value() != (r.present == 1)) {
            return "corrupt table 'race': " + at + "awakening_year does not match present";
        }
        out.push_back(r);
    }
    return std::nullopt;
}

const RaceRecord* present_race(const std::vector<RaceRecord>& races, std::int64_t id) {
    for (const RaceRecord& r : races) {
        if (r.id == id && r.present == 1) {
            return &r;
        }
    }
    return nullptr;
}

std::optional<std::string> read_ages(std::span<const std::vector<db::ColumnValue>> rows,
                                     const std::vector<RaceRecord>& races, std::vector<AgeRecord>& out) {
    static constexpr std::size_t TEXT[] = {4};  // name
    constexpr std::size_t NONE = 5;
    for (const std::vector<db::ColumnValue>& row : rows) {
        const std::string at = "corrupt table 'age': " + row_at(row);
        if (std::optional<std::string> why = column_problem(row, 5, TEXT, NONE)) {
            return at + *why;
        }
        AgeRecord a{row[0].integer, row[1].integer, row[2].integer, row[3].integer};
        const std::int64_t expected_id = out.empty() ? 1 : out.back().id + 1;
        if (a.id != expected_id) {
            return at + "id not consecutive";
        }
        const RaceRecord* race = present_race(races, a.race);
        if (race == nullptr) {
            return at + "race " + db::format_i64(a.race) + " is not a present race";
        }
        // The previous end_year + 1, computed without overflow (end_year is
        // at least 0 here: it is not below a start_year that is 0 or the
        // end_year before it plus 1).
        const std::uint64_t expected_start =
            out.empty() ? 0U : static_cast<std::uint64_t>(out.back().end_year) + 1U;
        if (a.start_year < 0 || static_cast<std::uint64_t>(a.start_year) != expected_start) {
            return at + "start_year " + db::format_i64(a.start_year) + ", expected " + db::format_u64(expected_start);
        }
        if (a.end_year < a.start_year) {
            return at + "end_year below start_year";
        }
        if (*race->awakening_year != a.start_year) {
            return at + "race " + db::format_i64(a.race) + " awakens in " + db::format_i64(*race->awakening_year);
        }
        out.push_back(a);
    }
    return std::nullopt;
}

}  // namespace

ChronologySectionResult chronology_section(std::span<const std::vector<db::ColumnValue>> race_rows,
                                           std::span<const std::vector<db::ColumnValue>> age_rows,
                                           std::uint64_t god_rows, std::int64_t span) {
    ChronologySectionResult result;
    if (race_rows.empty() && age_rows.empty()) {
        return result;
    }
    std::vector<RaceRecord> races;
    if (std::optional<std::string> why = read_races(race_rows, races)) {
        result.problem = std::move(why);
        return result;
    }
    std::vector<AgeRecord> ages;
    if (std::optional<std::string> why = read_ages(age_rows, races, ages)) {
        result.problem = std::move(why);
        return result;
    }
    if (!ages.empty() && ages.back().end_year != span - 1) {
        result.problem = "corrupt table 'age': the last age ends in " + db::format_i64(ages.back().end_year) +
                         ", expected " + db::format_i64(span - 1);
        return result;
    }
    std::uint64_t present = 0;
    for (const RaceRecord& r : races) {
        present += r.present == 1 ? 1U : 0U;
    }
    if (ages.size() != present) {
        result.problem = "corrupt table 'age': " + db::format_u64(ages.size()) + " rows for " +
                         db::format_u64(present) + " present races";
        return result;
    }
    if (ages.empty()) {
        return result;
    }

    std::string& out = result.section;
    out += "[chronology]\n";
    out += "races_present = " + db::format_u64(present) + "\n";
    out += "awakening_years =";
    for (const RaceRecord& r : races) {
        if (r.present == 1) {
            out += " " + db::format_i64(*r.awakening_year);
        }
    }
    out += "\nage_years =";
    std::int64_t min_years = 0;
    for (std::size_t i = 0; i < ages.size(); ++i) {
        const std::int64_t years = ages[i].end_year - ages[i].start_year + 1;
        min_years = i == 0 ? years : std::min(min_years, years);
        out += " " + db::format_i64(years);
    }
    out += "\nage_years_min = " + db::format_i64(min_years) + "\n";
    out += "gods = " + db::format_u64(god_rows) + "\n";
    return result;
}

}  // namespace worldgen::inspect
