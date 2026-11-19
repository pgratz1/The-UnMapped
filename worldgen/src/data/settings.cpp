// The world settings file (P0-03 section 6, P3-01 section 6).

#include "worldgen/data/settings.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/common.hpp"
#include "data/text.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

namespace worldgen::data {

namespace {

using detail::ErrorList;
using detail::Node;
using detail::NodeKind;

constexpr std::string_view SEED_EXPECTED =
    "expected an integer from 0 to 9223372036854775807, or a string holding a decimal number from 0 to "
    "18446744073709551615 (no leading zeros) or \"0x\" followed by 1 to 16 hex digits";

std::uint32_t hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return static_cast<std::uint32_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<std::uint32_t>(c - 'a') + 10U;
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<std::uint32_t>(c - 'A') + 10U;
    }
    return 16U;  // not a hex digit
}

void parse_seed(const Node& node, std::string_view path, ErrorList& errors, Settings& out) {
    if (detail::is_rejected_scalar(node)) {
        return;  // already reported by the value scan
    }
    if (node.kind == NodeKind::integer) {
        if (node.integer < 0) {
            errors.add(path, node.position, "'world.seed' is negative; " + std::string(SEED_EXPECTED));
            return;
        }
        out.seed = static_cast<std::uint64_t>(node.integer);
        return;
    }
    if (node.kind == NodeKind::string) {
        const std::optional<std::uint64_t> seed = parse_seed_text(node.text);
        if (!seed) {
            errors.add(path, node.position,
                       "invalid seed " + detail::in_quotes(node.text) + " for 'world.seed'; " + std::string(SEED_EXPECTED));
            return;
        }
        out.seed = *seed;
        return;
    }
    errors.add(path, node.position,
               "'world.seed' is " + std::string(detail::kind_phrase(node)) + "; " + std::string(SEED_EXPECTED));
}

void parse_phrase(const Node& node, std::string_view path, ErrorList& errors, Settings& out) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    if (node.kind != NodeKind::string) {
        errors.add(path, node.position,
                   "'world.seed_phrase' is " + std::string(detail::kind_phrase(node)) + "; expected a string");
        return;
    }
    // Deliberately not read as a schema string(max_bytes) field: the default
    // 4096-byte limit of P0-03 section 4 does not apply to the raw phrase.
    // Only the scrubbed phrase is limited (PHRASE_MAX_BYTES, by
    // seed_from_phrase), so a long text that scrubs short is accepted; the
    // 1 MiB file limit bounds the raw text. The string rule (forbidden
    // control characters) was applied by is_rejected_scalar above.
    const PhraseSeed ps = seed_from_phrase(node.text);
    switch (ps.error) {
        case PhraseError::none:
            out.seed = ps.seed;
            out.seed_phrase = ps.phrase;
            return;
        case PhraseError::empty:
            errors.add(path, node.position,
                       "'world.seed_phrase' has no letter a-z or digit 0-9; a phrase keeps only those, other "
                       "characters become spaces");
            return;
        case PhraseError::too_long:
            errors.add(path, node.position,
                       "'world.seed_phrase' is longer than 256 bytes after scrubbing (letters a-z, digits 0-9 and "
                       "single spaces)");
            return;
    }
}

void parse_map_size(const Node& node, std::string_view path, ErrorList& errors, Settings& out) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    if (node.kind == NodeKind::integer && is_valid_map_size(node.integer)) {
        out.map_size = node.integer;
        return;
    }
    const std::string found = node.kind == NodeKind::integer ? detail::int_text(node.integer)
                                                             : std::string(detail::kind_phrase(node));
    errors.add(path, node.position, "'world.map_size' is " + found + "; expected 512, 1024 or 2048");
}

// --- History keys (P3-01 6.1, 6.2) -------------------------------------------------

// "<found>" of a message: the integer, the string in double quotes, or the
// kind of value.
std::string found_text(const Node& node) {
    if (node.kind == NodeKind::integer) {
        return detail::int_text(node.integer);
    }
    if (node.kind == NodeKind::string) {
        return "\"" + node.text + "\"";
    }
    return std::string(detail::kind_phrase(node));
}

void value_error(const Node& node, std::string_view path, ErrorList& errors, std::string_view key,
                 std::string_view expected) {
    errors.add(path, node.position,
               "'" + std::string(key) + "' is " + found_text(node) + "; expected " + std::string(expected));
}

void parse_history_span(const Node& node, std::string_view path, ErrorList& errors, Settings& out) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    if (node.kind == NodeKind::integer && is_valid_history_span(node.integer)) {
        out.history_span = node.integer;
        return;
    }
    value_error(node, path, errors, "world.history_span", "an integer from 2000 to 12000");
}

// A three-valued string key: parse is the enum's parse_* function.
template <typename E>
void parse_enum_key(const Node& node, std::string_view path, ErrorList& errors, std::string_view key,
                    std::string_view expected, std::optional<E> (*parse)(std::string_view) noexcept, E& out) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    if (node.kind == NodeKind::string) {
        if (const std::optional<E> v = parse(node.text)) {
            out = *v;
            return;
        }
    }
    value_error(node, path, errors, key, expected);
}

void parse_civs_per_race(const Node& node, std::string_view path, ErrorList& errors, Settings& out) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    if (node.kind == NodeKind::string && node.text == "auto") {
        out.civs_per_race = std::nullopt;
        return;
    }
    if (node.kind == NodeKind::integer && node.integer >= MIN_CIVS_PER_RACE && node.integer <= MAX_CIVS_PER_RACE) {
        out.civs_per_race = node.integer;
        return;
    }
    value_error(node, path, errors, "world.civs_per_race", "\"auto\" or an integer from 1 to 4");
}

constexpr std::string_view RACES_EXPECTED = "\"all\" or an array of race ids";

void parse_races(const Node& node, std::string_view path, ErrorList& errors, Settings& out) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    if (node.kind == NodeKind::string && node.text == "all") {
        out.races = std::nullopt;
        out.races_origins.clear();
        return;
    }
    if (node.kind != NodeKind::array) {
        value_error(node, path, errors, "world.races", RACES_EXPECTED);
        return;
    }
    if (node.children.empty()) {
        errors.add(path, node.position,
                   "'world.races' is an empty array; expected " + std::string(RACES_EXPECTED));
        return;
    }
    bool ok = true;
    std::vector<std::string> ids;
    std::vector<ValueOrigin> origins;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        const Node& item = node.children[i];
        const std::string key = detail::element_key("world.races", static_cast<std::uint64_t>(i));
        if (detail::is_rejected_scalar(item)) {
            ok = false;
            continue;
        }
        if (item.kind != NodeKind::string || !is_valid_id(item.text)) {
            value_error(item, path, errors, key, "a race id");
            ok = false;
            continue;
        }
        bool repeated = false;
        for (const std::string& id : ids) {
            repeated = repeated || id == item.text;
        }
        if (repeated) {
            errors.add(path, item.position, "'" + key + "' is \"" + item.text + "\", which is already listed");
            ok = false;
            continue;
        }
        ids.push_back(item.text);
        origins.push_back(ValueOrigin{std::string(path), item.position.line, item.position.column});
    }
    if (ok) {
        out.races = std::move(ids);
        out.races_origins = std::move(origins);
    }
}

void parse_divine_script(const Node& node, std::string_view path, ErrorList& errors) {
    if (detail::is_rejected_scalar(node)) {
        return;
    }
    errors.add(path, node.position, "'world.divine_script' is not supported yet: divine scripts need phase P5");
}

void parse_world(const Node& world, std::string_view path, ErrorList& errors, Settings& out) {
    const Node* seed = nullptr;
    const Node* phrase = nullptr;
    for (std::size_t i = 0; i < world.keys.size(); ++i) {
        const std::string& key = world.keys[i];
        const Node& value = world.children[i];
        if (key == "seed") {
            seed = &value;
            parse_seed(value, path, errors, out);
        } else if (key == "seed_phrase") {
            phrase = &value;
            parse_phrase(value, path, errors, out);
        } else if (key == "map_size") {
            parse_map_size(value, path, errors, out);
        } else if (key == "history_span") {
            parse_history_span(value, path, errors, out);
        } else if (key == "history_detail") {
            parse_enum_key(value, path, errors, "world.history_detail", "sparse, normal or dense",
                           &parse_history_detail, out.history_detail);
        } else if (key == "civs_per_race") {
            parse_civs_per_race(value, path, errors, out);
        } else if (key == "races") {
            parse_races(value, path, errors, out);
        } else if (key == "event_intensity") {
            parse_enum_key(value, path, errors, "world.event_intensity", "calm, normal or turbulent",
                           &parse_event_intensity, out.event_intensity);
        } else if (key == "monster_pressure") {
            parse_enum_key(value, path, errors, "world.monster_pressure", "low, normal or high",
                           &parse_monster_pressure, out.monster_pressure);
        } else if (key == "divine_activity") {
            parse_enum_key(value, path, errors, "world.divine_activity", "distant, normal or meddling",
                           &parse_divine_activity, out.divine_activity);
        } else if (key == "divine_script") {
            parse_divine_script(value, path, errors);
        } else {
            errors.add(path, value.position,
                       "unknown key " + detail::in_quotes(detail::child_key("world", key)) +
                           "; expected seed, seed_phrase, map_size, history_span, history_detail, civs_per_race, "
                           "races, event_intensity, monster_pressure, divine_activity or divine_script");
        }
    }
    if (seed != nullptr && phrase != nullptr) {
        errors.add(path, phrase->position, "'world.seed' and 'world.seed_phrase' are both given; give at most one");
    }
}

// --- Enum names ----------------------------------------------------------------------

template <typename E>
struct EnumName {
    E value;
    std::string_view name;
};

constexpr EnumName<HistoryDetail> HISTORY_DETAILS[] = {
    {HistoryDetail::sparse, "sparse"}, {HistoryDetail::normal, "normal"}, {HistoryDetail::dense, "dense"}};
constexpr EnumName<EventIntensity> EVENT_INTENSITIES[] = {
    {EventIntensity::calm, "calm"}, {EventIntensity::normal, "normal"}, {EventIntensity::turbulent, "turbulent"}};
constexpr EnumName<MonsterPressure> MONSTER_PRESSURES[] = {
    {MonsterPressure::low, "low"}, {MonsterPressure::normal, "normal"}, {MonsterPressure::high, "high"}};
constexpr EnumName<DivineActivity> DIVINE_ACTIVITIES[] = {
    {DivineActivity::distant, "distant"}, {DivineActivity::normal, "normal"}, {DivineActivity::meddling, "meddling"}};

template <typename E, std::size_t N>
std::string_view name_of(const EnumName<E> (&table)[N], E v) noexcept {
    for (const EnumName<E>& e : table) {
        if (e.value == v) {
            return e.name;
        }
    }
    return {};  // not reached: every enumerator is in its table
}

template <typename E, std::size_t N>
std::optional<E> value_of(const EnumName<E> (&table)[N], std::string_view text) noexcept {
    for (const EnumName<E>& e : table) {
        if (e.name == text) {
            return e.value;
        }
    }
    return std::nullopt;
}

}  // namespace

std::string_view history_detail_name(HistoryDetail v) noexcept {
    return name_of(HISTORY_DETAILS, v);
}
std::string_view event_intensity_name(EventIntensity v) noexcept {
    return name_of(EVENT_INTENSITIES, v);
}
std::string_view monster_pressure_name(MonsterPressure v) noexcept {
    return name_of(MONSTER_PRESSURES, v);
}
std::string_view divine_activity_name(DivineActivity v) noexcept {
    return name_of(DIVINE_ACTIVITIES, v);
}
std::optional<HistoryDetail> parse_history_detail(std::string_view text) noexcept {
    return value_of(HISTORY_DETAILS, text);
}
std::optional<EventIntensity> parse_event_intensity(std::string_view text) noexcept {
    return value_of(EVENT_INTENSITIES, text);
}
std::optional<MonsterPressure> parse_monster_pressure(std::string_view text) noexcept {
    return value_of(MONSTER_PRESSURES, text);
}
std::optional<DivineActivity> parse_divine_activity(std::string_view text) noexcept {
    return value_of(DIVINE_ACTIVITIES, text);
}

bool operator==(const Settings& a, const Settings& b) noexcept {
    // races_origins is where the value was written, not part of it.
    return a.seed == b.seed && a.seed_phrase == b.seed_phrase && a.map_size == b.map_size &&
           a.history_span == b.history_span && a.history_detail == b.history_detail &&
           a.civs_per_race == b.civs_per_race && a.races == b.races && a.event_intensity == b.event_intensity &&
           a.monster_pressure == b.monster_pressure && a.divine_activity == b.divine_activity &&
           a.divine_script == b.divine_script;
}

bool is_valid_history_span(std::int64_t years) noexcept {
    return years >= MIN_HISTORY_SPAN && years <= MAX_HISTORY_SPAN;
}

RacesText parse_races_text(std::string_view text) {
    RacesText out;
    if (text == "all") {
        out.ok = true;
        return out;
    }
    std::vector<std::string> ids;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = text.find(',', start);
        const std::string_view item =
            text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        if (!is_valid_id(item)) {
            return out;  // empty list, empty item or not an id
        }
        for (const std::string& id : ids) {
            if (id == item) {
                return out;  // repeated
            }
        }
        ids.emplace_back(item);
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    out.ok = true;
    out.races = std::move(ids);
    return out;
}

std::vector<std::string> race_ids_in_order(const ContentSet& content) {
    std::vector<std::pair<std::int64_t, std::string>> keyed;
    if (const KindContent* races = content.find_kind(RACE_KIND)) {
        for (const Definition& d : races->definitions) {
            keyed.emplace_back(d.record.get_int("order"), d.id);
        }
    }
    // Definitions are in ascending id order already; a stable sort by order
    // keeps ties in id order.
    std::stable_sort(keyed.begin(), keyed.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> ids;
    ids.reserve(keyed.size());
    for (auto& [order, id] : keyed) {
        ids.push_back(std::move(id));
    }
    return ids;
}

std::vector<LoadError> check_settings_races(const Settings& settings, const ContentSet& content) {
    std::vector<LoadError> out;
    if (!settings.races || settings.races_origins.size() != settings.races->size()) {
        return out;
    }
    const std::vector<std::string> known = race_ids_in_order(content);
    std::string expected;
    for (const std::string& id : known) {
        expected += expected.empty() ? "" : ", ";
        expected += id;
    }
    const std::vector<std::string>& races = *settings.races;
    for (std::size_t i = 0; i < races.size(); ++i) {
        if (std::find(known.begin(), known.end(), races[i]) != known.end()) {
            continue;
        }
        const ValueOrigin& at = settings.races_origins[i];
        out.push_back(LoadError{at.path, at.line, at.column,
                                "'" + detail::element_key("world.races", static_cast<std::uint64_t>(i)) + "' is \"" +
                                    races[i] + "\", which is not a race id; expected one of " + expected});
    }
    sort_errors(out);
    return out;
}

void resolve_races(Settings& settings, const ContentSet& content) {
    std::vector<std::string> ordered = race_ids_in_order(content);
    if (settings.races) {
        const std::vector<std::string>& given = *settings.races;
        for (const std::string& id : given) {
            if (std::find(ordered.begin(), ordered.end(), id) == ordered.end()) {
                fatal("resolve_races: a race id names no race definition");
            }
        }
        std::vector<std::string> kept;
        for (std::string& id : ordered) {
            if (std::find(given.begin(), given.end(), id) != given.end()) {
                kept.push_back(std::move(id));
            }
        }
        ordered = std::move(kept);
    }
    settings.races = std::move(ordered);
    settings.races_origins.clear();
}

std::optional<std::uint64_t> parse_seed_text(std::string_view text) noexcept {
    if (text.size() >= 2 && text[0] == '0' && text[1] == 'x') {
        const std::string_view digits = text.substr(2);
        if (digits.empty() || digits.size() > 16) {
            return std::nullopt;
        }
        std::uint64_t v = 0;
        for (const char c : digits) {
            const std::uint32_t d = hex_digit(c);
            if (d >= 16U) {
                return std::nullopt;
            }
            v = (v << 4U) | d;  // at most 16 digits: no overflow
        }
        return v;
    }
    if (text.empty() || (text.size() > 1 && text[0] == '0')) {
        return std::nullopt;
    }
    constexpr std::uint64_t max = 18446744073709551615ULL;
    std::uint64_t v = 0;
    for (const char c : text) {
        if (!detail::is_digit(c)) {
            return std::nullopt;
        }
        const auto d = static_cast<std::uint64_t>(c - '0');
        if (v > (max - d) / 10U) {
            return std::nullopt;  // v * 10 + d would exceed 2^64 - 1
        }
        v = v * 10U + d;
    }
    return v;
}

bool is_valid_map_size(std::int64_t cells) noexcept {
    return cells == 512 || cells == 1024 || cells == 2048;
}

LoadResult<Settings> parse_settings(std::string_view display_path, std::string_view bytes) {
    ErrorList errors;
    Settings settings;
    Node root;
    if (detail::parse_file(display_path, bytes, errors, root)) {
        for (std::size_t i = 0; i < root.keys.size(); ++i) {
            const std::string& key = root.keys[i];
            const Node& value = root.children[i];
            if (key != "world") {
                errors.add(display_path, value.position,
                           "unknown key " + detail::in_quotes(key) + "; a settings file holds only the table [world]");
            } else if (value.kind != NodeKind::table) {
                errors.add(display_path, value.position,
                           "'world' is " + std::string(detail::kind_phrase(value)) + "; expected a table");
            } else {
                parse_world(value, display_path, errors, settings);
            }
        }
    }
    LoadResult<Settings> result;
    if (errors.empty()) {
        result.value = std::move(settings);
    } else {
        result.errors = errors.take_sorted();
    }
    return result;
}

LoadResult<Settings> load_settings_file(const std::filesystem::path& path) {
    const std::string display = detail::path_utf8(path);
    ErrorList errors;
    const std::optional<std::string> bytes = detail::read_file(path, display, errors);
    if (!bytes) {
        LoadResult<Settings> result;
        result.errors = errors.take_sorted();
        return result;
    }
    return parse_settings(display, *bytes);
}

}  // namespace worldgen::data
