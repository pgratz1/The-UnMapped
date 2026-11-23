// The kinds of the history phase (spec P3-01 sections 3 to 5): `race`,
// `culture` with its naming record (P3-02 section 3), and the fixed-id
// tuning kind `history` (id `chronology`). Registered by builtin_kinds()
// in kinds.cpp.

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
        // 22.3 make the templates of kinds 16 and 31 longer).
        Field::required("template", Type::string(512)),
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
        i("ruin_fade_years", 1, 100000),
        i("colony_new_region_salience", 0, 100),
        i("plague_small_salience", 0, 100),
        i("plague_major_dead", 0, 1000000),   // r6 (generator 0.17.0)
        i("crisis_small_salience", 0, 100),   // r6 (generator 0.17.0)
    };
    std::vector<Check> checks{
        Check{"peoples_ladder", [](const Record& r) { return check_peoples_ladder(r); }},
        Check{"peoples_tolls", [](const Record& r) { return check_peoples_tolls(r); }},
        Check{"peoples_floor", [](const Record& r) { return check_peoples_floor(r); }},
        Check{"peoples_colonists", [](const Record& r) { return check_peoples_colonists(r); }},
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
    };
    std::vector<Check> checks{
        Check{"race_generation_below_lifespan", [](const Record& r) { return check_race_generation_below_lifespan(r); }},
        Check{"race_site_weights_positive", [](const Record& r) { return check_race_site_weights_positive(r); }},
        Check{"race_biomes_unique", [](const Record& r) { return check_race_biomes_unique(r); }},
        Check{"race_settlement_kinds_unique", [](const Record& r) { return check_race_settlement_kinds_unique(r); }},
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
    };
    std::vector<Check> checks{
        Check{"chronology_age_name_form", [](const Record& r) { return check_chronology_age_name_form(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

}  // namespace worldgen::data
