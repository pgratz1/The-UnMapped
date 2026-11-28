// The kinds of the history phase (spec P3-01 sections 3 to 5): `race`,
// `culture` with its naming record (P3-02 section 3), and the fixed-id
// tuning kind `history` (id `chronology`). Registered by builtin_kinds()
// in kinds.cpp.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/text.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::data {

namespace {

using detail::int_text;
using detail::uint_text;

// --- String grammars of record culture_naming (P3-02 section 3) ---------------

constexpr bool is_ascii_letter(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr bool is_separator(char c) noexcept {
    return c == '\'' || c == '-';
}

// Zero or more ASCII lower-case letters a-z.
bool is_valid_syllable(std::string_view text) noexcept {
    for (const char c : text) {
        if (!detail::is_lower(c)) {
            return false;
        }
    }
    return true;
}

// One or more of O, N, C, ' and -; at least one N; not starting or ending
// with ' or -; no two of ' and - adjacent.
bool is_valid_name_pattern(std::string_view text) noexcept {
    if (text.empty() || is_separator(text.front()) || is_separator(text.back())) {
        return false;
    }
    bool nucleus = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == 'N') {
            nucleus = true;
        } else if (is_separator(c)) {
            if (is_separator(text[i - 1])) {  // i > 0: text does not start with a separator
                return false;
            }
        } else if (c != 'O' && c != 'C') {
            return false;
        }
    }
    return nucleus;
}

constexpr std::string_view ROOT_PLACEHOLDER = "{root}";

// ASCII letters, space, ' and -, plus exactly one {root}; no leading or
// trailing space, no two spaces in a row.
bool is_valid_name_form(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    std::size_t roots = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{') {
            if (text.substr(i, ROOT_PLACEHOLDER.size()) != ROOT_PLACEHOLDER) {
                return false;
            }
            ++roots;
            i += ROOT_PLACEHOLDER.size();
            continue;
        }
        if (c == ' ') {
            if (i + 1 < text.size() && text[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && !is_separator(c)) {
            return false;
        }
        ++i;
    }
    return roots == 1;
}

StringGrammar syllable_grammar() noexcept {
    return StringGrammar{"a syllable (lower-case letters a-z, possibly empty)", &is_valid_syllable};
}

StringGrammar name_pattern_grammar() noexcept {
    return StringGrammar{"a name pattern (O, N, C, ' and -, with at least one N)", &is_valid_name_pattern};
}

StringGrammar name_form_grammar() noexcept {
    return StringGrammar{"a name form (letters, spaces, ' and -, with exactly one {root})", &is_valid_name_form};
}

// P5-06 4.3: epithet: ASCII letters, single spaces and -, not starting or
// ending with a space (and not empty).
bool is_valid_epithet(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ') {
            if (text[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && c != '-') {
            return false;
        }
    }
    return true;
}

// P5-06 4.4 (engineer's reading): an epithets entry's text is an epithet or
// the slot {slayer} alone.
bool is_valid_epithet_text(std::string_view text) noexcept {
    return text == "{slayer}" || is_valid_epithet(text);
}

// P5-06 4.2: book_title: ASCII letters, single spaces, ' and -, starting
// with an upper-case letter.
bool is_valid_book_title(std::string_view text) noexcept {
    if (text.empty() || !(text.front() >= 'A' && text.front() <= 'Z') || text.back() == ' ') {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ') {
            if (text[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && !is_separator(c)) {
            return false;
        }
    }
    return true;
}

StringGrammar epithet_grammar() noexcept {
    return StringGrammar{"an epithet (ASCII letters, single spaces and -, not starting or ending with a space)",
                         &is_valid_epithet};
}

StringGrammar epithet_text_grammar() noexcept {
    return StringGrammar{"an epithet (ASCII letters, single spaces and -) or {slayer}", &is_valid_epithet_text};
}

StringGrammar book_title_grammar() noexcept {
    return StringGrammar{"a book title (ASCII letters, single spaces, ' and -, starting with an upper-case letter)",
                         &is_valid_book_title};
}

// P5-06 4.1, 4.4: a record {steady, warlike, peaceable, grasping, just}.
constexpr std::string_view TEMPER_FIELDS[] = {"steady", "warlike", "peaceable", "grasping", "just"};

Schema temper_schema(std::int64_t min, std::int64_t max) {
    std::vector<Field> fields;
    for (const std::string_view name : TEMPER_FIELDS) {
        fields.push_back(Field::required(std::string(name), Type::integer(min, max)));
    }
    return Schema(std::move(fields));
}

// --- race (P3-01 section 3) ---------------------------------------------------------

constexpr std::string_view SITE_WEIGHT_FIELDS[] = {"fertility", "forage", "timber", "ore", "fresh_water"};

Schema race_site_weights_schema() {
    std::vector<Field> fields;
    for (const std::string_view name : SITE_WEIGHT_FIELDS) {
        fields.push_back(Field::required(std::string(name), Type::integer(0, 1000)));
    }
    return Schema(std::move(fields));
}

// P3-01 r8 (generator 0.17.0): record race_cell_weights.
Schema race_cell_weights_schema() {
    return Schema(std::vector<Field>{
        Field::required("ore", Type::integer(0, 1000)),
        Field::required("height", Type::integer(0, 1000)),
        // P3-01 r11 3.4 (generator 0.19.0)
        Field::required("inland", Type::integer(0, 1000)),
        Field::required("port", Type::integer(0, 1000)),
        Field::required("habitability", Type::integer(0, 1000)),
    });
}

// P5-03 4.1: one entry of a race's stances.
Schema race_stance_schema() {
    return Schema(std::vector<Field>{
        Field::required("race", Type::string(32)),
        Field::required("value", Type::integer(-500, 500)),
    });
}

Schema race_biome_schema() {
    return Schema(std::vector<Field>{
        Field::required("biome", Type::reference(std::string(BIOME_KIND))),
        Field::required("weight", Type::integer(-1000, 1000)),
    });
}

std::optional<std::string> check_race_generation_below_lifespan(const Record& r) {
    if (r.get_int("generation_years") < r.get_int("lifespan_years")) {
        return std::nullopt;
    }
    return std::string("generation_years must be below lifespan_years");
}

std::optional<std::string> check_race_site_weights_positive(const Record& r) {
    const Record& w = r.get_record("site_weights");
    std::int64_t sum = 0;
    for (const std::string_view name : SITE_WEIGHT_FIELDS) {
        sum += w.get_int(name);  // five fields of at most 1000: no overflow
    }
    if (sum < 1) {
        return std::string("the site_weights must not all be 0");
    }
    return std::nullopt;
}

// The first later index i whose string field equals that of an earlier
// index j, as (i, j); std::nullopt when all are distinct.
std::optional<std::pair<std::size_t, std::size_t>> first_repeat(const std::vector<std::string>& items) {
    std::map<std::string, std::size_t, detail::BytesLess> first;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto [it, inserted] = first.try_emplace(items[i], i);
        if (!inserted) {
            return std::make_pair(i, it->second);
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_race_biomes_unique(const Record& r) {
    std::vector<std::string> ids;
    for (const Value& v : r.get_array("biomes")) {
        ids.push_back(v.as_record().get_string("biome"));
    }
    if (const auto repeat = first_repeat(ids)) {
        return "biomes[" + uint_text(repeat->first) + "]: biome '" + ids[repeat->first] +
               "' is already listed at biomes[" + uint_text(repeat->second) + "]";
    }
    return std::nullopt;
}

std::optional<std::string> check_race_settlement_kinds_unique(const Record& r) {
    std::vector<std::string> kinds;
    for (const Value& v : r.get_array("settlement_kinds")) {
        kinds.push_back(v.as_string());
    }
    if (const auto repeat = first_repeat(kinds)) {
        return "settlement_kinds[" + uint_text(repeat->first) + "]: '" + kinds[repeat->first] + "' is already listed";
    }
    return std::nullopt;
}

std::string race_name(const Definition& d) {
    return "'" + std::string(RACE_KIND) + "." + d.id + "'";
}

// Kind-level check race_order_unique: each later definition (id order)
// sharing an order is reported at its header, naming the earliest.
std::vector<KindCheckFailure> check_race_order_unique(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    std::map<std::int64_t, std::size_t> first;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const std::int64_t order = defs[i].record.get_int("order");
        const auto [it, inserted] = first.try_emplace(order, i);
        if (!inserted) {
            out.push_back(
                KindCheckFailure{i, "order " + int_text(order) + " is also used by " + race_name(defs[it->second])});
        }
    }
    return out;
}

// Kind-level check race_order_dense: the orders are exactly 1 ... n for n
// definitions; reported against the directory with the smallest missing
// order. Not run when two definitions share an order (race_order_unique
// reports that).
std::vector<KindCheckFailure> check_race_order_dense(std::span<const Definition> defs) {
    std::set<std::int64_t> orders;
    for (const Definition& d : defs) {
        if (!orders.insert(d.record.get_int("order")).second) {
            return {};  // race_order_unique failed
        }
    }
    const auto n = static_cast<std::int64_t>(defs.size());  // at most a few thousand definitions
    for (std::int64_t k = 1; k <= n; ++k) {
        if (orders.find(k) == orders.end()) {
            return {KindCheckFailure{std::nullopt, "orders must be 1 to " + int_text(n) + " without gaps; " +
                                                       int_text(k) + " is missing"}};
        }
    }
    return {};
}

// Kind-level check race_awakening_order: the race of order 1 awakens at
// share 0, and each race's share is above that of the race of the next lower
// order. A race whose order k - 1 is not defined is not compared (the
// order checks report the gap); with a shared order, the earliest
// definition (id order) of order k - 1 is the one compared against.
std::vector<KindCheckFailure> check_race_awakening_order(std::span<const Definition> defs) {
    std::map<std::int64_t, std::size_t> by_order;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        by_order.try_emplace(defs[i].record.get_int("order"), i);
    }
    std::vector<KindCheckFailure> out;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const std::int64_t order = defs[i].record.get_int("order");
        const Fx share = defs[i].record.get_fx("awakening_share");
        if (order == 1) {
            if (share != Fx{}) {
                out.push_back(KindCheckFailure{i, "the race of order 1 must have awakening_share 0"});
            }
            continue;
        }
        const auto previous = by_order.find(order - 1);
        if (previous == by_order.end()) {
            continue;
        }
        const Definition& p = defs[previous->second];
        if (share <= p.record.get_fx("awakening_share")) {
            out.push_back(KindCheckFailure{i, "awakening_share must be above that of " + race_name(p) + " (order " +
                                                  int_text(order - 1) + ")"});
        }
    }
    return out;
}

// --- culture (P3-01 section 4, P3-02 section 3) -------------------------------------

std::optional<std::string> check_naming_nuclei_nonempty(const Record& r) {
    const std::vector<Value>& nuclei = r.get_record("naming").get_array("nuclei");
    for (std::size_t i = 0; i < nuclei.size(); ++i) {
        if (nuclei[i].as_string().empty()) {
            return "naming.nuclei[" + uint_text(i) + "] must not be empty";
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_naming_letters_ordered(const Record& r) {
    const Record& naming = r.get_record("naming");
    if (naming.get_int("min_letters") > naming.get_int("max_letters")) {
        return std::string("naming.min_letters must not be above naming.max_letters");
    }
    return std::nullopt;
}

std::optional<std::string> check_naming_forbidden_nonempty(const Record& r) {
    const std::vector<Value>& forbidden = r.get_record("naming").get_array("forbidden");
    for (std::size_t i = 0; i < forbidden.size(); ++i) {
        if (forbidden[i].as_string().empty()) {
            return "naming.forbidden[" + uint_text(i) + "] must not be empty";
        }
    }
    return std::nullopt;
}

// P3-02 r5 11.4: every civilization form holds a word besides {root}.
std::optional<std::string> check_naming_realm_forms(const Record& r) {
    const std::vector<Value>& forms = r.get_record("naming").get_array("civilization_forms");
    for (std::size_t i = 0; i < forms.size(); ++i) {
        if (forms[i].as_string().find(' ') == std::string::npos) {
            return "naming.civilization_forms[" + uint_text(i) + "] must hold a word besides {root}";
        }
    }
    return std::nullopt;
}

// --- history.chronology (P3-01 section 5.1) ------------------------------------------

constexpr std::string_view AGE_PLACEHOLDERS[] = {"{plural}", "{adjective}"};

std::optional<std::string> check_chronology_age_name_form(const Record& r) {
    const std::string& form = r.get_string("age_name_form");
    std::size_t found = 0;
    std::size_t i = 0;
    bool ok = true;
    while (i < form.size()) {
        if (form[i] != '{') {
            ++i;
            continue;
        }
        bool matched = false;
        for (const std::string_view p : AGE_PLACEHOLDERS) {
            if (std::string_view(form).substr(i, p.size()) == p) {
                matched = true;
                i += p.size();
                ++found;
                break;
            }
        }
        if (!matched) {
            ok = false;
            break;
        }
    }
    if (ok && found > 0) {
        return std::nullopt;
    }
    return std::string("age_name_form must contain {plural} or {adjective} and no other placeholder");
}

// --- history.engine (P3-04 section 4.1) ------------------------------------------------

constexpr std::string_view ENGINE_LEVELS[] = {"sparse", "normal", "dense"};

std::optional<std::string> check_engine_simulated_bounds(const Record& r) {
    if (r.get_int("simulated_min_permille") <= r.get_int("simulated_max_permille")) {
        return std::nullopt;
    }
    return std::string("simulated_min_permille must not be above simulated_max_permille");
}

// P3-04 r15 4.1: makes 20.1 item 1a's decision exact.
std::optional<std::string> check_engine_quiet_min(const Record& r) {
    if (r.get_int("quiet_min_years") <= 2 * r.get_int("quiet_step_years")) {
        return std::nullopt;
    }
    return std::string("quiet_min_years must not be above 2 x quiet_step_years");
}

// P3-04 r19 22.3: no level's flare-up may be shorter than its minimum.
std::optional<std::string> check_engine_flare_min(const Record& r) {
    for (const std::string_view level : ENGINE_LEVELS) {
        if (r.get_int("flare_min_years") > r.get_record(level).get_int("flare_years")) {
            return std::string(level) + ": flare_min_years must not be above flare_years";
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_engine_phase_budget(const Record& r) {
    const std::int64_t budget = 6 * r.get_int("simulated_max_permille") - 30;  // at most 2970
    for (const std::string_view level : ENGINE_LEVELS) {
        const Record& d = r.get_record(level);
        if (6 * d.get_int("active_years") + d.get_int("final_years") > budget) {  // at most 15000
            // ASCII x and - for the spec's multiplication and minus signs (lead, 2026-11-19).
            return std::string(level) +
                   ": 6 x active_years + final_years must not be above 6 x simulated_max_permille "
                   "- 30";
        }
    }
    return std::nullopt;
}

Schema engine_detail_schema() {
    return Schema(std::vector<Field>{
        Field::required("active_years", Type::integer(1, 2000)),
        Field::required("flare_years", Type::integer(1, 1000)),
        Field::required("final_years", Type::integer(1, 3000)),
        Field::required("flare_threshold", Type::integer(1, 1000000)),
    });
}

// --- history.report (P3-04 section 4.2) ------------------------------------------------

Schema report_event_schema() {
    return Schema(std::vector<Field>{
        Field::required("kind", Type::string(32, id_grammar())),
        Field::required("salience", Type::integer(0, 100)),
        // P3-10: 512 bytes (P3-04 4.2 says 256; the cause lists of P3-05 r12
        // 22.3 make the templates of kinds 16 and 31 longer). P5-03a (a
        // reading): 1024, since the cause lists of 3.5 and 7.3 make the
        // templates of kinds 58 and 59 longer than 512. P5-08 (a reading):
        // 2048, since the cause words of 8.2 make kinds 100 to 104 longer
        // than 1024.
        Field::required("template", Type::string(2048)),
    });
}

std::vector<KindCheckFailure> check_report_events(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].id != HISTORY_REPORT_ID) {
            continue;
        }
        for (std::string& message : history::report_events_problems(defs[i].record)) {
            out.push_back(KindCheckFailure{i, std::move(message)});
        }
    }
    return out;
}

// --- history.peoples (P3-05 section 12.1) ----------------------------------------------

Schema peoples_kind_schema(std::int64_t max) {
    return Schema(std::vector<Field>{
        Field::required("hamlet", Type::integer(0, max)),
        Field::required("village", Type::integer(0, max)),
        Field::required("town", Type::integer(0, max)),
        Field::required("city", Type::integer(0, max)),
        Field::required("fortress", Type::integer(0, max)),
    });
}

Schema peoples_intensity_schema() {
    return Schema(std::vector<Field>{
        Field::required("calm", Type::integer(0, 10000)),
        Field::required("normal", Type::integer(0, 10000)),
        Field::required("turbulent", Type::integer(0, 10000)),
    });
}

std::optional<std::string> check_peoples_ladder(const Record& r) {
    if (r.get_int("village_min") < r.get_int("town_min") && r.get_int("town_min") < r.get_int("city_min") &&
        r.get_int("city_min") <= r.get_int("city_max")) {
        return std::nullopt;
    }
    return std::string("village_min, town_min, city_min must increase and city_max must not be below city_min");
}

std::optional<std::string> check_peoples_tolls(const Record& r) {
    if (r.get_int("famine_toll_min") <= r.get_int("famine_toll_max") &&
        r.get_int("plague_toll_min") <= r.get_int("plague_toll_max")) {
        return std::nullopt;
    }
    return std::string("a toll minimum must not be above its maximum");
}

std::optional<std::string> check_peoples_floor(const Record& r) {
    if (r.get_int("capacity_floor") >= r.get_int("founding_population")) {
        return std::nullopt;
    }
    return std::string("capacity_floor must not be below founding_population");
}

std::optional<std::string> check_peoples_colonists(const Record& r) {
    if (r.get_int("abandon_min") < r.get_int("colonists_min")) {
        return std::nullopt;
    }
    return std::string("colonists_min must be above abandon_min");
}

}  // namespace

Schema history_peoples_schema() {
    // P3-05 12.1, in the table's order.
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    std::vector<Field> fields{
        i("score_site", 0, 2000),
        i("score_biome", 0, 2000),
        i("score_upland", 0, 2000),
        i("score_coast", 0, 2000),
        i("ore_reach_full", 1, 100000),
        i("coast_full", 1, 100000),
        i("rough_base", 0, 1000),
        i("rough_upland", 0, 1000),
        i("people_per_kcell", 1, 100000),
        i("capacity_floor", 1, 100000),
        i("capital_weight", 1000, 10000),
        i("founding_population", 1, 10000),
        i("village_min", 1, 1000000),
        i("town_min", 1, 1000000),
        i("city_min", 1, 1000000),
        i("city_max", 1, 1000000),
        i("fortress_max", 1, 1000000),
        i("kind_shrink_permille", 0, 1000),
        i("site_spacing", 1, 1000),
        i("region_slots_max", 1, 64),
        i("mouth_class_min", 1, 15),
        i("port_radius", 0, 16),
        i("port_mouth_bonus", 0, 255),
        i("port_coast_bonus", 0, 255),
        i("decline_permille", 1, 1000),
        i("recover_permille", 0, 1000),
        i("colonist_permille", 1, 1000),
        i("colonists_min", 1, 10000),
        i("abandon_min", 1, 10000),
        i("abandon_frail_max", 1, 1000000),
        Field::required("abandon_permille", Type::record(peoples_kind_schema(1000))),
        i("found_pressure_permille", 1, 2000),
        i("found_rate_permille", 0, 1000),
        i("quiet_found_permille", 0, 1000),
        i("reach_base", 0, 64),
        i("reach_per_expansion", 0, 64),
        i("expand_min_score", -3000, 4000),
        i("expand_relative_permille", 0, 1000),
        i("expand_min_capacity", 0, 1000000),
        i("cost_penalty", 0, 10000),
        i("pass_cost", 1, 100),
        i("river_cost", 0, 100),
        i("river_border_share_permille", 0, 1000),
        i("civs_two_permille", 0, 1000),
        i("regions_per_civ", 0, 100),
        i("regions_per_extra_civ", 1, 100000),  // P3-05 r8 21 item 5 (generator 0.18.0)
        i("civs_extra_max", 0, 3),
        i("cause_memory_years", 0, 100000),  // P3-05 r12 22.2 (generator 0.19.0)
        i("inland_cap", 1, 256),             // P3-05 r12 22.5
        i("home_min_hops", 1, 64),
        i("home_min_capacity", 0, 1000000),
        Field::required("intensity", Type::record(peoples_intensity_schema())),
        i("famine_crowding_permille", 0, 2000),
        i("famine_ppm", 0, 1000000),
        i("famine_toll_min", 0, 1000),
        i("famine_toll_max", 0, 1000),
        Field::required("plague_ppm", Type::record(peoples_kind_schema(1000000))),
        i("plague_toll_min", 0, 1000),
        i("plague_toll_max", 0, 1000),
        i("plague_spread_permille", 0, 1000),
        i("plague_spread_toll_permille", 0, 1000),
        i("crisis_per_generation_permille", 0, 1000),
        i("crisis_loss_permille", 0, 1000),
        i("split_min_settlements", 2, 1000),
        i("split_min_regions", 2, 1000),
        i("split_permille", 0, 1000),
        i("colony_new_region_salience", 0, 100),
        i("plague_small_salience", 0, 100),
        i("plague_major_dead", 0, 1000000),   // r6 (generator 0.17.0)
        i("crisis_small_salience", 0, 100),   // r6 (generator 0.17.0)
        i("ghost_permille", 0, 1000),         // P5-02 4.5 (generator 0.23.0)
        i("ghost_max_population", 0, 1000000),
        // P5-03 11.1, 11.2 (generator 0.24.0).
        i("abandon_reach_hops", 0, 16),
        i("abandon_loss_min_permille", 0, 1000),
        i("abandon_loss_max_permille", 0, 1000),
        // P5-06 25.1 (generator 0.27.0): ruin_fade_years and ghost_fade_years
        // removed (every ruin stays).
    };
    std::vector<Check> checks{
        Check{"peoples_ladder", [](const Record& r) { return check_peoples_ladder(r); }},
        Check{"peoples_tolls", [](const Record& r) { return check_peoples_tolls(r); }},
        Check{"peoples_floor", [](const Record& r) { return check_peoples_floor(r); }},
        Check{"peoples_colonists", [](const Record& r) { return check_peoples_colonists(r); }},
        Check{"peoples_abandon_loss_ordered",  // P5-03 11.1
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("abandon_loss_min_permille") > r.get_int("abandon_loss_max_permille")) {
                      return "abandon_loss_min_permille " + int_text(r.get_int("abandon_loss_min_permille")) +
                             " is above abandon_loss_max_permille " + int_text(r.get_int("abandon_loss_max_permille"));
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_conflict_schema() {
    // P3-06 12.1, in the table's order.
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    const auto camp = Schema(std::vector<Field>{
        i("good_good", -1000, 1000),
        i("good_neutral", -1000, 1000),
        i("good_evil", -1000, 1000),
        i("neutral_neutral", -1000, 1000),
        i("neutral_evil", -1000, 1000),
        i("evil_evil", -1000, 1000),
    });
    const auto by_kind = [&i](std::int64_t min, std::int64_t max) {
        return Schema(std::vector<Field>{
            i("hamlet", min, max),
            i("village", min, max),
            i("town", min, max),
            i("city", min, max),
            i("fortress", min, max),
        });
    };
    std::vector<Field> fields{
        Field::required("camp", Type::record(camp)),
        i("enmity_max", -1000, 1000),
        i("border_friction", 0, 2000),
        i("want_weight", 0, 2000),
        i("hunger_full", 1, 100000),
        i("fear_share", 0, 999),
        i("fear_weight", 0, 2000),
        i("tick_strain_permille", 0, 1000),
        i("quiet_strain_permille", 0, 1000),
        i("strain_max", 1, 1000000),
        i("claim_strain", 1, 1000000),
        i("declare_min", 0, 1000000),
        i("declare_span", 1, 1000000),
        i("declare_permille", 0, 1000),
        i("declare_ratio", 1, 10000),
        i("shelter_years", 0, 1000),
        i("march_hops", 1, 4),
        i("cost_penalty", 0, 10000),
        Field::required("levy_permille", Type::record(by_kind(0, 1000))),
        i("terrain_defense", 0, 10000),
        i("pass_defense", 0, 10000),
        i("river_defense", 0, 10000),
        i("fort_defense", 0, 10000),
        i("battle_permille", 0, 1000),
        i("loss_loser_permille", 0, 1000),
        i("loss_winner_permille", 0, 1000),
        i("garrison_permille", 0, 1000),
        Field::required("walls_permille", Type::record(by_kind(1, 100000))),
        i("capital_walls", 0, 100000),
        i("sack_base", 0, 1000),
        i("sack_hostility", 0, 2000),
        i("sack_toll_permille", 0, 1000),
        i("capture_toll_permille", 0, 1000),
        i("exhaustion_permille", 1, 1000),
        i("war_max_years", 1, 1000),
        i("truce_years", 0, 10000),
        i("war_grudge", 0, 2000),
        i("capture_grudge", 0, 2000),
        i("secession_grudge", 0, 2000),
        i("grudge_decay_per_century", 0, 10000),
        i("last_refuge", 0, 1),
        // P3-06 r9 20.9 (P3-09, generator 0.18.0), in the table's order.
        i("refuge_flight", 0, 1),
        i("flight_hops", 1, 8),
        // P3-06 r17 22.2: hidden_years is a race field (removed here).
        i("weary_wars", 0, 100),
        i("tribute_permille", 0, 1000),
        i("revenge_aggression", 0, 1000),
        i("levy_hops", 0, 16),
        i("odds_floor", 0, 1000),
        i("truce_break_aggression", 0, 999),
        i("enmity_hops", 1, 4),
        // P3-06 r10 20.19 (P3-09 r3).
        i("kill_max_permille", 1, 10000),
        i("find_permille", 0, 1000),
        i("search_years", 1, 100),
        i("search_weary_years", 0, 10000),   // P3-06 r14 21.7 (generator 0.19.0)
        i("search_repeat_salience", 0, 100),
        i("quarrel_strain", 0, 5000),        // P3-06 r15 21.12 item 2
        i("quarrel_min_settlements", 0, 1000),  // P3-06 r17 22.1
        i("quarrel_strength_permille", 0, 1000),  // P3-06 r22 24.2 (generator 0.21.0)
        i("toll_spread_permille", 0, 500),        // P3-06 r22 24.5
        i("capital_sacked_salience", 0, 100),
        i("capital_captured_salience", 0, 100),
        // P5-04 4.3 (generator 0.26.0), appended in this order.
        i("allied_strain_permille", 0, 1000),
        i("ally_deterrence_permille", 0, 1000),
        i("ally_effort_permille", 0, 1000),
        i("trade_strength_permille", 0, 1000),
        i("trade_strength_max", 0, 16),
        i("forge_strength_permille", 0, 1000),
        i("arms_strength_permille", 0, 1000),
        i("forge_depth_permille", 0, 1000),  // P5-07 4.3
    };
    std::vector<Check> checks{
        Check{"conflict_declare",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("claim_strain") <= r.get_int("declare_min")) {
                      return std::nullopt;
                  }
                  return std::string("claim_strain must not be above declare_min");
              }},
        Check{"conflict_strain_max",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("declare_min") + r.get_int("declare_span") <= r.get_int("strain_max")) {
                      return std::nullopt;
                  }
                  return std::string("declare_min + declare_span must not be above strain_max");
              }},
        Check{"conflict_enmity_hops",  // P3-06 r9 20.9
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("enmity_hops") >= r.get_int("march_hops")) {
                      return std::nullopt;
                  }
                  return std::string("enmity_hops must not be below march_hops");
              }},
        Check{"conflict_quarrel_strain",  // P3-06 r15 21.12 item 2
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("quarrel_strain") >= r.get_int("claim_strain")) {
                      return std::nullopt;
                  }
                  return std::string("quarrel_strain must not be below claim_strain");
              }},
        Check{"conflict_quarrel_strain_max",  // P3-06 r17 22.7
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("quarrel_strain") <= r.get_int("strain_max")) {
                      return std::nullopt;
                  }
                  return std::string("quarrel_strain must not be above strain_max");
              }},
        Check{"conflict_toll_spread",  // P3-06 r22 24.5
              [](const Record& r) -> std::optional<std::string> {
                  const std::int64_t s = r.get_int("toll_spread_permille");
                  const std::int64_t bound =
                      std::min(r.get_int("loss_loser_permille"), r.get_int("kill_max_permille"));
                  if (r.get_int("loss_winner_permille") * (1000 + s) <= bound * (1000 - s)) {
                      return std::nullopt;
                  }
                  return std::string("toll_spread_permille lets the victors lose more than the defeated");
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_engine_schema() {
    // P3-04 4.1, in declaration order.
    std::vector<Field> fields{
        Field::required("quiet_step_years", Type::integer(1, 200)),
        Field::required("quiet_events_max", Type::integer(1, 100)),
        Field::required("quiet_min_years", Type::integer(0, 400)),  // P3-04 r15 20.1 item 1a
        Field::required("flare_min_years", Type::integer(1, 1000)),  // P3-04 r19 22.3
        Field::required("simulated_min_permille", Type::integer(100, 500)),
        Field::required("simulated_max_permille", Type::integer(100, 500)),
        Field::required("islet_max_dry_cells", Type::integer(0, 1000000)),
        Field::required("sparse", Type::record(engine_detail_schema())),
        Field::required("normal", Type::record(engine_detail_schema())),
        Field::required("dense", Type::record(engine_detail_schema())),
    };
    std::vector<Check> checks{
        Check{"engine_simulated_bounds", [](const Record& r) { return check_engine_simulated_bounds(r); }},
        Check{"engine_quiet_min", [](const Record& r) { return check_engine_quiet_min(r); }},  // P3-04 r15
        Check{"engine_flare_min", [](const Record& r) { return check_engine_flare_min(r); }},  // P3-04 r19
        Check{"engine_phase_budget", [](const Record& r) { return check_engine_phase_budget(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_report_schema() {
    // P3-04 4.2.
    return Schema(std::vector<Field>{
        Field::required("events", Type::array(Type::record(report_event_schema()), 1, 256)),
    });
}

// --- history.renaming (P5-12 section 4, P3-01 r16 5.4) ---------------------------

namespace {

constexpr std::string_view NAME_SLOT = "{name}";
constexpr std::string_view NOUN_SLOT = "{noun}";
constexpr std::string_view STEM_SLOT = "{stem}";  // P5-02 21.2

// ASCII letters, space and ', plus {name} and {noun} each at most once; at
// least one letter outside the slots; no leading, trailing or double space;
// no other { or }.
bool is_valid_rename_form(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    std::size_t names = 0;
    std::size_t nouns = 0;
    std::size_t stems = 0;
    bool letter = false;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{') {
            if (text.substr(i, NAME_SLOT.size()) == NAME_SLOT) {
                ++names;
                i += NAME_SLOT.size();
                continue;
            }
            if (text.substr(i, NOUN_SLOT.size()) == NOUN_SLOT) {
                ++nouns;
                i += NOUN_SLOT.size();
                continue;
            }
            if (text.substr(i, STEM_SLOT.size()) == STEM_SLOT) {
                ++stems;
                i += STEM_SLOT.size();
                continue;
            }
            return false;
        }
        if (c == ' ') {
            if (i + 1 < text.size() && text[i + 1] == ' ') {
                return false;
            }
        } else if (is_ascii_letter(c)) {
            letter = true;
        } else if (c != '\'') {
            return false;
        }
        ++i;
    }
    return names <= 1 && nouns <= 1 && stems <= 1 && names + stems <= 1 && letter;
}

StringGrammar rename_form_grammar() noexcept {
    return StringGrammar{"a region name form (letters, spaces, ' and the slots {name} or {stem}, and {noun})",
                         &is_valid_rename_form};
}

Schema rename_rule_schema() {
    return Schema(std::vector<Field>{
        Field::required("key", Type::string(32, id_grammar())),
        Field::required("kinds", Type::array(Type::string(32), 1, 8)),
        Field::required("values", Type::array(Type::string(32), 0, 4)),
        Field::required("min_magnitude", Type::integer(1, 1000000000)),
        Field::required("window_years", Type::integer(1, 100000)),
        Field::required("min_salience", Type::integer(0, 100)),
        Field::required("capital", Type::boolean()),
        Field::required("chance_permille", Type::integer(0, 1000)),
        Field::required("weight", Type::integer(1, 1000)),
        Field::required("name_role", Type::string(32)),
        Field::required("namer_role", Type::string(32)),
        Field::required("forms", Type::array(Type::string(48, rename_form_grammar()), 1, 16)),
    });
}

std::optional<std::string> check_renaming_rule_keys(const Record& r) {
    const std::vector<Value>& rules = r.get_array("rules");
    for (std::size_t j = 0; j < rules.size(); ++j) {
        for (std::size_t i = 0; i < j; ++i) {
            const std::string& k = rules[j].as_record().get_string("key");
            if (rules[i].as_record().get_string("key") == k) {
                return "rules[" + uint_text(j) + "] key '" + k + "' repeats rules[" + uint_text(i) + "]";
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_renaming_rule_weights(const Record& r) {
    const std::vector<Value>& rules = r.get_array("rules");
    for (std::size_t j = 0; j < rules.size(); ++j) {
        for (std::size_t i = 0; i < j; ++i) {
            const std::int64_t w = rules[j].as_record().get_int("weight");
            if (rules[i].as_record().get_int("weight") == w) {
                return "rules[" + uint_text(j) + "] weight " + int_text(w) + " repeats rules[" + uint_text(i) + "]";
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_renaming_rule_lists(const Record& r) {
    const std::vector<Value>& rules = r.get_array("rules");
    for (std::size_t i = 0; i < rules.size(); ++i) {
        for (const std::string_view field : {std::string_view("kinds"), std::string_view("values"),
                                             std::string_view("forms")}) {
            const std::vector<Value>& list = rules[i].as_record().get_array(field);
            for (std::size_t j = 0; j < list.size(); ++j) {
                for (std::size_t k = 0; k < j; ++k) {
                    if (list[k].as_string() == list[j].as_string()) {
                        return "rules[" + uint_text(i) + "]." + std::string(field) + "[" + uint_text(j) + "] '" +
                               list[j].as_string() + "' repeats " + std::string(field) + "[" + uint_text(k) + "]";
                    }
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_renaming_forms_slots(const Record& r) {
    const std::vector<Value>& rules = r.get_array("rules");
    for (std::size_t i = 0; i < rules.size(); ++i) {
        const Record& rule = rules[i].as_record();
        if (!rule.get_string("name_role").empty()) {
            continue;
        }
        const std::vector<Value>& forms = rule.get_array("forms");
        for (std::size_t j = 0; j < forms.size(); ++j) {
            if (forms[j].as_string().find(NAME_SLOT) != std::string::npos) {
                return "rules[" + uint_text(i) + "].forms[" + uint_text(j) + "] uses {name} but name_role is empty";
            }
            if (forms[j].as_string().find(STEM_SLOT) != std::string::npos) {  // P5-02 21.2
                return "rules[" + uint_text(i) + "].forms[" + uint_text(j) + "] uses {stem} but name_role is empty";
            }
        }
    }
    return std::nullopt;
}

}  // namespace

Schema history_renaming_schema() {
    std::vector<Field> fields{
        Field::required("region_max", Type::integer(0, 16)),
        Field::required("region_gap_years", Type::integer(1, 100000)),
        Field::required("world_max_permille", Type::integer(0, 1000)),
        Field::required("century_max_permille", Type::integer(0, 1000)),
        Field::required("century_years", Type::integer(1, 100000)),
        Field::required("rules", Type::array(Type::record(rename_rule_schema()), 0, 64)),
    };
    std::vector<Check> checks{
        Check{"renaming_rule_keys", [](const Record& r) { return check_renaming_rule_keys(r); }},
        Check{"renaming_rule_weights", [](const Record& r) { return check_renaming_rule_weights(r); }},
        Check{"renaming_rule_lists", [](const Record& r) { return check_renaming_rule_lists(r); }},
        Check{"renaming_forms_slots", [](const Record& r) { return check_renaming_forms_slots(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

std::vector<KindCheck> history_kind_checks() {
    std::vector<KindCheck> out;
    out.push_back(KindCheck{"report_events", [](std::span<const Definition> d) { return check_report_events(d); }});
    return out;
}

namespace {
}  // namespace

Schema race_schema() {
    // P3-01 section 3, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    std::vector<Field> fields{
        Field::required("order", Type::integer(1, 6)),
        Field::required("name", Type::string(32, word_grammar())),
        Field::required("plural", Type::string(32, word_grammar())),
        Field::required("adjective", Type::string(32, word_grammar())),
        Field::required("camp", Type::enumeration({"good", "neutral", "evil"})),
        Field::required("awakening_share", Type::fixed("0", "1")),
        Field::required("lifespan_years", Type::integer(1, 10000)),
        Field::required("generation_years", Type::integer(1, 1000)),
        Field::required("growth_permille", Type::integer(0, 1000)),
        Field::required("density_permille", Type::integer(1, 1000)),  // P3-05 12.2
        Field::required("site_weights", Type::record(race_site_weights_schema())),
        Field::required("cell_weights", Type::record(race_cell_weights_schema())),  // P3-01 r8
        Field::required("biomes", Type::array(Type::record(race_biome_schema()), 0, 16)),
        Field::required("upland", Type::integer(-1000, 1000)),
        Field::required("coast", Type::integer(-1000, 1000)),
        Field::required("expansion", Type::integer(0, 1000)),
        Field::required("aggression", Type::integer(0, 1000)),
        Field::required("prowess_permille", Type::integer(1, 4000)),  // P3-06 12.2
        // P3-01 r9 3.2 (P3-06 20.12, generator 0.18.0)
        Field::required("muster_permille", Type::integer(1, 10000)),
        Field::required("conceal_min", Type::integer(0, 1001)),
        Field::required("home_defense_permille", Type::integer(0, 10000)),
        // P3-01 r11 3.5 (P3-06 r14 21.9, 21.12; generator 0.19.0)
        Field::required("odds_min_permille", Type::integer(0, 1000)),
        Field::required("awakening_quarrel", Type::integer(0, 1)),
        Field::required("hidden_years", Type::integer(0, 10000)),  // P3-01 r13 3.6 (P3-06 r17 22.2)
        Field::required("awakening_population", Type::integer(0, 100000)),  // P3-01 r14 3.7 (P3-05 r16 23)
        Field::required("settlement_kinds",
                        Type::array(Type::enumeration({"hamlet", "village", "town", "city", "fortress"}), 1, 5)),
        Field::required("culture", Type::reference(std::string(CULTURE_KIND))),
        // P5-03 4.1 (generator 0.24.0): the race's leaning toward other races.
        Field::required("stances", Type::array(Type::record(race_stance_schema()), 0, 8)),
        // P5-04 4.1 (generator 0.26.0): allies by origin, the readiness to
        // join an ally's war and to open mines.
        Field::required("origin_allies", Type::array(Type::string(32), 0, 8)),
        Field::required("ally_join_permille", Type::integer(0, 1000)),
        Field::required("mining_permille", Type::integer(0, 1000)),
        // P5-06 4.1 (generator 0.27.0): tempers, generals, heroes and magic.
        Field::required("temper_weights", Type::record(temper_schema(0, 1000))),
        Field::required("general_permille", Type::integer(0, 1000)),
        Field::required("hero_permille", Type::integer(0, 1000)),
        Field::required("magic_permille", Type::integer(0, 1000)),
        // P5-07 r2 4.5 (generator 0.29.0): the pace of deep warren delving;
        // absent counts 0.
        Field::optional("delving_permille", Type::integer(0, 1000), Value::integer(0)),
    };
    std::vector<Check> checks{
        Check{"race_delves_one_way",  // P5-07 r2 4.5
              [](const Record& r) -> std::optional<std::string> {
                  const Value* d = r.find("delving_permille");
                  if (r.get_int("mining_permille") > 0 && d != nullptr && d->as_int() > 0) {
                      return std::string("a race delves mines or warrens, not both");
                  }
                  return std::nullopt;
              }},
        Check{"race_generation_below_lifespan", [](const Record& r) { return check_race_generation_below_lifespan(r); }},
        Check{"race_site_weights_positive", [](const Record& r) { return check_race_site_weights_positive(r); }},
        Check{"race_biomes_unique", [](const Record& r) { return check_race_biomes_unique(r); }},
        Check{"race_settlement_kinds_unique", [](const Record& r) { return check_race_settlement_kinds_unique(r); }},
        Check{"race_temper_weights",  // P5-06 4.1
              [](const Record& r) -> std::optional<std::string> {
                  const Record& w = r.get_record("temper_weights");
                  std::int64_t sum = 0;
                  for (const std::string_view t : TEMPER_FIELDS) {
                      sum += w.get_int(t);  // five fields of at most 1000
                  }
                  if (sum < 1) {
                      return std::string("temper_weights must not all be 0");
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

std::vector<KindCheck> race_kind_checks() {
    std::vector<KindCheck> out;
    out.push_back(KindCheck{"race_order_unique", [](std::span<const Definition> d) { return check_race_order_unique(d); }});
    out.push_back(KindCheck{"race_order_dense", [](std::span<const Definition> d) { return check_race_order_dense(d); }});
    out.push_back(
        KindCheck{"race_awakening_order", [](std::span<const Definition> d) { return check_race_awakening_order(d); }});
    return out;
}

Schema culture_naming_schema() {
    // P3-02 section 3, in declaration order.
    const auto syllables = [](std::uint64_t min_len, std::uint64_t max_len) {
        return Type::array(Type::string(8, syllable_grammar()), min_len, max_len);
    };
    const auto forms = [] { return Type::array(Type::string(48, name_form_grammar()), 1, 8); };
    const auto god_forms = [] { return Type::array(Type::string(48, god_form_string_grammar()), 1, 8); };
    return Schema(std::vector<Field>{
        Field::required("onsets", syllables(1, 64)),
        Field::required("nuclei", syllables(1, 64)),
        Field::required("codas", syllables(1, 64)),
        Field::required("patterns", Type::array(Type::string(16, name_pattern_grammar()), 1, 16)),
        // P3-02 r5 11.4 (generator 0.17.0): the shapes of a god's root.
        Field::required("god_patterns", Type::array(Type::string(16, name_pattern_grammar()), 1, 16)),
        Field::required("min_letters", Type::integer(1, 32)),
        Field::required("max_letters", Type::integer(1, 32)),
        Field::required("forbidden", syllables(0, 32)),
        Field::required("god_forms", forms()),
        Field::required("civilization_forms", forms()),
        Field::required("settlement_forms", forms()),
        Field::required("site_forms", forms()),  // P5-02 4.4 (generator 0.23.0)
        // P5-06 4.2 (generator 0.27.0).
        Field::required("person_forms", forms()),
        Field::required("tower_forms", forms()),
        Field::required("library_forms", forms()),
        Field::required("book_titles", Type::array(Type::string(48, book_title_grammar()), 4, 32)),
        Field::required("regnal_permille", Type::integer(0, 1000)),
        // P5-08 4.4 (generator 0.30.0).
        Field::required("cult_forms", god_forms()),
        Field::required("temple_forms", god_forms()),
        Field::required("sanctum_forms", god_forms()),
        Field::required("sacred_forms", god_forms()),
        Field::required("profane_forms", god_forms()),
        // P5-11 4.2 (generator 0.31.0).
        Field::required("artifact_forms", Type::array(Type::string(48, artifact_form_string_grammar()), 1, 8)),
    });
}

Schema culture_schema() {
    // P3-01 section 4, in declaration order.
    std::vector<Field> fields{
        Field::required("race", Type::reference(std::string(RACE_KIND))),
        Field::required("name", Type::string(32, word_grammar())),
        Field::required("naming", Type::record(culture_naming_schema())),
    };
    // P3-02 section 3: checks of the naming record, run as checks of the
    // culture definition so that they are reported for 'culture.<id>'.
    std::vector<Check> checks{
        Check{"naming_nuclei_nonempty", [](const Record& r) { return check_naming_nuclei_nonempty(r); }},
        Check{"naming_letters_ordered", [](const Record& r) { return check_naming_letters_ordered(r); }},
        Check{"naming_forbidden_nonempty", [](const Record& r) { return check_naming_forbidden_nonempty(r); }},
        Check{"naming_realm_forms", [](const Record& r) { return check_naming_realm_forms(r); }},  // P3-02 r5
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_chronology_schema() {
    // P3-01 section 5.1, in declaration order.
    std::vector<Field> fields{
        Field::required("awakening_jitter_frac", Type::fixed("0", "0.05")),
        Field::required("awakening_min_gap_years", Type::integer(1, 2000)),
        Field::required("age_name_form", Type::string(64)),
        Field::required("creator_title", Type::string(40, creator_title_string_grammar())),  // P5-08 4.2
    };
    std::vector<Check> checks{
        Check{"chronology_age_name_form", [](const Record& r) { return check_chronology_age_name_form(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

// --- beast, monster and history.monsters (P5-02 4.1 to 4.3) ------------------------

namespace {

// display_name: ASCII letters and space, starting with an upper-case letter.
bool is_valid_display_name(std::string_view text) noexcept {
    if (text.empty() || !(text.front() >= 'A' && text.front() <= 'Z')) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char c) { return is_ascii_letter(c) || c == ' '; });
}

// beast_plural: lower-case ASCII letters and single spaces, starting and
// ending with a letter.
bool is_valid_beast_plural(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ') {
            if (text[i + 1] == ' ') {
                return false;
            }
        } else if (!(c >= 'a' && c <= 'z')) {
            return false;
        }
    }
    return true;
}

// monster_noun: lower-case ASCII letters, space and ', starting with "a " or
// "an ".
bool is_valid_monster_noun(std::string_view text) noexcept {
    if (!(text.substr(0, 2) == "a " || text.substr(0, 3) == "an ")) {
        return false;
    }
    return std::all_of(text.begin(), text.end(),
                       [](char c) { return (c >= 'a' && c <= 'z') || c == ' ' || c == '\''; });
}

StringGrammar display_name_grammar() noexcept {
    return StringGrammar{"a display name (ASCII letters and space, starting with an upper-case letter)",
                         &is_valid_display_name};
}

StringGrammar beast_plural_grammar() noexcept {
    return StringGrammar{"a plural (lower-case letters and single spaces, starting and ending with a letter)",
                         &is_valid_beast_plural};
}

StringGrammar monster_noun_grammar() noexcept {
    return StringGrammar{"a monster noun (lower-case letters, space and ', starting with 'a ' or 'an ')",
                         &is_valid_monster_noun};
}

Schema habitat_entry_schema() {
    return Schema(std::vector<Field>{
        Field::required("biome", Type::string(32)),
        Field::required("weight", Type::integer(0, 1000)),
    });
}

// The repeated-biome half of beast_habitat_biomes and monster_biomes; the
// other half (every biome names a biome definition) needs the biome kind
// and is checked by history::check_history_data (P5-02 4.1, reading 1).
std::optional<std::string> check_habitat_repeats(const Record& r, std::string_view field) {
    const std::vector<Value>& list = r.get_array(field);
    for (std::size_t j = 0; j < list.size(); ++j) {
        const std::string& b = list[j].as_record().get_string("biome");
        for (std::size_t k = 0; k < j; ++k) {
            if (list[k].as_record().get_string("biome") == b) {
                return std::string(field) + "[" + uint_text(j) + "].biome '" + b + "' is listed twice";
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_monster_wild_habitat(const Record& r) {
    if (r.get_string("wake") != "wild") {
        return std::nullopt;
    }
    for (const Value& v : r.get_array("biomes")) {
        if (v.as_record().get_int("weight") > 0) {
            return std::nullopt;
        }
    }
    return std::string("a wild monster needs a biome with a weight above 0");
}

// P5-07 4.1: a deep kind wakes only from a mine, so it has no biomes.
std::optional<std::string> check_monster_deep_biomes(const Record& r) {
    if (r.get_string("wake") == "deep" && !r.get_array("biomes").empty()) {
        return std::string("a deep monster has no biomes");
    }
    return std::nullopt;
}

std::optional<std::string> check_monster_toll_ordered(const Record& r) {
    if (r.get_int("toll_min_permille") > r.get_int("toll_max_permille")) {
        return "toll_min_permille " + int_text(r.get_int("toll_min_permille")) + " is above toll_max_permille " +
               int_text(r.get_int("toll_max_permille"));
    }
    return std::nullopt;
}

std::optional<std::string> check_monster_overruns_unique(const Record& r) {
    const std::vector<Value>& list = r.get_array("overruns");
    for (std::size_t j = 0; j < list.size(); ++j) {
        for (std::size_t k = 0; k < j; ++k) {
            if (list[k].as_string() == list[j].as_string()) {
                return "overruns[" + uint_text(j) + "] '" + list[j].as_string() + "' repeats overruns[" +
                       uint_text(k) + "]";
            }
        }
    }
    return std::nullopt;
}

// P5-03 9.5: phrase: lower-case ASCII letters, single spaces, ' and -,
// starting and ending with a letter.
bool is_valid_phrase(std::string_view text) noexcept {
    const auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
    if (text.empty() || !lower(text.front()) || !lower(text.back())) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ') {
            if (text[i + 1] == ' ') {
                return false;
            }
        } else if (!lower(c) && c != '\'' && c != '-') {
            return false;
        }
    }
    return true;
}

StringGrammar phrase_grammar() noexcept {
    return StringGrammar{"a phrase (lower-case letters, single spaces, ' and -, starting and ending with a letter)",
                         &is_valid_phrase};
}

constexpr std::string_view PLACE_PLACEHOLDER = "{place}";

// P5-03 12.2: a monster's name form: a name form with exactly one slot,
// {root} or {place}.
bool is_valid_monster_form(std::string_view text) noexcept {
    if (is_valid_name_form(text)) {
        return true;
    }
    const std::size_t at = text.find(PLACE_PLACEHOLDER);
    if (at == std::string_view::npos || text.find(PLACE_PLACEHOLDER, at + 1) != std::string_view::npos) {
        return false;
    }
    std::string as_root(text.substr(0, at));
    as_root += ROOT_PLACEHOLDER;
    as_root += text.substr(at + PLACE_PLACEHOLDER.size());
    return is_valid_name_form(as_root);
}

StringGrammar monster_form_grammar() noexcept {
    return StringGrammar{"a name form (letters, spaces, ' and -, with exactly one {root} or {place})",
                         &is_valid_monster_form};
}

std::optional<std::string> check_monster_place_forms(const Record& r) {
    const bool dead = r.get_string("wake") == "dead";
    const std::vector<Value>& forms = r.get_array("name_forms");
    for (std::size_t j = 0; j < forms.size(); ++j) {
        const std::string& f = forms[j].as_string();
        const bool place = f.find(PLACE_PLACEHOLDER) != std::string::npos;
        const bool root = f.find(ROOT_PLACEHOLDER) != std::string::npos;
        if (dead && (!place || root)) {
            return "name_forms[" + uint_text(j) + "] '" + f + "' of a dead kind needs {place} and no {root}";
        }
        if (!dead && place) {
            return "name_forms[" + uint_text(j) + "] '" + f + "' holds {place}, which only a dead kind may";
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_monster_bare_form_last(const Record& r) {
    const std::vector<Value>& forms = r.get_array("name_forms");
    for (std::size_t j = 0; j + 1 < forms.size(); ++j) {
        if (forms[j].as_string() == ROOT_PLACEHOLDER) {
            return "name_forms[" + uint_text(j) + "] '{root}' must be the last form";
        }
    }
    return std::nullopt;
}

}  // namespace

Schema beast_schema() {
    std::vector<Field> fields{
        Field::required("name", Type::string(32, display_name_grammar())),
        Field::required("plural", Type::string(32, beast_plural_grammar())),
        Field::required("habitat", Type::array(Type::record(habitat_entry_schema()), 1, 16)),
        Field::required("density", Type::integer(0, 100000)),
        Field::required("growth_permille", Type::integer(1, 1000)),
        Field::required("menace", Type::integer(0, 1000)),
        Field::required("food", Type::integer(0, 1000)),
        // P5-03 13 (generator 0.24.0).
        Field::required("bold_phrases", Type::array(Type::string(48, phrase_grammar()), 1, 4)),
    };
    std::vector<Check> checks{
        Check{"beast_habitat_biomes", [](const Record& r) { return check_habitat_repeats(r, "habitat"); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema monster_schema() {
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    const auto forms = [] { return Type::array(Type::string(48, name_form_grammar()), 1, 8); };
    // P5-03 12.2: a monster's name forms may hold {place} in place of {root}.
    const auto monster_forms = [] { return Type::array(Type::string(48, monster_form_grammar()), 1, 8); };
    std::vector<Field> fields{
        Field::required("name", Type::string(32, display_name_grammar())),
        Field::required("noun", Type::string(48, monster_noun_grammar())),
        Field::required("grouped", Type::boolean()),
        Field::required("wake", Type::enumeration({"wild", "dead", "deep"})),  // P5-07 4.1: deep
        Field::required("biomes", Type::array(Type::record(habitat_entry_schema()), 0, 16)),
        i("wake_rate", 0, 10000000),
        Field::required("naming_culture", Type::string(32)),
        Field::required("name_forms", monster_forms()),
        Field::required("lair_forms", forms()),
        i("might", 1, 10000000),
        i("range_hops", 0, 4),
        i("raid_permille", 0, 1000),
        i("toll_min_permille", 0, 1000),
        i("toll_max_permille", 0, 1000),
        i("kill_max", 0, 10000000),
        Field::required("overruns", Type::array(Type::enumeration({"hamlet", "village", "town"}), 0, 3)),
        i("overrun_permille", 0, 1000),
        i("active_years", 1, 100000),
        i("rewake_ppm", 0, 1000000),
        i("lifespan_years", 0, 100000),
        i("max_alive", 0, 64),
        Field::required("flare", Type::boolean()),
        i("hoard_permille", 0, 1000),
        // P5-03 9.5 (generator 0.24.0).
        Field::required("raid_phrases", Type::array(Type::string(48, phrase_grammar()), 1, 4)),
        // P5-06 4.3 (generator 0.27.0).
        Field::required("slayer_epithet", Type::string(24, epithet_grammar())),
    };
    std::vector<Check> checks{
        Check{"monster_biomes", [](const Record& r) { return check_habitat_repeats(r, "biomes"); }},
        Check{"monster_wild_habitat", [](const Record& r) { return check_monster_wild_habitat(r); }},
        Check{"monster_deep_biomes", [](const Record& r) { return check_monster_deep_biomes(r); }},  // P5-07 4.1
        Check{"monster_toll_ordered", [](const Record& r) { return check_monster_toll_ordered(r); }},
        Check{"monster_overruns_unique", [](const Record& r) { return check_monster_overruns_unique(r); }},
        Check{"monster_bare_form_last", [](const Record& r) { return check_monster_bare_form_last(r); }},
        Check{"monster_place_forms", [](const Record& r) { return check_monster_place_forms(r); }},  // P5-03 12.2
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_monsters_schema() {
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    const Schema pressure(std::vector<Field>{
        i("low", 0, 10000),
        i("normal", 0, 10000),
        i("high", 0, 10000),
    });
    const Schema prey(std::vector<Field>{
        i("hamlet", 0, 1000),
        i("village", 0, 1000),
        i("town", 0, 1000),
        i("city", 0, 1000),
        i("fortress", 0, 1000),
    });
    std::vector<Field> fields{
        Field::required("pressure", Type::record(pressure)),
        i("living_permille", 0, 1000),
        i("undead_min_dead", 1, 1000000000),
        i("undead_window_years", 1, 100000),
        i("hunt_hops", 0, 8),
        i("hunt_levy_permille", 0, 1000),
        i("hunt_permille", 0, 1000),
        i("hunt_memory_years", 1, 100000),
        i("slay_max_permille", 0, 1000),
        i("drive_permille", 0, 1000),
        i("drive_years", 0, 100000),
        i("displace_per_kpeople", 0, 1000),
        i("displace_max_permille", 0, 1000),
        i("beast_decline_permille", 1, 1000),
        i("people_floor", 1, 100000),
        i("drag_k", 0, 10000),
        i("drag_max_permille", 0, 500),
        i("feed_k", 0, 10000),
        i("feed_max_permille", 0, 500),
        i("bold_on_permille", 1, 1000000),
        i("bold_off_permille", 0, 1000000),
        i("bold_permille", 0, 1000),
        i("bold_toll_permille", 0, 1000),
        i("outpost_permille", 0, 1000),
        i("outpost_max", 0, 16),
        i("outpost_quiet_years", 1, 100000),
        Field::required("prey_weight", Type::record(prey)),
        i("prey_capital_permille", 0, 1000),
        i("prey_hop_permille", 0, 1000),
        i("prey_might_ref", 1, 10000000),
        i("prey_might_max_permille", 0, 10000),
        // P5-03 9.1 to 9.4, 14.1 (generator 0.24.0).
        i("raid_chain_years", 1, 100000),
        i("toll_over_permille", 0, 1000),
        i("hunt_raid_permille", 0, 1000),
        i("hunt_max_permille", 0, 1000),
        i("fear_raids", 1, 100),
        i("fear_years", 1, 100000),
        i("fear_permille", 0, 1000),
        i("overrun_people_ref", 1, 1000000),
        i("undead_dead_permille", 0, 1000),  // P5-03a 10.2
        i("undead_regions_ref", 0, 100000),  // P5-06 25.3 (generator 0.27.0)
        // P5-07 4.2 (generator 0.29.0): the mountain depths.
        Field::required("deep_years", Type::array(Type::integer(1, 100000), 3, 3)),
        i("deep_delve_permille", 0, 1000),
        Field::required("deep_wake_ppm", Type::array(Type::integer(0, 1000000), 3, 3)),
        i("deep_gate_levy_permille", 0, 4000),
        i("deep_seal_max_permille", 0, 1000),
        i("deep_toll_permille", 0, 1000),
        i("deep_takes_max", 0, 16),
        i("deep_reach_hops", 0, 4),
        // P5-07 r2 4.2: goblin deep warrens (6.10) and allies' aid (6.11).
        i("deep_warren_population_min", 0, 1000000),
        i("deep_warren_ore_min", 0, 100000),
        i("deep_warren_wake_permille", 0, 1000),
        i("deep_aid_permille", 0, 1000),
        i("deep_aid_hops", 0, 8),
        i("deep_aid_levy_permille", 0, 4000),
        i("deep_aid_max", 0, 8),
        i("deep_aid_memory", 0, 500),
        // P5-08 12.2 (generator 0.30.0).
        i("deep_stir_permille", 0, 1000),
    };
    std::vector<Check> checks{
        Check{"monsters_bold_ordered",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("bold_off_permille") > r.get_int("bold_on_permille")) {
                      return "bold_off_permille " + int_text(r.get_int("bold_off_permille")) +
                             " is above bold_on_permille " + int_text(r.get_int("bold_on_permille"));
                  }
                  return std::nullopt;
              }},
        Check{"monsters_deep_years_ordered",  // P5-07 4.2
              [](const Record& r) -> std::optional<std::string> {
                  const std::vector<Value>& y = r.get_array("deep_years");
                  for (std::size_t j = 1; j < y.size(); ++j) {
                      if (y[j].as_int() <= y[j - 1].as_int()) {
                          return "deep_years[" + uint_text(j) + "] " + int_text(y[j].as_int()) +
                                 " is not above deep_years[" + uint_text(j - 1) + "] " + int_text(y[j - 1].as_int());
                      }
                  }
                  return std::nullopt;
              }},
        Check{"monsters_hunt_ordered",  // P5-03 9.4
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("hunt_permille") > r.get_int("hunt_max_permille")) {
                      return "hunt_permille " + int_text(r.get_int("hunt_permille")) + " is above hunt_max_permille " +
                             int_text(r.get_int("hunt_max_permille"));
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

// P5-04 4.2 (generator 0.26.0): the tuning id history.alliances, in the
// table's order, with its five checks.
Schema history_alliances_schema() {
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    std::vector<Field> fields{
        i("ally_hops", -1, 8),
        i("ally_min_disposition", -2000, 2000),
        i("ally_full_disposition", -2000, 2000),
        i("ally_friend_disposition", -2000, 2000),
        i("ally_break_disposition", -2000, 2000),
        i("allies_max", 0, 16),
        i("ally_after_war_years", 0, 100000),
        i("foe_years", 1, 100000),
        i("fear_share", 0, 1000),
        i("form_foe_permille", 0, 1000),
        i("form_fear_permille", 0, 1000),
        i("form_trade_permille", 0, 1000),
        i("form_friend_permille", 0, 1000),
        i("form_max_permille", 0, 1000),
        i("bloc_share_max", 0, 1000),
        i("bloc_lapse_share", 0, 1001),
        i("term_years", 1, 100000),
        i("refusals_to_break", 1, 100),
        i("defend_permille", 0, 1000),
        i("attack_permille", 0, 1000),
        i("hostility_full", 1, 2000),
        i("ally_wars_max", 1, 16),
        i("ally_memory", 0, 1000),
        i("renew_memory", 0, 1000),
        i("join_memory", 0, 1000),
        i("refuse_memory", 0, 1000),
        i("betray_memory", 0, 1000),
        i("ally_war_memory", 0, 1000),
        i("trade_hops", -1, 8),
        i("trade_min_disposition", -2000, 2000),
        i("trade_break_margin", 0, 2000),
        i("trade_after_war_years", 0, 100000),
        i("trade_permille", 0, 1000),
        i("trade_max", 0, 16),
        i("trade_credit_years", 1, 100000),
        i("trade_memory", 0, 1000),
        i("trade_ally_years", 0, 100000),
        i("spoils_plunder_permille", 0, 1000),
        i("spoils_floor_permille", 0, 1000),
        i("spoils_cap_permille", 0, 1000),
        i("spoils_grant_permille", 0, 1000),
        i("spoils_warmth_permille", 0, 1000),
        i("spoils_part_permille", 0, 1000),
        i("spoils_need_permille", 0, 1000),
        i("spoils_grant_min", 0, 1000),
        i("spoils_grant_max", 0, 1000),
        i("share_memory", 0, 1000),
        i("withheld_memory", 0, 1000),
        i("withheld_grudge", 0, 2000),
        i("withheld_to_break", 1, 100),
        i("mine_permille", 0, 1000),
        i("mine_ore_min", 1, 100000),
        i("mine_settlement_min", 0, 1000000),
        i("arms_trade_permille", 0, 4000),
        Field::required("mine_forms", Type::array(Type::string(64), 1, 4)),
        // P5-04a 14.1 (generator 0.28.0), appended in this order.
        i("spoils_large_share", 1, 1000000),
        i("spoils_large_part_permille", 0, 1001),
        i("soured_renew_permille", 0, 1000),
        i("soured_join_permille", 0, 1000),
        i("weary_years", 0, 100000),
        i("join_weary_permille", 0, 1000),
        i("join_busy_permille", 0, 1000),
        i("join_fear_floor", 0, 1000),
        i("join_warmth_floor", 0, 1000),
        i("grasping_join_permille", 0, 1000),
        i("join_claim_permille", 0, 1000),
        i("excused_memory", 0, 1000),
        i("refuse_grudge", 0, 2000),
        i("betray_cooled_disposition", -2000, 2000),
        i("betray_weak_permille", 0, 1000),
        // P5-08 12.5 (generator 0.30.0).
        i("renew_written_max", 0, 100),
    };
    const auto above = [](const Record& r, std::string_view a, std::string_view b,
                          std::string_view words) -> std::optional<std::string> {
        return std::string(a) + " " + int_text(r.get_int(a)) + std::string(words) + std::string(b) + " " +
               int_text(r.get_int(b));
    };
    std::vector<Check> checks{
        Check{"alliances_spoils",
              [above](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("spoils_floor_permille") > r.get_int("spoils_cap_permille")) {
                      return above(r, "spoils_floor_permille", "spoils_cap_permille", " is above ");
                  }
                  if (r.get_int("spoils_grant_min") > r.get_int("spoils_grant_max")) {
                      return above(r, "spoils_grant_min", "spoils_grant_max", " is above ");
                  }
                  return std::nullopt;
              }},
        Check{"alliances_mine_forms",
              [](const Record& r) -> std::optional<std::string> {
                  const std::vector<Value>& forms = r.get_array("mine_forms");
                  for (std::size_t k = 0; k < forms.size(); ++k) {
                      const std::string& f = forms[k].as_string();
                      constexpr std::string_view SLOT = "{settlement}";
                      std::size_t slots = 0;
                      std::size_t braces = 0;
                      for (std::size_t at = f.find(SLOT); at != std::string::npos; at = f.find(SLOT, at + 1)) {
                          ++slots;
                      }
                      for (const char ch : f) {
                          braces += (ch == '{' || ch == '}') ? 1 : 0;
                      }
                      if (slots != 1 || braces != 2) {
                          return "mine_forms[" + uint_text(k) + "] '" + f + "' must contain {settlement} once";
                      }
                  }
                  return std::nullopt;
              }},
        Check{"alliances_dispositions",
              [above](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("ally_min_disposition") >= r.get_int("ally_full_disposition")) {
                      return above(r, "ally_min_disposition", "ally_full_disposition", " is not below ");
                  }
                  if (r.get_int("ally_break_disposition") >= r.get_int("ally_min_disposition")) {
                      return above(r, "ally_break_disposition", "ally_min_disposition", " is not below ");
                  }
                  return std::nullopt;
              }},
        Check{"alliances_blocs",
              [above](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("bloc_share_max") >= r.get_int("bloc_lapse_share")) {
                      return above(r, "bloc_share_max", "bloc_lapse_share", " is not below ");
                  }
                  return std::nullopt;
              }},
        Check{"alliances_trade",
              [above](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("trade_min_disposition") >= r.get_int("ally_full_disposition")) {
                      return above(r, "trade_min_disposition", "ally_full_disposition", " is not below ");
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

// P5-06 4.4 (generator 0.27.0): the tuning id history.figures, in the
// table's order, with its three checks.
Schema history_figures_schema() {
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    const Schema epithet(std::vector<Field>{
        Field::required("role", Type::enumeration({"ruler", "claimant", "general", "hero", "caster"})),
        Field::required("deed", Type::enumeration({"founded_years", "wars_won", "conquests", "betrayals", "realm_fell",
                                                   "reign_permille", "battles_won", "battles_unbeaten", "slew_might",
                                                   "exiled", "books", "blights", "corrupted"})),
        i("min", 1, 100000),
        i("substantial", 0, 100000),
        Field::required("text", Type::string(24, epithet_text_grammar())),
    });
    std::vector<Field> fields{
        Field::required("name_rulers", Type::boolean()),
        i("reign_min_permille", 1, 4000),
        i("reign_span_permille", 0, 4000),
        i("accession_age_permille", 0, 1000),
        i("accession_span_permille", 0, 1000),
        Field::required("temper_strain_permille", Type::record(temper_schema(0, 4000))),
        Field::required("temper_allied_permille", Type::record(temper_schema(0, 4000))),
        Field::required("temper_spoils_permille", Type::record(temper_schema(-1000, 1000))),
        i("short_reign_permille", 0, 4000),
        i("short_reign_split_permille", 0, 1000),
        i("exile_permille", 0, 1000),
        i("exile_hops", 0, 8),
        i("general_scale_permille", 0, 1000),
        i("skill_min", 0, 1000),
        i("skill_max", 0, 1000),
        i("career_permille", 0, 1000),
        i("fall_loser_permille", 0, 1000),
        i("fall_winner_permille", 0, 1000),
        i("general_age_permille", 0, 1000),
        i("general_age_span_permille", 0, 1000),
        i("hero_age_permille", 0, 1000),
        i("hero_age_span_permille", 0, 1000),
        i("life_min_permille", 0, 4000),
        i("life_span_permille", 0, 4000),
        i("hero_scale_permille", 0, 1000),
        i("hero_hunt_permille", 0, 1000),
        i("hero_might_permille", 0, 4000),
        i("hero_fall_permille", 0, 1000),
        i("caster_ppm", 0, 1000000),
        i("caster_age_permille", 0, 1000),
        i("caster_age_span_permille", 0, 1000),
        i("book_permille", 0, 1000),
        i("books_max", 0, 16),
        i("blight_ppm", 0, 1000000),
        i("blight_toll_permille", 0, 1000),
        i("regnal_max", 2, 39),
        Field::required("epithets", Type::array(Type::record(epithet), 0, 32)),
        i("epithet_repeat_max", 1, 100),
        // P5-04a 14.2a (generator 0.28.0), appended in this order.
        i("tower_spacing_permille", 0, 1000),
        i("hero_return_years", 1, 100000),
        // P5-08 12.6 (generator 0.30.0).
        i("tower_lapse_years", 0, 10000),
        // P5-11 4.3 (generator 0.31.0): a smith's age at entry.
        i("smith_age_permille", 0, 1000),
        i("smith_age_span_permille", 0, 1000),
    };
    std::vector<Check> checks{
        Check{"figures_skill_ordered",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("skill_min") > r.get_int("skill_max")) {
                      return "skill_min " + int_text(r.get_int("skill_min")) + " is above skill_max " +
                             int_text(r.get_int("skill_max"));
                  }
                  return std::nullopt;
              }},
        Check{"figures_epithet_slayer",
              [](const Record& r) -> std::optional<std::string> {
                  const std::vector<Value>& list = r.get_array("epithets");
                  for (std::size_t k = 0; k < list.size(); ++k) {
                      const Record& e = list[k].as_record();
                      if (e.get_string("text") == "{slayer}" && e.get_string("deed") != "slew_might") {
                          return "epithets[" + uint_text(k) + "]: {slayer} needs the deed slew_might";
                      }
                  }
                  return std::nullopt;
              }},
        Check{"figures_epithet_thresholds",
              [](const Record& r) -> std::optional<std::string> {
                  const std::vector<Value>& list = r.get_array("epithets");
                  for (std::size_t k = 0; k < list.size(); ++k) {
                      const Record& e = list[k].as_record();
                      const std::int64_t sub = e.get_int("substantial");
                      if (sub != 0 && sub < e.get_int("min")) {
                          return "epithets[" + uint_text(k) + "]: substantial " + int_text(sub) + " is below min " +
                                 int_text(e.get_int("min"));
                      }
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

// --- god and history.pantheon (P5-08 4.1 to 4.4) ------------------------------------

namespace {

constexpr std::string_view GOD_PLACEHOLDER = "{god}";
constexpr std::string_view PLURAL_PLACEHOLDER = "{plural}";

// 4.1 god_title: "the " followed by ASCII letters, single spaces, ' and -;
// with `plural`, the slot {plural} exactly once among them (4.2, the
// chronology's creator_title).
bool god_title_ok(std::string_view text, bool plural) noexcept {
    constexpr std::string_view THE = "the ";
    if (text.size() <= THE.size() || text.substr(0, THE.size()) != THE || text.back() == ' ') {
        return false;
    }
    std::size_t slots = 0;
    std::size_t i = THE.size();
    if (text[i] == ' ') {
        return false;
    }
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{') {
            if (!plural || text.substr(i, PLURAL_PLACEHOLDER.size()) != PLURAL_PLACEHOLDER) {
                return false;
            }
            ++slots;
            i += PLURAL_PLACEHOLDER.size();
            continue;
        }
        if (c == ' ') {
            if (i + 1 < text.size() && text[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && !is_separator(c)) {
            return false;
        }
        ++i;
    }
    return plural ? slots == 1 : true;
}

bool is_valid_god_title(std::string_view text) noexcept {
    return god_title_ok(text, false);
}

bool is_valid_creator_title(std::string_view text) noexcept {
    return god_title_ok(text, true);
}

// 4.4 god_form: name_form's grammar with the slot {god} exactly once in
// place of {root}.
bool is_valid_god_form(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    std::size_t slots = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{') {
            if (text.substr(i, GOD_PLACEHOLDER.size()) != GOD_PLACEHOLDER) {
                return false;
            }
            ++slots;
            i += GOD_PLACEHOLDER.size();
            continue;
        }
        if (c == ' ') {
            if (i + 1 < text.size() && text[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && !is_separator(c)) {
            return false;
        }
        ++i;
    }
    return slots == 1;
}

StringGrammar god_title_grammar() noexcept {
    return StringGrammar{"a god title ('the ' then ASCII letters, single spaces, ' and -)", &is_valid_god_title};
}

StringGrammar creator_title_grammar() noexcept {
    return StringGrammar{"a creator title ('the ' then ASCII letters, single spaces, ' and -, with {plural} once)",
                         &is_valid_creator_title};
}

StringGrammar god_form_grammar() noexcept {
    return StringGrammar{"a god form (letters, spaces, ' and -, with exactly one {god})", &is_valid_god_form};
}

Schema weights_schema(std::initializer_list<std::string_view> names) {
    std::vector<Field> fields;
    for (const std::string_view n : names) {
        fields.push_back(Field::required(std::string(n), Type::integer(0, 1000)));
    }
    return Schema(std::move(fields));
}

// 4.3: a deed.
Schema deed_schema() {
    return Schema(std::vector<Field>{
        Field::required("kind", Type::string(32)),
        Field::required("role", Type::string(32)),
        Field::required("points", Type::integer(-1000, 1000)),
        Field::required("who", Type::enumeration({"kin", "any", "foe"})),
        Field::required("against", Type::string(32)),
    });
}

// The fields of 4.1 from favour on, shared by a god definition and the
// pantheon tuning's creator record.
std::vector<Field> god_act_fields() {
    return std::vector<Field>{
        Field::required("favour", Type::record(weights_schema({"bless", "champion", "mark"}))),
        Field::required("wrath", Type::record(weights_schema({"curse", "monster", "mark"}))),
        Field::required("bless", Type::enumeration({"plenty", "valour"})),
        Field::required("curse", Type::enumeration({"pestilence", "withering", "defeat"})),
        Field::required("monsters", Type::array(Type::string(32), 0, 8)),
        Field::required("deeds", Type::array(Type::record(deed_schema()), 0, 32)),
    };
}

std::optional<std::string> check_weights(const Record& r, std::string_view field,
                                         std::initializer_list<std::string_view> names, std::string_view message) {
    const Record& w = r.get_record(field);
    for (const std::string_view n : names) {
        if (w.get_int(n) != 0) {
            return std::nullopt;
        }
    }
    return std::string(message);
}

bool civ_role_of(const history::KindDef& kind, std::string_view role) {
    const history::RoleDef* d = kind.find_role(role);
    return d != nullptr && d->type == history::EntityType::civilization;
}

// 4.3 pantheon_deed_*: the first failing deed of the check.
enum class DeedCheck : std::uint8_t { kind, role, against, points, foe };

std::optional<std::string> check_deeds(const Record& r, DeedCheck which) {
    const std::vector<Value>& list = r.get_array("deeds");
    const history::EventCatalogue& catalogue = history::compiled_event_catalogue();
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Record& d = list[i].as_record();
        const std::string at = "deeds[" + uint_text(i) + "]: ";
        const std::string& k = d.get_string("kind");
        const history::KindDef* kind = catalogue.find(k);
        const std::string& role = d.get_string("role");
        const std::string& against = d.get_string("against");
        switch (which) {
            case DeedCheck::kind:
                if (kind == nullptr) {
                    return at + "kind " + k + " is unknown";
                }
                break;
            case DeedCheck::role:
                if (kind != nullptr && !civ_role_of(*kind, role)) {
                    return at + "role " + role + " of " + k + " is not a civilization role";
                }
                if (kind != nullptr && !against.empty() && !civ_role_of(*kind, against)) {
                    return at + "role " + against + " of " + k + " is not a civilization role";
                }
                break;
            case DeedCheck::against:
                if (!against.empty() && against == role) {
                    return at + "against " + against + " is the deed's own role";
                }
                break;
            case DeedCheck::points:
                if (d.get_int("points") == 0) {
                    return at + "points must not be 0";
                }
                break;
            case DeedCheck::foe:
                if (d.get_string("who") == "foe" && d.get_int("points") >= 0) {
                    return at + "a deed for foes must be negative";
                }
                break;
        }
    }
    return std::nullopt;
}

std::vector<Check> god_act_checks() {
    return std::vector<Check>{
        Check{"god_favour_weights",
              [](const Record& r) {
                  return check_weights(r, "favour", {"bless", "champion", "mark"}, "favour weights must not all be 0");
              }},
        Check{"god_wrath_weights",
              [](const Record& r) {
                  return check_weights(r, "wrath", {"curse", "monster", "mark"}, "wrath weights must not all be 0");
              }},
        Check{"god_monster_weight",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_record("wrath").get_int("monster") > 0 && r.get_array("monsters").empty()) {
                      return std::string("wrath.monster needs monsters");
                  }
                  return std::nullopt;
              }},
        Check{"pantheon_deed_kind", [](const Record& r) { return check_deeds(r, DeedCheck::kind); }},
        Check{"pantheon_deed_role", [](const Record& r) { return check_deeds(r, DeedCheck::role); }},
        Check{"pantheon_deed_against", [](const Record& r) { return check_deeds(r, DeedCheck::against); }},
        Check{"pantheon_deed_points", [](const Record& r) { return check_deeds(r, DeedCheck::points); }},
        Check{"pantheon_deed_foe", [](const Record& r) { return check_deeds(r, DeedCheck::foe); }},
    };
}

}  // namespace

StringGrammar god_form_string_grammar() noexcept {
    return god_form_grammar();
}

StringGrammar creator_title_string_grammar() noexcept {
    return creator_title_grammar();
}

Schema god_schema() {
    std::vector<Field> fields{
        Field::required("name_culture", Type::string(32)),
        Field::required("camp", Type::enumeration({"good", "neutral", "evil"})),
        Field::required("domain", Type::enumeration({"war", "death", "harvest", "craft", "wild", "oaths", "lore", "ruin"})),
        Field::required("title", Type::string(40, god_title_grammar())),
    };
    for (Field& f : god_act_fields()) {
        fields.push_back(std::move(f));
    }
    return Schema(std::move(fields), god_act_checks());
}

Schema history_pantheon_schema() {
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    const auto rec = [](std::initializer_list<std::string_view> names, std::int64_t min, std::int64_t max) {
        std::vector<Field> f;
        for (const std::string_view n : names) {
            f.push_back(Field::required(std::string(n), Type::integer(min, max)));
        }
        return Type::record(Schema(std::move(f)));
    };
    std::vector<Field> fields{
        Field::required("activity_permille", rec({"distant", "normal", "meddling"}, 0, 10000)),
        i("regard_max", 1, 100000),
        i("regard_fade_per_century", 0, 100000),
        i("regard_memory_years", 1, 100000),
        i("contributions_max", 1, 64),
        i("act_threshold", 1, 100000),
        i("act_permille", 0, 1000),
        i("god_rest_years", 0, 10000),
        i("pair_rest_years", 0, 10000),
        i("flare_regard", 1, 100000),
        i("flare_permille", 0, 1000),
        i("bless_years", 1, 1000),
        i("curse_years", 1, 1000),
        i("plenty_growth_permille", 0, 1000),
        i("withering_growth_permille", 0, 1000),
        i("valour_permille", 0, 500),
        i("defeat_permille", 0, 500),
        i("pestilence_toll_permille", 0, 1000),
        i("send_hops", 0, 8),
        i("sacred_growth_permille", 0, 1000),
        i("profane_years", 0, 10000),
        Field::required("champion_epithet", Type::string(24, god_form_grammar())),
        i("cult_found_permille", 0, 1000),
        i("cult_fear_permille", 0, 1000),
        i("cult_spread_permille", 0, 1000),
        Field::required("affinity_permille", rec({"same", "near", "opposed"}, 0, 1000)),
        i("adopt_regard", 0, 100000),
        i("adopt_permille", 0, 1000),
        i("suppress_permille", 0, 1000),
        Field::required("temper_adopt_permille", Type::record(temper_schema(0, 4000))),
        Field::required("temper_suppress_permille", Type::record(temper_schema(0, 4000))),
        i("faith_points", 0, 100000),
        i("rival_faith_points", 0, 100000),
        i("suppress_points", 0, 100000),
        i("hide_permille", 0, 1000),
        i("sanctum_hops", 0, 8),
        Field::required("creator", Type::record(Schema(god_act_fields(), god_act_checks()))),
        i("cult_return_years", 0, 10000),  // P5-11 12.2 (generator 0.31.0)
    };
    std::vector<Check> checks{
        Check{"pantheon_flare_above_act",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("flare_regard") < r.get_int("act_threshold")) {
                      return "flare_regard " + int_text(r.get_int("flare_regard")) + " is below act_threshold " +
                             int_text(r.get_int("act_threshold"));
                  }
                  return std::nullopt;
              }},
        Check{"pantheon_act_below_max",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("act_threshold") > r.get_int("regard_max")) {
                      return "act_threshold " + int_text(r.get_int("act_threshold")) + " is above regard_max " +
                             int_text(r.get_int("regard_max"));
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

// --- artifact and history.artifacts (P5-11 4.1 to 4.4) --------------------------------

namespace {

constexpr std::string_view TITLE_PLACEHOLDER = "{title}";

// 4.2 artifact_form: name_form's grammar with {root} exactly once and
// {title} at most once.
bool is_valid_artifact_form(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    std::size_t roots = 0;
    std::size_t titles = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{') {
            if (text.substr(i, ROOT_PLACEHOLDER.size()) == ROOT_PLACEHOLDER) {
                ++roots;
                i += ROOT_PLACEHOLDER.size();
                continue;
            }
            if (text.substr(i, TITLE_PLACEHOLDER.size()) == TITLE_PLACEHOLDER) {
                ++titles;
                i += TITLE_PLACEHOLDER.size();
                continue;
            }
            return false;
        }
        if (c == ' ') {
            if (i + 1 < text.size() && text[i + 1] == ' ') {
                return false;
            }
        } else if (!is_ascii_letter(c) && !is_separator(c)) {
            return false;
        }
        ++i;
    }
    return roots == 1 && titles <= 1;
}

// 4.1 artifact_word: ASCII letters and -, starting with an upper-case letter.
bool is_valid_artifact_word(std::string_view text) noexcept {
    if (text.empty() || !(text.front() >= 'A' && text.front() <= 'Z')) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char c) { return is_ascii_letter(c) || c == '-'; });
}

// 4.1 artifact_noun: lower-case ASCII letters and single spaces.
bool is_valid_artifact_noun(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ') {
            if (text[i + 1] == ' ') {
                return false;
            }
        } else if (!(c >= 'a' && c <= 'z')) {
            return false;
        }
    }
    return true;
}

StringGrammar artifact_word_grammar() noexcept {
    return StringGrammar{"an artifact word (ASCII letters and -, starting with an upper-case letter)",
                         &is_valid_artifact_word};
}

StringGrammar artifact_noun_grammar() noexcept {
    return StringGrammar{"an artifact noun (lower-case letters and single spaces)", &is_valid_artifact_noun};
}

constexpr std::array<std::string_view, 5> ARTIFACT_CLASSES = {"arms", "armour", "regalia", "relic", "staff"};

// 4.4 artifacts_occasion_*: the first failing occasion of the check.
enum class OccasionCheck : std::uint8_t { kind, role, bearer, classes, god_camp };

std::optional<std::string> check_occasions(const Record& r, OccasionCheck which) {
    const std::vector<Value>& list = r.get_array("occasions");
    const history::EventCatalogue& catalogue = history::compiled_event_catalogue();
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Record& o = list[i].as_record();
        const std::string at = "occasions[" + uint_text(i) + "]: ";
        const std::string& k = o.get_string("kind");
        const history::KindDef* kind = catalogue.find(k);
        const std::string& role = o.get_string("role");
        const std::string& bearer = o.get_string("bearer");
        switch (which) {
            case OccasionCheck::kind:
                if (kind == nullptr) {
                    return at + "kind " + k + " is unknown";
                }
                break;
            case OccasionCheck::role:
                if (kind != nullptr && !civ_role_of(*kind, role)) {
                    return at + "role " + role + " of " + k + " is not a civilization role";
                }
                break;
            case OccasionCheck::bearer: {
                if (bearer == "general" && k != "war_declared") {
                    return at + "bearer general needs war_declared";
                }
                const bool figure = bearer == "figure";
                if (kind != nullptr && (figure || bearer == "site")) {
                    const history::EntityType want = figure ? history::EntityType::figure : history::EntityType::site;
                    const bool has = std::any_of(kind->roles.begin(), kind->roles.end(),
                                                 [want](const history::RoleDef& d) { return d.type == want; });
                    if (!has) {
                        return at + "bearer " + bearer + " needs a " + (figure ? "figure" : "site") + " role in " + k;
                    }
                }
                break;
            }
            case OccasionCheck::god_camp: {  // r4 4.4a
                const std::string& camp = o.get_string("god_camp");
                if (camp != "any" && kind != nullptr) {
                    const bool has = std::any_of(kind->roles.begin(), kind->roles.end(), [](const history::RoleDef& d) {
                        return d.type == history::EntityType::god;
                    });
                    if (!has) {
                        return at + "god_camp " + camp + " needs a god role in " + k;
                    }
                }
                break;
            }
            case OccasionCheck::classes: {
                const Record& w = o.get_record("classes");
                bool any = false;
                for (const std::string_view c : ARTIFACT_CLASSES) {
                    any = any || w.get_int(c) != 0;
                }
                if (!any) {
                    return at + "class weights must not all be 0";
                }
                break;
            }
        }
    }
    return std::nullopt;
}

}  // namespace

StringGrammar artifact_form_string_grammar() noexcept {
    return StringGrammar{"an artifact form (letters, spaces, ' and -, with exactly one {root} and at most one {title})",
                         &is_valid_artifact_form};
}

Schema artifact_schema() {
    std::vector<Field> fields{
        Field::required("class", Type::enumeration({"arms", "armour", "regalia", "relic", "staff"})),
        Field::required("title", Type::string(16, artifact_word_grammar())),
        Field::required("noun", Type::string(16, artifact_noun_grammar())),
        Field::required("weight", Type::integer(1, 1000)),
        Field::required("battle_permille", Type::integer(0, 500)),
        Field::required("hunt_permille", Type::integer(0, 1000)),
        Field::required("claim_permille", Type::integer(0, 1000)),
    };
    std::vector<Check> checks{
        Check{"artifact_battle_class",
              [](const Record& r) -> std::optional<std::string> {
                  const std::string& c = r.get_string("class");
                  if (r.get_int("battle_permille") > 0 && c != "arms" && c != "armour") {
                      return std::string("battle_permille needs arms or armour");
                  }
                  return std::nullopt;
              }},
        Check{"artifact_hunt_class",
              [](const Record& r) -> std::optional<std::string> {
                  const std::string& c = r.get_string("class");
                  if (r.get_int("hunt_permille") > 0 && (c == "regalia" || c == "relic")) {
                      return std::string("hunt_permille must be 0 for regalia and relics");
                  }
                  return std::nullopt;
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_artifacts_schema() {
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    std::vector<Field> classes;
    for (const std::string_view c : ARTIFACT_CLASSES) {
        classes.push_back(Field::required(std::string(c), Type::integer(0, 1000)));
    }
    const Schema occasion(std::vector<Field>{
        Field::required("kind", Type::string(32)),
        Field::required("role", Type::string(32)),
        Field::required("chance_permille", Type::integer(0, 1000)),
        Field::required("bearer", Type::enumeration({"realm", "general", "figure", "site"})),
        Field::required("classes", Type::record(Schema(std::move(classes)))),
        // r4 4.4a: the dark gift's camp filter, absent = any.
        Field::optional("god_camp", Type::enumeration({"any", "good", "neutral", "evil"}), Value::string("any")),
    });
    std::vector<Field> fields{
        Field::required("occasions", Type::array(Type::record(occasion), 0, 32)),
        i("forge_chance_permille", 0, 4000),
        i("realm_rest_years", 0, 10000),
        i("world_max", 0, 1000),
        i("deep_metal_depth", 1, 3),
        i("deep_metal_permille", 0, 500),
        i("force_max_permille", 0, 1000),
        i("hunt_max_permille", 0, 2000),
        i("claim_max_permille", 0, 4000),
        i("burial_permille", 0, 1000),
        i("battle_take_permille", 0, 1000),
        i("sack_take_permille", 0, 1000),
        i("capture_take_permille", 0, 1000),
        i("raid_take_permille", 0, 1000),
        i("hoard_hunt_permille", 0, 1000),
        // P5-11 r2 4.3: corruption by an evil artifact (6.11).
        i("corrupt_years", 1, 10000),
        i("corrupt_permille", 0, 1000),
        Field::required("corrupt_camp_permille",
                        Type::record(Schema(std::vector<Field>{i("good", 0, 1000), i("neutral", 0, 1000),
                                                               i("evil", 0, 1000)}))),
        i("corrupt_warlike_permille", 0, 1000),
        // P5-11 r3 4.3: corruption of a bearer (6.12).
        i("corrupt_bearer_years", 1, 10000),
        i("corrupt_bearer_permille", 0, 1000),
        i("corrupt_sack_permille", 0, 1000),
    };
    std::vector<Check> checks{
        Check{"artifacts_occasion_kind", [](const Record& r) { return check_occasions(r, OccasionCheck::kind); }},
        Check{"artifacts_occasion_role", [](const Record& r) { return check_occasions(r, OccasionCheck::role); }},
        Check{"artifacts_occasion_bearer", [](const Record& r) { return check_occasions(r, OccasionCheck::bearer); }},
        Check{"artifacts_occasion_classes",
              [](const Record& r) { return check_occasions(r, OccasionCheck::classes); }},
        Check{"artifacts_occasion_god_camp",
              [](const Record& r) { return check_occasions(r, OccasionCheck::god_camp); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema history_relations_schema() {
    // P5-03 4.2, in the table's order.
    const auto i = [](std::string_view name, std::int64_t min, std::int64_t max) {
        return Field::required(std::string(name), Type::integer(min, max));
    };
    std::vector<Field> fields{
        i("hatred_max", -2000, 0),
        i("friend_min", 0, 2000),
        i("band_margin", 0, 500),
        i("memory_min", -2000, 0),
        i("memory_max", 0, 2000),
        i("memory_decay_per_century", 0, 10000),
        i("war_memory", 0, 1000),
        i("conquest_memory", 0, 1000),
        i("sack_memory", 0, 1000),
        i("capture_memory", 0, 1000),
        i("expel_memory", 0, 1000),
        i("shelter_memory", 0, 1000),
        i("meet_hops", -1, 8),
        i("migrate_hops", 0, 8),
        i("migrate_min", 1, 100000),
        i("welcome_min", -2000, 2001),
        i("subject_years", 0, 100000),
        i("conquest_settlers_permille", 0, 1000),
        i("drift_permille_per_century", 0, 1000),
        i("fade_permille", 0, 1000),
        i("mixed_min_permille", 1, 1001),
        i("expel_permille", 0, 1000),
        i("expel_share_max", 0, 1000),
        // P5-03a 10.1 (generator 0.25.0), appended in this order.
        i("peace_years", 0, 100000),
        i("peace_memory_per_century", 0, 10000),
        i("peace_memory_max", 0, 2000),
        i("contact_hops", 0, 8),
        i("foe_years", 1, 100000),
        i("foe_war_memory", 0, 1000),
        i("foe_monster_memory", 0, 1000),
        i("meet_sealed_hops", -1, 8),
        i("meet_sealed_years", 0, 100000),
        i("conquest_settlers_span_permille", 0, 1000),
        i("expel_gap_years", 0, 100000),
        // P5-04a 14.2 (generator 0.28.0), appended in this order.
        i("turn_cooldown_years", 0, 100000),
        i("expel_settled_years", 0, 100000),
        i("expel_fresh_years", 0, 100000),
    };
    std::vector<Check> checks{
        Check{"relations_bands",
              [](const Record& r) -> std::optional<std::string> {
                  const std::int64_t h = r.get_int("hatred_max");
                  const std::int64_t f = r.get_int("friend_min");
                  const std::int64_t m = r.get_int("band_margin");
                  if (h + m < f - m) {
                      return std::nullopt;
                  }
                  return "bands overlap: hatred_max " + int_text(h) + " and friend_min " + int_text(f) +
                         " with band_margin " + int_text(m);
              }},
        Check{"relations_memory",
              [](const Record& r) -> std::optional<std::string> {
                  if (r.get_int("memory_min") <= 0 && r.get_int("memory_max") >= 0) {
                      return std::nullopt;
                  }
                  return std::string("memory_min must not be above 0 nor memory_max below 0");
              }},
        // P5-03a 10.1.
        Check{"relations_peace_cap",
              [](const Record& r) -> std::optional<std::string> {
                  const std::int64_t a = r.get_int("peace_memory_max");
                  const std::int64_t b = r.get_int("memory_max");
                  if (a <= b) {
                      return std::nullopt;
                  }
                  return "peace_memory_max " + int_text(a) + " is above memory_max " + int_text(b);
              }},
        Check{"relations_settlers",
              [](const Record& r) -> std::optional<std::string> {
                  const std::int64_t a = r.get_int("conquest_settlers_permille");
                  const std::int64_t b = r.get_int("conquest_settlers_span_permille");
                  if (a + b <= 1000) {
                      return std::nullopt;
                  }
                  return "conquest settlers " + int_text(a) + " + " + int_text(b) + " exceed 1000";
              }},
    };
    return Schema(std::move(fields), std::move(checks));
}

}  // namespace worldgen::data
