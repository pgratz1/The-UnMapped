#pragma once

// Data checks across kinds for the history (spec P3-03 section 6): the
// conditions of P3-01 3 that a per-kind check cannot see, plus the checks of
// the age names. Run by worldgen-cli generate right after the data is
// loaded (an input error, exit 3) and again at the chronology stage's entry
// (a seam guard, P3-03 C1 item 4).

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"

namespace worldgen::history {

// The longest age name before any numeral (P3-03 D4), in bytes.
inline constexpr std::size_t AGE_NAME_MAX_BYTES = 48;

// C4 step 1 of P3-03: form with every "{plural}" replaced by plural and
// every "{adjective}" by adjective, scanning left to right; replacement text
// is not rescanned, and any other '{' is copied as it is.
[[nodiscard]] std::string age_name_text(std::string_view form, std::string_view plural, std::string_view adjective);

// The errors of P3-03 section 6 (D0 to D5), each with path "races", no
// position and the message of the table of section 6. D0 first: "no race is
// defined; kind 'race' needs at least one definition" when the content has
// no race definition (or no kind race). Then D1 to D5, evaluated for every
// race definition in race order, each condition in the order D1 (and D2
// per biome entry, in list order), D3, D4, D5; then sorted and
// deduplicated (P0-03 9). D4 and D5 use history.chronology's
// age_name_form and are skipped when the content has no such definition.
// Empty with the shipped data.
[[nodiscard]] std::vector<data::LoadError> check_history_data(const data::ContentSet& content);

}  // namespace worldgen::history
