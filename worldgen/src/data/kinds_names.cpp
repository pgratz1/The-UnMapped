// The name data kinds of P3-08 (generator 0.17.0): `placename`, the words of
// drawn region names (spec P1-06 r3 section 4.1), and `name_filter`, the
// shared filter of generated history names (spec P3-02 r5 section 11.2).
// Registered by builtin_kinds() in kinds.cpp.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/text.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

namespace worldgen::data {

namespace {

using detail::uint_text;

constexpr bool is_upper(char c) noexcept {
    return c >= 'A' && c <= 'Z';
}

constexpr bool is_letter(char c) noexcept {
    return detail::is_lower(c) || is_upper(c);
}

// --- Grammars of P1-06 4.1 ---------------------------------------------------------

// One or more ASCII lower-case letters [a-z]+.
bool is_valid_element(std::string_view text) noexcept {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (!detail::is_lower(c)) {
            return false;
        }
    }
    return true;
}

// A word, optionally preceded by "the" and one space.
bool is_valid_name_object(std::string_view text) noexcept {
    constexpr std::string_view THE = "the ";
    if (text.substr(0, THE.size()) == THE) {
        text.remove_prefix(THE.size());
    }
    return is_valid_word(text);
}

// No leading, trailing or double space.
bool spaces_ok(std::string_view text) noexcept {
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return false;
    }
    for (std::size_t i = 1; i < text.size(); ++i) {
        if (text[i] == ' ' && text[i - 1] == ' ') {
            return false;
        }
    }
    return true;
}

// ASCII letters, spaces and '; starts with a letter; no leading, trailing or
// double space.
bool is_valid_place_name(std::string_view text) noexcept {
    if (text.empty() || !is_letter(text.front()) || !spaces_ok(text)) {
        return false;
    }
    for (const char c : text) {
        if (!is_letter(c) && c != ' ' && c != '\'') {
            return false;
        }
    }
    return true;
}

// The eight slots of a place form, in the order of the lists (RG11 6).
constexpr std::array<std::string_view, 8> PLACE_SLOTS = {"{noun}",   "{suffix}",    "{prefix}", "{adjective}",
                                                         "{feature}", "{possessor}", "{place}",  "{object}"};

// Literal characters (ASCII letters, space, ') and slots; at least one
// slot, none twice; no other { or }; no leading, trailing or double space.
bool is_valid_place_form(std::string_view text) noexcept {
    if (!spaces_ok(text)) {
        return false;
    }
    std::array<bool, PLACE_SLOTS.size()> seen{};
    bool any = false;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{') {
            bool matched = false;
            for (std::size_t s = 0; s < PLACE_SLOTS.size(); ++s) {
                if (text.substr(i, PLACE_SLOTS[s].size()) == PLACE_SLOTS[s]) {
                    if (seen[s]) {
                        return false;
                    }
                    seen[s] = true;
                    any = true;
                    matched = true;
                    i += PLACE_SLOTS[s].size();
                    break;
                }
            }
            if (!matched) {
                return false;
            }
            continue;
        }
        if (!is_letter(c) && c != ' ' && c != '\'') {
            return false;
        }
        ++i;
    }
    return any;
}

StringGrammar element_grammar() noexcept {
    return StringGrammar{"lower-case letters a-z", &is_valid_element};
}

StringGrammar name_object_grammar() noexcept {
    return StringGrammar{"a capitalised word, optionally after 'the '", &is_valid_name_object};
}

StringGrammar place_name_grammar() noexcept {
    return StringGrammar{"a name (letters, single spaces and ')", &is_valid_place_name};
}

StringGrammar place_form_grammar() noexcept {
    return StringGrammar{"a name form (letters, spaces, ' and slots such as {noun}, each slot at most once)",
                         &is_valid_place_form};
}

// --- Grammar of P3-02 11.2 ---------------------------------------------------------------

// Two to sixteen ASCII lower-case letters.
bool is_valid_filter_word(std::string_view text) noexcept {
    return text.size() >= 2U && text.size() <= 16U && is_valid_element(text);
}

StringGrammar filter_word_grammar() noexcept {
    return StringGrammar{"lower-case letters a-z, 2 to 16 of them", &is_valid_filter_word};
}

// --- Shared helpers --------------------------------------------------------------------

std::vector<std::string> strings_of(const Record& r, std::string_view field) {
    std::vector<std::string> out;
    for (const Value& v : r.get_array(field)) {
        out.push_back(v.as_string());
    }
    return out;
}

// The first later index j whose string equals that of an earlier index i,
// as (j, i).
std::optional<std::pair<std::size_t, std::size_t>> first_repeat(const std::vector<std::string>& items) {
    std::map<std::string, std::size_t, detail::BytesLess> first;
    for (std::size_t j = 0; j < items.size(); ++j) {
        const auto [it, inserted] = first.try_emplace(items[j], j);
        if (!inserted) {
            return std::make_pair(j, it->second);
        }
    }
    return std::nullopt;
}

std::optional<std::string> repeat_message(std::string_view field, const std::vector<std::string>& items) {
    if (const auto r = first_repeat(items)) {
        return std::string(field) + "[" + uint_text(r->first) + "] '" + items[r->first] + "' repeats " +
               std::string(field) + "[" + uint_text(r->second) + "]";
    }
    return std::nullopt;
}

// --- placename (P1-06 4.1) ---------------------------------------------------------------

// The word lists of a placename definition after forms, in declaration order.
constexpr std::array<std::string_view, 8> PLACE_LISTS = {"nouns",    "suffixes",   "prefixes", "adjectives",
                                                         "features", "possessors", "places",   "objects"};

Schema place_form_schema() {
    return Schema(std::vector<Field>{
        Field::required("form", Type::string(48, place_form_grammar())),
        Field::required("weight", Type::integer(1, 1000)),
    });
}

bool is_empty_list(const Record& r, std::string_view field) {
    return r.get_array(field).empty();
}

std::optional<std::string> check_placename_role_fields(const Record& r) {
    const std::string& role = r.get_string("role");
    const auto must = [&role](std::string_view field, std::string_view what) {
        return std::optional<std::string>("a " + role + " definition must have " + std::string(field) + " " +
                                          std::string(what));
    };
    const bool biomes_empty = is_empty_list(r, "biomes");
    const bool special_none = r.get_string("special") == "none";
    const bool trait_none = r.get_string("trait") == "none";
    if (role == "common") {
        if (!biomes_empty) return must("biomes", "empty");
        if (!special_none) return must("special", "none");
        if (!trait_none) return must("trait", "none");
        if (!is_empty_list(r, "forms")) return must("forms", "empty");
        if (!is_empty_list(r, "nouns")) return must("nouns", "empty");
        return std::nullopt;
    }
    if (role == "land") {
        if (!trait_none) return must("trait", "none");
        if (!biomes_empty && !special_none) {
            return std::string("a land definition must name biomes or a special, not both");
        }
        if (biomes_empty && special_none) {
            return std::string("a land definition must name biomes or a special, not neither");
        }
        if (is_empty_list(r, "forms")) return must("forms", "non-empty");
        if (is_empty_list(r, "nouns")) return must("nouns", "non-empty");
        if (is_empty_list(r, "suffixes")) return must("suffixes", "non-empty");
        if (!is_empty_list(r, "forbidden")) return must("forbidden", "empty");
        if (!is_empty_list(r, "excluded")) return must("excluded", "empty");
        return std::nullopt;
    }
    // trait
    if (trait_none) return must("trait", "not none");
    if (!biomes_empty) return must("biomes", "empty");
    if (!special_none) return must("special", "none");
    if (!is_empty_list(r, "forms")) return must("forms", "empty");
    if (!is_empty_list(r, "nouns")) return must("nouns", "empty");
    if (!is_empty_list(r, "forbidden")) return must("forbidden", "empty");
    if (!is_empty_list(r, "excluded")) return must("excluded", "empty");
    return std::nullopt;
}

std::optional<std::string> check_placename_lists_unique(const Record& r) {
    if (auto m = repeat_message("biomes", strings_of(r, "biomes"))) {
        return m;
    }
    std::vector<std::string> forms;
    for (const Value& v : r.get_array("forms")) {
        forms.push_back(v.as_record().get_string("form"));
    }
    if (auto m = repeat_message("forms", forms)) {
        return m;
    }
    for (const std::string_view field : PLACE_LISTS) {
        if (auto m = repeat_message(field, strings_of(r, field))) {
            return m;
        }
    }
    for (const std::string_view field : {std::string_view("forbidden"), std::string_view("excluded")}) {
        if (auto m = repeat_message(field, strings_of(r, field))) {
            return m;
        }
    }
    return std::nullopt;
}

std::string quoted_id(const Definition& d) {
    return "'" + d.id + "'";
}

std::vector<KindCheckFailure> check_placename_one_common(std::span<const Definition> defs) {
    std::uint64_t n = 0;
    for (const Definition& d : defs) {
        n += d.record.get_string("role") == "common" ? 1U : 0U;
    }
    if (n == 1U) {
        return {};
    }
    return {KindCheckFailure{std::nullopt, "there must be exactly one common definition, found " + uint_text(n)}};
}

std::vector<KindCheckFailure> check_placename_lands_distinct(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    // Biome keys over the land definitions, in id order then list order.
    std::map<std::string, std::size_t, detail::BytesLess> biome_first;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].record.get_string("role") != "land") {
            continue;
        }
        for (const std::string& key : strings_of(defs[i].record, "biomes")) {
            const auto [it, inserted] = biome_first.try_emplace(key, i);
            if (!inserted && it->second != i) {
                out.push_back(KindCheckFailure{i, "biome '" + key + "' is in " + quoted_id(defs[it->second]) +
                                                      " and " + quoted_id(defs[i])});
            }
        }
    }
    for (const std::string_view field : {std::string_view("special"), std::string_view("trait")}) {
        std::map<std::string, std::size_t, detail::BytesLess> first;
        for (std::size_t i = 0; i < defs.size(); ++i) {
            const std::string& v = defs[i].record.get_string(field);
            if (v == "none") {
                continue;
            }
            const auto [it, inserted] = first.try_emplace(v, i);
            if (!inserted) {
                out.push_back(KindCheckFailure{i, std::string(field) + " '" + v + "' is in " +
                                                      quoted_id(defs[it->second]) + " and " + quoted_id(defs[i])});
            }
        }
    }
    return out;
}

std::vector<KindCheckFailure> check_placename_common_sizes(std::span<const Definition> defs) {
    static constexpr std::array<std::pair<std::string_view, std::uint64_t>, 6> MINIMA = {{
        {"prefixes", 100},
        {"adjectives", 100},
        {"features", 16},
        {"possessors", 32},
        {"places", 16},
        {"objects", 32},
    }};
    std::vector<KindCheckFailure> out;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].record.get_string("role") != "common") {
            continue;
        }
        for (const auto& [field, n] : MINIMA) {
            const std::uint64_t m = defs[i].record.get_array(field).size();
            if (m < n) {
                out.push_back(KindCheckFailure{i, "the common definition needs at least " + uint_text(n) + " " +
                                                      std::string(field) + ", has " + uint_text(m)});
                break;
            }
        }
    }
    return out;
}

// --- name_filter (P3-02 11.2) --------------------------------------------------------------

constexpr std::array<std::string_view, 3> FILTER_LISTS = {"anywhere", "at_start", "whole"};

std::optional<std::string> check_name_filter_unique(const Record& r) {
    for (const std::string_view field : FILTER_LISTS) {
        if (auto m = repeat_message(field, strings_of(r, field))) {
            return m;
        }
    }
    return std::nullopt;
}

// The three lists of every definition, in id order.
using FilterLists = std::vector<std::array<std::vector<std::string>, 3>>;

FilterLists filter_lists(std::span<const Definition> defs) {
    FilterLists out;
    for (const Definition& d : defs) {
        std::array<std::vector<std::string>, 3> lists;
        for (std::size_t k = 0; k < FILTER_LISTS.size(); ++k) {
            lists[k] = strings_of(d.record, FILTER_LISTS[k]);
        }
        out.push_back(std::move(lists));
    }
    return out;
}

std::vector<KindCheckFailure> check_name_filter_distinct(std::span<const Definition> defs) {
    const FilterLists lists = filter_lists(defs);
    for (std::size_t j = 0; j < defs.size(); ++j) {
        for (std::size_t k = 0; k < FILTER_LISTS.size(); ++k) {
            for (const std::string& v : lists[j][k]) {
                for (std::size_t i = 0; i < j; ++i) {
                    for (const std::string& w : lists[i][k]) {
                        if (w == v) {
                            return {KindCheckFailure{j, std::string(FILTER_LISTS[k]) + " '" + v + "' is in " +
                                                            quoted_id(defs[i]) + " and " + quoted_id(defs[j])}};
                        }
                    }
                }
            }
        }
    }
    return {};
}

// The first entry (definition id, list, position) that makes `value`
// redundant: an `anywhere` entry inside it, or (for a `whole` entry) an
// `at_start` entry it starts with.
struct Cover {
    std::size_t definition = 0;
    std::string_view list;
    std::string entry;
};

std::optional<Cover> first_cover(const FilterLists& lists, std::string_view value, bool by_at_start) {
    for (std::size_t i = 0; i < lists.size(); ++i) {
        for (const std::string& a : lists[i][0]) {
            if (value.find(a) != std::string_view::npos) {
                return Cover{i, FILTER_LISTS[0], a};
            }
        }
        if (by_at_start) {
            for (const std::string& s : lists[i][1]) {
                if (value.substr(0, s.size()) == s) {
                    return Cover{i, FILTER_LISTS[1], s};
                }
            }
        }
    }
    return std::nullopt;
}

std::vector<KindCheckFailure> check_name_filter_not_redundant(std::span<const Definition> defs) {
    const FilterLists lists = filter_lists(defs);
    for (std::size_t j = 0; j < defs.size(); ++j) {
        for (std::size_t k = 1; k < FILTER_LISTS.size(); ++k) {  // at_start, then whole
            for (const std::string& v : lists[j][k]) {
                if (const std::optional<Cover> c = first_cover(lists, v, k == 2)) {
                    return {KindCheckFailure{j, quoted_id(defs[j]) + " " + std::string(FILTER_LISTS[k]) + " '" + v +
                                                    "' is already rejected by " + std::string(c->list) + " '" +
                                                    c->entry + "' of " + quoted_id(defs[c->definition])}};
                }
            }
        }
    }
    return {};
}

}  // namespace

Schema placename_schema() {
    // P1-06 4.1, in declaration order (the canonical encoding, P0-03 8).
    const auto words = [](std::uint64_t bytes, StringGrammar g, std::uint64_t max) {
        return Type::array(Type::string(bytes, g), 0, max);
    };
    std::vector<Field> fields{
        Field::required("role", Type::enumeration({"common", "land", "trait"})),
        Field::required("biomes", Type::array(Type::reference(std::string(BIOME_KIND)), 0, 16)),
        Field::required("special", Type::enumeration({"none", "isle", "lake", "upland"})),
        Field::required("trait", Type::enumeration(
                                     {"none", "coast", "river", "lake", "high", "ore", "pass", "barren", "fertile"})),
        Field::required("forms", Type::array(Type::record(place_form_schema()), 0, 16)),
        Field::required("nouns", words(24, word_grammar(), 1024)),
        Field::required("suffixes", words(24, element_grammar(), 1024)),
        Field::required("prefixes", words(24, word_grammar(), 1024)),
        Field::required("adjectives", words(24, word_grammar(), 1024)),
        Field::required("features", words(24, element_grammar(), 1024)),
        Field::required("possessors", words(24, word_grammar(), 1024)),
        Field::required("places", words(24, word_grammar(), 1024)),
        Field::required("objects", words(32, name_object_grammar(), 1024)),
        Field::required("forbidden", words(16, element_grammar(), 64)),
        Field::required("excluded", words(48, place_name_grammar(), 1024)),
    };
    std::vector<Check> checks{
        Check{"placename_role_fields", [](const Record& r) { return check_placename_role_fields(r); }},
        Check{"placename_lists_unique", [](const Record& r) { return check_placename_lists_unique(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

std::vector<KindCheck> placename_kind_checks() {
    std::vector<KindCheck> out;
    out.push_back(
        KindCheck{"placename_one_common", [](std::span<const Definition> d) { return check_placename_one_common(d); }});
    out.push_back(KindCheck{"placename_lands_distinct",
                            [](std::span<const Definition> d) { return check_placename_lands_distinct(d); }});
    out.push_back(KindCheck{"placename_common_sizes",
                            [](std::span<const Definition> d) { return check_placename_common_sizes(d); }});
    return out;
}

Schema name_filter_schema() {
    // P3-02 11.2, in declaration order.
    std::vector<Field> fields;
    for (const std::string_view field : FILTER_LISTS) {
        fields.push_back(
            Field::required(std::string(field), Type::array(Type::string(16, filter_word_grammar()), 0, 4096)));
    }
    std::vector<Check> checks{
        Check{"name_filter_unique", [](const Record& r) { return check_name_filter_unique(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

std::vector<KindCheck> name_filter_kind_checks() {
    std::vector<KindCheck> out;
    out.push_back(
        KindCheck{"name_filter_distinct", [](std::span<const Definition> d) { return check_name_filter_distinct(d); }});
    out.push_back(KindCheck{"name_filter_not_redundant",
                            [](std::span<const Definition> d) { return check_name_filter_not_redundant(d); }});
    return out;
}

}  // namespace worldgen::data
