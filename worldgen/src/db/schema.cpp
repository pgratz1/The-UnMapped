#include "worldgen/db/schema.hpp"

#include <charconv>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include "worldgen/core/hash.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/data/settings.hpp"

namespace worldgen::db {

namespace {

constexpr bool is_lower(char c) noexcept {
    return c >= 'a' && c <= 'z';
}

constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

// [a-z][a-z0-9_]* with no length limit.
bool is_segment(std::string_view s) noexcept {
    if (s.empty() || !is_lower(s.front())) {
        return false;
    }
    for (const char c : s) {
        if (!is_lower(c) && !is_digit(c) && c != '_') {
            return false;
        }
    }
    return true;
}

// "0" or [1-9][0-9]*.
bool is_canonical_unsigned_digits(std::string_view s) noexcept {
    if (s.empty()) {
        return false;
    }
    for (const char c : s) {
        if (!is_digit(c)) {
            return false;
        }
    }
    return s.size() == 1 || s.front() != '0';
}

}  // namespace

bool is_valid_meta_key(std::string_view key) noexcept {
    std::size_t start = 0;
    for (;;) {
        const std::size_t dot = key.find('.', start);
        const std::string_view segment =
            key.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        if (!is_segment(segment)) {
            return false;
        }
        if (dot == std::string_view::npos) {
            return true;
        }
        start = dot + 1;
    }
}

bool is_valid_stage_name(std::string_view name) noexcept {
    return name.size() <= 64 && is_segment(name);
}

bool is_valid_layer_name(std::string_view name) noexcept {
    return is_valid_stream_name(name);
}

std::string format_u64(std::uint64_t value) {
    char buffer[24];
    const std::to_chars_result r = std::to_chars(buffer, buffer + sizeof buffer, value);
    return std::string(buffer, r.ptr);
}

std::string format_i64(std::int64_t value) {
    char buffer[24];
    const std::to_chars_result r = std::to_chars(buffer, buffer + sizeof buffer, value);
    return std::string(buffer, r.ptr);
}

std::optional<std::uint64_t> parse_canonical_u64(std::string_view text) noexcept {
    if (!is_canonical_unsigned_digits(text)) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const std::from_chars_result r = std::from_chars(text.data(), text.data() + text.size(), value);
    if (r.ec != std::errc{} || r.ptr != text.data() + text.size()) {
        return std::nullopt;  // overflow
    }
    return value;
}

bool is_canonical_version(std::string_view text) noexcept {
    const std::size_t first = text.find('.');
    if (first == std::string_view::npos) {
        return false;
    }
    const std::size_t second = text.find('.', first + 1);
    if (second == std::string_view::npos) {
        return false;
    }
    return is_canonical_unsigned_digits(text.substr(0, first)) &&
           is_canonical_unsigned_digits(text.substr(first + 1, second - first - 1)) &&
           is_canonical_unsigned_digits(text.substr(second + 1));
}

bool is_history_meta_key(std::string_view key) noexcept {
    for (const std::string_view k : HISTORY_META_KEYS) {
        if (k == key) {
            return true;
        }
    }
    return false;
}

namespace {

bool is_one_of(std::string_view value, std::initializer_list<std::string_view> allowed) noexcept {
    for (const std::string_view a : allowed) {
        if (a == value) {
            return true;
        }
    }
    return false;
}

// param.races: one or more ids joined by ',', none twice (P3-03 4.2).
bool is_canonical_races(std::string_view value) {
    std::vector<std::string_view> seen;
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = value.find(',', start);
        const std::string_view id =
            value.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        if (!data::is_valid_id(id)) {
            return false;
        }
        for (const std::string_view earlier : seen) {
            if (earlier == id) {
                return false;
            }
        }
        seen.push_back(id);
        if (comma == std::string_view::npos) {
            return true;
        }
        start = comma + 1;
    }
}

}  // namespace

bool is_canonical_history_meta_value(std::string_view key, std::string_view value) {
    if (key == META_PARAM_CIVS_PER_RACE) {
        return is_one_of(value, {"auto", "1", "2", "3", "4"});
    }
    if (key == META_PARAM_DIVINE_ACTIVITY) {
        return data::parse_divine_activity(value).has_value();
    }
    if (key == META_PARAM_DIVINE_SCRIPT) {
        if (value.empty()) {
            return true;
        }
        if (value.size() != 64) {
            return false;
        }
        for (const char c : value) {
            if (!is_digit(c) && !(c >= 'a' && c <= 'f')) {
                return false;
            }
        }
        return true;
    }
    if (key == META_PARAM_EVENT_INTENSITY) {
        return data::parse_event_intensity(value).has_value();
    }
    if (key == META_PARAM_HISTORY_DETAIL) {
        return data::parse_history_detail(value).has_value();
    }
    if (key == META_PARAM_HISTORY_SPAN) {
        const std::optional<std::uint64_t> v = parse_canonical_u64(value);
        return v && *v >= static_cast<std::uint64_t>(data::MIN_HISTORY_SPAN) &&
               *v <= static_cast<std::uint64_t>(data::MAX_HISTORY_SPAN);
    }
    if (key == META_PARAM_MONSTER_PRESSURE) {
        return data::parse_monster_pressure(value).has_value();
    }
    if (key == META_PARAM_RACES) {
        return is_canonical_races(value);
    }
    return false;
}

}  // namespace worldgen::db
