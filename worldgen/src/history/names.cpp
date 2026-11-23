// The name generator (spec P3-02).

#include "worldgen/history/names.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/roman.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

namespace {

constexpr std::string_view ROOT_SLOT = "{root}";

[[nodiscard]] bool is_lower(char c) noexcept {
    return c >= 'a' && c <= 'z';
}

[[nodiscard]] bool is_separator(char c) noexcept {
    return c == '\'' || c == '-';
}

// Upper-casing by arithmetic, no locale (P3-02 7).
[[nodiscard]] char to_upper(char c) noexcept {
    return static_cast<char>(c - 'a' + 'A');
}

[[nodiscard]] std::vector<std::string> string_list(const data::Record& record, std::string_view field) {
    std::vector<std::string> out;
    for (const data::Value& v : record.get_array(field)) {
        out.push_back(v.as_string());
    }
    return out;
}

[[nodiscard]] const std::string& pick_from(const std::vector<std::string>& list, std::uint64_t u) {
    if (list.empty()) {
        fatal("names: a naming list is empty");
    }
    return list[static_cast<std::size_t>(pick(u, list.size()))];
}

[[nodiscard]] std::int64_t letter_count(std::string_view root) noexcept {
    std::int64_t n = 0;
    for (const char c : root) {
        if (is_lower(c)) {
            ++n;
        }
    }
    return n;
}

// Separators: not at either end, no two adjacent (P3-02 4.2).
[[nodiscard]] bool separators_ok(std::string_view root) noexcept {
    if (root.empty()) {
        return true;
    }
    if (is_separator(root.front()) || is_separator(root.back())) {
        return false;
    }
    for (std::size_t i = 1; i < root.size(); ++i) {
        if (is_separator(root[i - 1]) && is_separator(root[i])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool root_valid(const CultureNaming& naming, std::string_view root) noexcept {
    const std::int64_t letters = letter_count(root);
    if (letters < naming.min_letters || letters > naming.max_letters) {
        return false;
    }
    for (const std::string& f : naming.forbidden) {
        if (root.find(f) != std::string_view::npos) {
            return false;
        }
    }
    return separators_ok(root);
}

// r13 13.2: lower(n) holds one letter a-z at three consecutive positions
// (A-Z mapped to a-z; every other character breaks a run).
[[nodiscard]] char lower_letter(char c) noexcept {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

[[nodiscard]] bool triple_letter(std::string_view name) noexcept {
    for (std::size_t i = 2; i < name.size(); ++i) {
        const char c = lower_letter(name[i]);
        if (c >= 'a' && c <= 'z' && lower_letter(name[i - 1]) == c && lower_letter(name[i - 2]) == c) {
            return true;
        }
    }
    return false;
}

// r15 14.2: one syllable's text with (s) and without (b) its codas.
struct Syllable {
    std::string s;
    std::string b;
};

// The syllables of 14.2 from a pattern and the strings drawn for its O, N
// and C symbols (in order).
[[nodiscard]] std::vector<Syllable> syllables(std::string_view pattern, const std::vector<std::string>& drawn) {
    std::vector<Syllable> out;
    std::size_t k = 0;
    bool open = false;  // a syllable is open (none before the first symbol)
    char prev = '\0';
    for (const char sym : pattern) {
        if (is_separator(sym)) {
            open = false;  // a boundary; the separator belongs to no syllable
            prev = sym;
            continue;
        }
        if (sym != 'O' && sym != 'N' && sym != 'C') {
            fatal("names: a name pattern holds a symbol other than O, N, C, ' and -");
        }
        const bool boundary = !open || sym == 'O' || (sym == 'N' && prev != 'O');
        if (boundary) {
            out.emplace_back();
            open = true;
        }
        if (k >= drawn.size()) {
            fatal("names: fewer drawn strings than pattern symbols");
        }
        const std::string& text = drawn[k];
        ++k;
        out.back().s += text;
        if (sym != 'C') {
            out.back().b += text;
        }
        prev = sym;
    }
    if (k != drawn.size()) {
        fatal("names: more drawn strings than pattern symbols");
    }
    return out;
}

// The form with its one {root} replaced. Fatal unless the form holds
// exactly one {root}.
[[nodiscard]] std::string fill_form(std::string_view form, std::string_view cap_root) {
    const std::size_t at = form.find(ROOT_SLOT);
    if (at == std::string_view::npos || form.find(ROOT_SLOT, at + 1) != std::string_view::npos) {
        fatal("names: a name form must hold exactly one {root}");
    }
    std::string out(form.substr(0, at));
    out += cap_root;
    out += form.substr(at + ROOT_SLOT.size());
    return out;
}

[[nodiscard]] char to_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] std::string lower_text(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = to_lower(c);
    }
    return out;
}

// The letters of a text without ' and - (S of 11.3).
[[nodiscard]] std::string squeezed(std::string_view s) {
    std::string out;
    for (const char c : s) {
        if (!is_separator(c)) {
            out += c;
        }
    }
    return out;
}

[[nodiscard]] bool starts_with(std::string_view text, std::string_view prefix) noexcept {
    return text.substr(0, prefix.size()) == prefix;
}

// The first entry of `list` found inside `text`, or nullptr.
[[nodiscard]] const std::string* first_inside(const std::vector<std::string>& list, std::string_view text) {
    for (const std::string& e : list) {
        if (text.find(e) != std::string_view::npos) {
            return &e;
        }
    }
    return nullptr;
}

// The first entry of `list` that `text` starts with, or nullptr.
[[nodiscard]] const std::string* first_start(const std::vector<std::string>& list, std::string_view text) {
    for (const std::string& e : list) {
        if (starts_with(text, e)) {
            return &e;
        }
    }
    return nullptr;
}

// A form split at its one {root}: (A, B). Fatal unless it holds exactly one.
[[nodiscard]] std::pair<std::string_view, std::string_view> split_form(std::string_view form) {
    const std::size_t at = form.find(ROOT_SLOT);
    if (at == std::string_view::npos || form.find(ROOT_SLOT, at + 1) != std::string_view::npos) {
        fatal("names: a name form must hold exactly one {root}");
    }
    return {form.substr(0, at), form.substr(at + ROOT_SLOT.size())};
}

[[nodiscard]] NameAttempt attempt_with_key(const CultureNaming& naming, const NameFilter& filter, NameKind kind,
                                           std::uint64_t key, std::uint64_t culture, std::uint64_t entity,
                                           std::uint64_t attempt) {
    const auto u = [&](std::uint64_t j) { return draw(key, attempt, culture, entity, j); };
    const std::string& form = pick_from(name_forms(naming, kind), u(0));
    // r5 (11.4): gods take their shape from god_patterns.
    const std::string& pattern = pick_from(kind == NameKind::god ? naming.god_patterns : naming.patterns, u(1));
    NameAttempt out;
    std::vector<std::string> drawn;  // r15 14.2: the strings of O, N and C, in order
    std::uint64_t j = 2;
    for (const char s : pattern) {
        switch (s) {
            case 'O':
                drawn.push_back(pick_from(naming.onsets, u(j)));
                out.root += drawn.back();
                ++j;
                break;
            case 'N':
                drawn.push_back(pick_from(naming.nuclei, u(j)));
                out.root += drawn.back();
                ++j;
                break;
            case 'C':
                drawn.push_back(pick_from(naming.codas, u(j)));
                out.root += drawn.back();
                ++j;
                break;
            case '\'':
            case '-':
                out.root += s;
                break;
            default:
                fatal("names: a name pattern holds a symbol other than O, N, C, ' and -");
        }
    }
    out.name = fill_form(form, capitalize_root(out.root));
    out.valid = root_valid(naming, out.root) && out.name.size() <= NAME_ATTEMPT_MAX_BYTES;
    // r5 (11.3): the root word must pass the name filter.
    if (out.valid && name_filtered(filter, root_word(form, out.root))) {
        out.valid = false;
        out.filtered = true;
    }
    // r13 13.2: no three equal letters in a row in the rendered name.
    if (out.valid && triple_letter(out.name)) {
        out.valid = false;
    }
    // r15 14.2: no syllable said twice in a row (no draw added).
    if (out.valid && repeated_syllable(pattern, drawn)) {
        out.valid = false;
        out.repeated = true;
    }
    // r19 16.1: no doubled place suffix (no draw added).
    if (out.valid) {
        const auto [a, b] = split_form(form);
        (void)a;
        const std::string x = squeezed(lower_text(b.substr(0, b.find(' '))));
        if (!x.empty() && doubled_suffix(out.root, x, fused_tails(name_forms(naming, kind)))) {
            out.valid = false;
            out.doubled = true;
        }
    }
    return out;
}

[[nodiscard]] bool is_tail_vowel(char c) noexcept {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

[[nodiscard]] bool ends_with(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

// The letters of one word of a name (P1-06 r11 17.1): S_w and the offset of
// each letter in the name; part[i] is the index of the part (run of letters
// between ' and -) letter i belongs to.
struct WordLetters {
    std::string letters;
    std::vector<std::size_t> offset;
    std::vector<std::size_t> part;
};

[[nodiscard]] bool spans_join(const std::vector<std::size_t>& joins, std::size_t first, std::size_t last) noexcept {
    for (const std::size_t j : joins) {
        if (first < j && j <= last) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool word_filtered_at_joins(const NameFilter& filter, const WordLetters& w,
                                          const std::vector<std::size_t>& joins) {
    const std::string_view s = w.letters;
    for (const std::string& e : filter.anywhere) {
        if (e.empty()) {
            continue;
        }
        for (std::size_t p = s.find(e); p != std::string_view::npos; p = s.find(e, p + 1)) {
            if (spans_join(joins, w.offset[p], w.offset[p + e.size() - 1])) {
                return true;
            }
        }
    }
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t j = i;
        while (j < s.size() && w.part[j] == w.part[i]) {
            ++j;
        }
        const std::string_view part = s.substr(i, j - i);
        for (const std::string& e : filter.at_start) {
            if (!e.empty() && starts_with(part, e) && spans_join(joins, w.offset[i], w.offset[i + e.size() - 1])) {
                return true;
            }
        }
        for (const std::string& e : filter.whole) {
            if (part == e && spans_join(joins, w.offset[i], w.offset[j - 1])) {
                return true;
            }
        }
        i = j;
    }
    return false;
}

const NameFilter& empty_filter() {
    static const NameFilter none;
    return none;
}

}  // namespace

bool repeated_syllable(std::string_view pattern, const std::vector<std::string>& drawn) {
    const std::vector<Syllable> syl = syllables(pattern, drawn);
    for (std::size_t i = 0; i + 1 < syl.size(); ++i) {
        const std::string& si = syl[i].s;
        if (!si.empty() && (si == syl[i + 1].s || si == syl[i + 1].b)) {
            return true;
        }
    }
    return false;
}

CultureNaming culture_naming_from(const data::Record& naming) {
    CultureNaming out;
    out.onsets = string_list(naming, "onsets");
    out.nuclei = string_list(naming, "nuclei");
    out.codas = string_list(naming, "codas");
    out.patterns = string_list(naming, "patterns");
    out.god_patterns = string_list(naming, "god_patterns");  // r5
    out.min_letters = naming.get_int("min_letters");
    out.max_letters = naming.get_int("max_letters");
    out.forbidden = string_list(naming, "forbidden");
    out.god_forms = string_list(naming, "god_forms");
    out.civilization_forms = string_list(naming, "civilization_forms");
    out.settlement_forms = string_list(naming, "settlement_forms");
    return out;
}

std::string_view name_kind_text(NameKind kind) noexcept {
    switch (kind) {
        case NameKind::god:
            return "god";
        case NameKind::civilization:
            return "civilization";
        case NameKind::settlement:
            return "settlement";
    }
    return "settlement";  // unreachable for a valid enumerator
}

std::string_view name_stream(NameKind kind) noexcept {
    switch (kind) {
        case NameKind::god:
            return streams::hist_names_god;
        case NameKind::civilization:
            return streams::hist_names_civilization;
        case NameKind::settlement:
            return streams::hist_names_settlement;
    }
    return streams::hist_names_settlement;  // unreachable for a valid enumerator
}

const std::vector<std::string>& name_forms(const CultureNaming& naming, NameKind kind) noexcept {
    switch (kind) {
        case NameKind::god:
            return naming.god_forms;
        case NameKind::civilization:
            return naming.civilization_forms;
        case NameKind::settlement:
            return naming.settlement_forms;
    }
    return naming.settlement_forms;  // unreachable for a valid enumerator
}

std::string capitalize_root(std::string_view root) {
    std::string out(root);
    bool up = true;  // the first character, then each one after a '-'
    for (char& c : out) {
        const bool next_up = c == '-';
        if (up && is_lower(c)) {
            c = to_upper(c);
        }
        up = next_up;
    }
    return out;
}

NameFilter name_filter_from(const data::ContentSet& content) {
    NameFilter out;
    const data::KindContent* kind = content.find_kind(data::NAME_FILTER_KIND);
    if (kind == nullptr) {
        return out;
    }
    for (const data::Definition& d : kind->definitions) {  // ascending id
        for (const std::string& e : string_list(d.record, "anywhere")) out.anywhere.push_back(e);
        for (const std::string& e : string_list(d.record, "at_start")) out.at_start.push_back(e);
        for (const std::string& e : string_list(d.record, "whole")) out.whole.push_back(e);
    }
    return out;
}

std::string root_word(std::string_view form, std::string_view root) {
    const auto [a, b] = split_form(form);
    const std::size_t last_space = a.rfind(' ');
    const std::string_view a1 = last_space == std::string_view::npos ? a : a.substr(last_space + 1);
    const std::string_view b1 = b.substr(0, b.find(' '));
    return lower_text(a1) + std::string(root) + lower_text(b1);
}

bool name_filtered(const NameFilter& filter, std::string_view w) {
    if (first_inside(filter.anywhere, squeezed(w)) != nullptr) {
        return true;
    }
    // The parts: maximal runs of a-z, W split at ' and -.
    std::size_t i = 0;
    while (i < w.size()) {
        if (!is_lower(w[i])) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < w.size() && is_lower(w[j])) {
            ++j;
        }
        const std::string_view part = w.substr(i, j - i);
        if (first_start(filter.at_start, part) != nullptr) {
            return true;
        }
        for (const std::string& e : filter.whole) {
            if (part == e) {
                return true;
            }
        }
        i = j;
    }
    return false;
}

std::vector<std::string> fused_tails(const std::vector<std::string>& forms) {
    std::vector<std::string> out;
    for (const std::string& f : forms) {
        const auto [a, b] = split_form(f);
        (void)a;
        std::string x = squeezed(lower_text(b.substr(0, b.find(' '))));
        if (!x.empty() && std::find(out.begin(), out.end(), x) == out.end()) {
            out.push_back(std::move(x));
        }
    }
    return out;
}

std::string tail_first_syllable(std::string_view x) {
    std::size_t i = 0;
    while (i < x.size() && !is_tail_vowel(x[i])) {
        ++i;
    }
    while (i < x.size() && is_tail_vowel(x[i])) {
        ++i;
    }
    while (i < x.size() && !is_tail_vowel(x[i])) {
        ++i;
    }
    return std::string(x.substr(0, i));
}

bool doubled_suffix(std::string_view root, std::string_view x, const std::vector<std::string>& tails) {
    const std::string r = squeezed(lower_text(root));
    if (r.empty() || x.empty()) {
        return false;
    }
    // Item 1: the tail or its first syllable.
    if (ends_with(r, x) || ends_with(r, tail_first_syllable(x))) {
        return true;
    }
    // Item 2: another tail of the kind.
    for (const std::string& y : tails) {
        if (ends_with(r, y)) {
            return true;
        }
    }
    // Item 3: the root's last letter is the tail's first.
    return r.back() == x.front();
}

bool name_filtered_at_joins(const NameFilter& filter, std::string_view name, const std::vector<std::size_t>& joins) {
    std::size_t start = 0;
    while (start <= name.size()) {
        const std::size_t sp = name.find(' ', start);
        const std::size_t end = sp == std::string_view::npos ? name.size() : sp;
        WordLetters w;
        std::size_t part = 0;
        for (std::size_t o = start; o < end; ++o) {
            const char c = to_lower(name[o]);
            if (is_lower(c)) {
                w.letters += c;
                w.offset.push_back(o);
                w.part.push_back(part);
            } else {
                ++part;  // ' and - (and any other byte) end a part
            }
        }
        if (word_filtered_at_joins(filter, w, joins)) {
            return true;
        }
        if (sp == std::string_view::npos) {
            break;
        }
        start = sp + 1;
    }
    return false;
}

std::optional<std::string> names_form_text_problem(const data::ContentSet& content, const NameFilter& filter) {
    const data::KindContent* cultures = content.find_kind(data::CULTURE_KIND);
    if (cultures == nullptr) {
        return std::nullopt;
    }
    for (const data::Definition& c : cultures->definitions) {  // ascending id
        const CultureNaming naming = culture_naming_from(c.record.get_record("naming"));
        for (const NameKind kind : {NameKind::god, NameKind::civilization, NameKind::settlement}) {
            const std::vector<std::string>& forms = name_forms(naming, kind);
            for (std::size_t i = 0; i < forms.size(); ++i) {
                const auto [a, b] = split_form(forms[i]);
                const std::size_t last_space = a.rfind(' ');
                const std::string_view a1 = last_space == std::string_view::npos ? a : a.substr(last_space + 1);
                const std::size_t first_space = b.find(' ');
                const std::string_view b1 = b.substr(0, first_space);
                const std::string sa = squeezed(lower_text(a1));
                const std::string sb = squeezed(lower_text(b1));
                const std::string* hit = first_inside(filter.anywhere, sa);
                if (hit == nullptr) hit = first_inside(filter.anywhere, sb);
                if (hit == nullptr && !sa.empty()) hit = first_start(filter.at_start, sa);
                // r6: the fixed words, A before A' and B after B'.
                std::vector<std::string> fixed;
                const auto add_words = [&fixed](std::string_view text) {
                    std::size_t from = 0;
                    while (from <= text.size()) {
                        const std::size_t sp = text.find(' ', from);
                        const std::string_view w = text.substr(from, sp == std::string_view::npos ? text.size() - from
                                                                                                   : sp - from);
                        if (!w.empty()) {
                            fixed.push_back(squeezed(lower_text(w)));
                        }
                        if (sp == std::string_view::npos) {
                            break;
                        }
                        from = sp + 1;
                    }
                };
                if (last_space != std::string_view::npos) {
                    add_words(a.substr(0, last_space));
                }
                if (first_space != std::string_view::npos) {
                    add_words(b.substr(first_space + 1));
                }
                for (const std::string& w : fixed) {
                    if (hit == nullptr) hit = first_inside(filter.anywhere, w);
                    if (hit == nullptr) hit = first_start(filter.at_start, w);
                }
                if (hit != nullptr) {
                    return "history: culture '" + c.id + "' " + std::string(name_kind_text(kind)) + "_forms[" +
                           db::format_u64(static_cast<std::uint64_t>(i)) + "] '" + forms[i] + "' holds the filtered text '" + *hit + "'";
                }
            }
        }
    }
    return std::nullopt;
}

void check_names_form_text(const data::ContentSet& content, const NameFilter& filter) {
    if (std::optional<std::string> problem = names_form_text_problem(content, filter)) {
        throw FatalError(*problem);
    }
}

NameAttempt name_attempt(const CultureNaming& naming, const NameFilter& filter, NameKind kind, std::uint64_t seed,
                         std::uint64_t culture, std::uint64_t entity, std::uint64_t attempt) {
    return attempt_with_key(naming, filter, kind, stream_key(seed, name_stream(kind)), culture, entity, attempt);
}

NameAttempt name_attempt(const CultureNaming& naming, NameKind kind, std::uint64_t seed, std::uint64_t culture,
                         std::uint64_t entity, std::uint64_t attempt) {
    return name_attempt(naming, empty_filter(), kind, seed, culture, entity, attempt);
}

bool NameRegistry::contains(std::string_view name) const {
    return names_.find(name) != names_.end();
}

void NameRegistry::add(std::string name) {
    names_.insert(std::move(name));
}

std::size_t NameRegistry::size() const noexcept {
    return names_.size();
}

namespace {

// 4.3, with (r10 12.2) the core rule when `cores` is given.
NameChoice choose_name_impl(const CultureNaming& naming, const NameFilter& filter, std::string_view culture_id,
                            NameKind kind, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                            NameRegistry& registry, NameCores* cores) {
    const std::uint64_t key = stream_key(seed, name_stream(kind));
    std::optional<NameChoice> first;
    std::string first_core;
    for (std::uint64_t t = 0; t < NAME_ATTEMPTS; ++t) {
        // A filtered attempt is invalid: skipped, never the fallback's first
        // name (11.3); the next attempt is drawn with its own keys.
        NameAttempt a = attempt_with_key(naming, filter, kind, key, culture, entity, t);
        if (!a.valid) {
            continue;
        }
        std::string core = cores != nullptr ? name_core(a.root) : std::string();
        if (!first) {
            first = NameChoice{a.name, t, true};
            first_core = core;
        }
        if (!registry.contains(a.name) && (cores == nullptr || !cores->near(core))) {
            registry.add(a.name);
            if (cores != nullptr) {
                cores->add(std::move(core));
            }
            return NameChoice{std::move(a.name), t, false};
        }
    }
    if (!first) {
        std::string message = "names: culture '";
        message += culture_id;
        message += "' produced no valid ";
        message += name_kind_text(kind);
        message += " name in 64 attempts";
        fatal(message.c_str());
    }
    std::uint64_t k = 2;
    while (registry.contains(first->name + " " + roman(k))) {
        ++k;
    }
    first->name += " " + roman(k);
    if (first->name.size() > NAME_MAX_BYTES) {
        fatal("names: a name longer than 64 bytes");
    }
    registry.add(first->name);
    if (cores != nullptr) {
        cores->add(std::move(first_core));
    }
    return std::move(*first);
}

}  // namespace

std::string name_core(std::string_view root) {
    std::string out;
    for (const char c : root) {
        if (c != '\'' && c != '-') {
            out += c;
        }
    }
    return out;
}

bool within_one_edit(std::string_view a, std::string_view b) noexcept {
    if (a.size() < b.size()) {
        std::swap(a, b);  // a is the longer
    }
    if (a.size() - b.size() > 1U) {
        return false;
    }
    std::size_t i = 0;
    while (i < b.size() && a[i] == b[i]) {
        ++i;
    }
    if (i == b.size()) {
        return true;  // equal, or a has one more letter at the end
    }
    if (a.size() == b.size()) {
        return a.substr(i + 1) == b.substr(i + 1);  // one letter replaced
    }
    return a.substr(i + 1) == b.substr(i);  // one letter inserted
}

bool NameCores::near(std::string_view core) const {
    for (const std::string& k : cores_) {
        if (within_one_edit(k, core)) {
            return true;
        }
    }
    return false;
}

void NameCores::add(std::string core) {
    cores_.insert(std::move(core));
}

std::size_t NameCores::size() const noexcept {
    return cores_.size();
}

NameChoice choose_name(const CultureNaming& naming, const NameFilter& filter, std::string_view culture_id,
                       NameKind kind, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                       NameRegistry& registry) {
    return choose_name_impl(naming, filter, culture_id, kind, seed, culture, entity, registry, nullptr);
}

NameChoice choose_name(const CultureNaming& naming, const NameFilter& filter, std::string_view culture_id,
                       NameKind kind, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                       NameRegistry& registry, NameCores& cores) {
    return choose_name_impl(naming, filter, culture_id, kind, seed, culture, entity, registry, &cores);
}

NameChoice choose_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind, std::uint64_t seed,
                       std::uint64_t culture, std::uint64_t entity, NameRegistry& registry) {
    return choose_name(naming, empty_filter(), culture_id, kind, seed, culture, entity, registry);
}

std::string make_name(const CultureNaming& naming, const NameFilter& filter, std::string_view culture_id,
                      NameKind kind, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                      NameRegistry& registry) {
    return choose_name(naming, filter, culture_id, kind, seed, culture, entity, registry).name;
}

std::string make_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind, std::uint64_t seed,
                      std::uint64_t culture, std::uint64_t entity, NameRegistry& registry) {
    return make_name(naming, empty_filter(), culture_id, kind, seed, culture, entity, registry);
}

std::string roman(std::uint64_t k) {
    if (k < 1 || k > 100000) {
        fatal("roman: k outside [1, 100000]");
    }
    return roman_numeral(static_cast<std::int64_t>(k));
}

}  // namespace worldgen::history
