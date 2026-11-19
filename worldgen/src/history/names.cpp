// The name generator (spec P3-02).

#include "worldgen/history/names.hpp"

#include <optional>
#include <utility>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/stages/regions.hpp"

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

[[nodiscard]] NameAttempt attempt_with_key(const CultureNaming& naming, NameKind kind, std::uint64_t key,
                                           std::uint64_t culture, std::uint64_t entity, std::uint64_t attempt) {
    const auto u = [&](std::uint64_t j) { return draw(key, attempt, culture, entity, j); };
    const std::string& form = pick_from(name_forms(naming, kind), u(0));
    const std::string& pattern = pick_from(naming.patterns, u(1));
    NameAttempt out;
    std::uint64_t j = 2;
    for (const char s : pattern) {
        switch (s) {
            case 'O':
                out.root += pick_from(naming.onsets, u(j));
                ++j;
                break;
            case 'N':
                out.root += pick_from(naming.nuclei, u(j));
                ++j;
                break;
            case 'C':
                out.root += pick_from(naming.codas, u(j));
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
    return out;
}

}  // namespace

CultureNaming culture_naming_from(const data::Record& naming) {
    CultureNaming out;
    out.onsets = string_list(naming, "onsets");
    out.nuclei = string_list(naming, "nuclei");
    out.codas = string_list(naming, "codas");
    out.patterns = string_list(naming, "patterns");
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

NameAttempt name_attempt(const CultureNaming& naming, NameKind kind, std::uint64_t seed, std::uint64_t culture,
                         std::uint64_t entity, std::uint64_t attempt) {
    return attempt_with_key(naming, kind, stream_key(seed, name_stream(kind)), culture, entity, attempt);
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

NameChoice choose_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind, std::uint64_t seed,
                       std::uint64_t culture, std::uint64_t entity, NameRegistry& registry) {
    const std::uint64_t key = stream_key(seed, name_stream(kind));
    std::optional<NameChoice> first;
    for (std::uint64_t t = 0; t < NAME_ATTEMPTS; ++t) {
        NameAttempt a = attempt_with_key(naming, kind, key, culture, entity, t);
        if (!a.valid) {
            continue;
        }
        if (!first) {
            first = NameChoice{a.name, t, true};
        }
        if (!registry.contains(a.name)) {
            registry.add(a.name);
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
    return std::move(*first);
}

std::string make_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind, std::uint64_t seed,
                      std::uint64_t culture, std::uint64_t entity, NameRegistry& registry) {
    return choose_name(naming, culture_id, kind, seed, culture, entity, registry).name;
}

std::string roman(std::uint64_t k) {
    if (k < 1 || k > 100000) {
        fatal("roman: k outside [1, 100000]");
    }
    return stages::roman(static_cast<std::int64_t>(k));
}

}  // namespace worldgen::history
