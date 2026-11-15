// The world settings file (P0-03 section 6).

#include "worldgen/data/settings.hpp"

#include <string>
#include <utility>

#include "data/common.hpp"
#include "data/text.hpp"
#include "worldgen/core/hash.hpp"

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
    const PhraseSeed ps = seed_from_phrase(node.text);
    switch (ps.error) {
        case PhraseError::none:
            out.seed = ps.seed;
            out.seed_phrase = ps.phrase;
            return;
        case PhraseError::invalid_utf8:
            errors.add(path, node.position, "'world.seed_phrase' is not valid UTF-8");
            return;
        case PhraseError::empty:
            errors.add(path, node.position, "'world.seed_phrase' is empty after trimming whitespace");
            return;
        case PhraseError::too_long:
            errors.add(path, node.position, "'world.seed_phrase' is longer than 256 bytes after trimming whitespace");
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
        } else {
            errors.add(path, value.position,
                       "unknown key " + detail::in_quotes(detail::child_key("world", key)) +
                           "; expected seed, seed_phrase or map_size");
        }
    }
    if (seed != nullptr && phrase != nullptr) {
        errors.add(path, phrase->position, "'world.seed' and 'world.seed_phrase' are both given; give at most one");
    }
}

}  // namespace

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
