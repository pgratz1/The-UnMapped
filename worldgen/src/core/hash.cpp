// Mixing primitives, encodings and derivations (P0-01 7).

#include "worldgen/core/hash.hpp"

#include <cstddef>

#include "worldgen/core/error.hpp"

namespace worldgen {

namespace {

constexpr std::size_t kMaxStreamName = 64;
constexpr std::size_t kMaxPhrase = 256;

bool is_lower(char c) noexcept {
    return c >= 'a' && c <= 'z';
}

bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

bool is_ascii_space(std::uint8_t b) noexcept {
    return b == 0x20 || b == 0x09 || b == 0x0D || b == 0x0A;
}

// Strict UTF-8 validation (RFC 3629): shortest form only, no surrogates,
// nothing above U+10FFFF.
bool is_valid_utf8(std::string_view text) noexcept {
    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        const auto b0 = static_cast<std::uint8_t>(text[i]);
        if (b0 < 0x80U) {
            ++i;
            continue;
        }
        std::size_t len = 0;
        std::uint8_t lo = 0x80U;  // allowed range of the second byte
        std::uint8_t hi = 0xBFU;
        if (b0 >= 0xC2U && b0 <= 0xDFU) {
            len = 2;
        } else if (b0 >= 0xE0U && b0 <= 0xEFU) {
            len = 3;
            if (b0 == 0xE0U) {
                lo = 0xA0U;  // no overlong 3-byte forms
            } else if (b0 == 0xEDU) {
                hi = 0x9FU;  // no surrogates
            }
        } else if (b0 >= 0xF0U && b0 <= 0xF4U) {
            len = 4;
            if (b0 == 0xF0U) {
                lo = 0x90U;  // no overlong 4-byte forms
            } else if (b0 == 0xF4U) {
                hi = 0x8FU;  // nothing above U+10FFFF
            }
        } else {
            return false;  // continuation byte, C0, C1 or F5..FF as a lead
        }
        if (n - i < len) {
            return false;
        }
        const auto b1 = static_cast<std::uint8_t>(text[i + 1]);
        if (b1 < lo || b1 > hi) {
            return false;
        }
        for (std::size_t k = 2; k < len; ++k) {
            const auto bk = static_cast<std::uint8_t>(text[i + k]);
            if (bk < 0x80U || bk > 0xBFU) {
                return false;
            }
        }
        i += len;
    }
    return true;
}

}  // namespace

std::uint64_t mix64(std::uint64_t z) noexcept {
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31U);
}

std::uint64_t absorb(std::uint64_t h, std::uint64_t w) noexcept {
    return mix64((h ^ w) + GOLDEN);
}

std::uint64_t hash_words(std::uint64_t tag, std::span<const std::uint64_t> words) noexcept {
    std::uint64_t h = tag;
    for (const std::uint64_t w : words) {
        h = absorb(h, w);
    }
    return absorb(h, static_cast<std::uint64_t>(words.size()));
}

std::uint64_t hash_words(std::uint64_t tag, std::initializer_list<std::uint64_t> words) noexcept {
    return hash_words(tag, std::span<const std::uint64_t>(words.begin(), words.size()));
}

std::vector<std::uint64_t> encode_bytes(std::span<const std::uint8_t> bytes) {
    const auto len = static_cast<std::uint64_t>(bytes.size());
    std::vector<std::uint64_t> out;
    out.reserve(1U + (bytes.size() + 7U) / 8U);
    out.push_back(len);
    std::uint64_t word = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto shift = static_cast<unsigned int>(8U * (i % 8U));
        word |= static_cast<std::uint64_t>(bytes[i]) << shift;
        if (i % 8U == 7U) {
            out.push_back(word);
            word = 0;
        }
    }
    if (bytes.size() % 8U != 0U) {
        out.push_back(word);
    }
    return out;
}

std::vector<std::uint64_t> encode_bytes(std::string_view text) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size());
    for (const char c : text) {
        bytes.push_back(static_cast<std::uint8_t>(c));
    }
    return encode_bytes(std::span<const std::uint8_t>(bytes));
}

bool is_valid_stream_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > kMaxStreamName) {
        return false;
    }
    bool at_segment_start = true;
    for (const char c : name) {
        if (at_segment_start) {
            if (!is_lower(c)) {
                return false;
            }
            at_segment_start = false;
        } else if (c == '.') {
            at_segment_start = true;
        } else if (!is_lower(c) && !is_digit(c) && c != '_') {
            return false;
        }
    }
    return !at_segment_start;  // no trailing '.'
}

std::uint64_t stream_key(std::uint64_t seed, std::string_view name) {
    if (!is_valid_stream_name(name)) {
        fatal("stream_key: invalid stream name");
    }
    std::vector<std::uint64_t> words;
    words.reserve(2U + name.size() / 8U + 1U);
    words.push_back(seed);
    const std::vector<std::uint64_t> packed = encode_bytes(name);
    words.insert(words.end(), packed.begin(), packed.end());
    return hash_words(TAG_STREAM, words);
}

std::uint64_t substream(std::uint64_t key, std::uint64_t index) noexcept {
    return hash_words(TAG_SUB, {key, index});
}

std::uint64_t hash_at(std::uint64_t key, std::int64_t x, std::int64_t y) noexcept {
    return hash_words(TAG_CELL2, {key, encode_i64(x), encode_i64(y)});
}

std::uint64_t hash_at3(std::uint64_t key, std::int64_t x, std::int64_t y, std::int64_t z) noexcept {
    return hash_words(TAG_CELL3, {key, encode_i64(x), encode_i64(y), encode_i64(z)});
}

std::uint64_t phrase_seed(std::span<const std::uint8_t> bytes) {
    return hash_words(TAG_PHRASE, encode_bytes(bytes));
}

std::uint64_t phrase_seed(std::string_view bytes) {
    return hash_words(TAG_PHRASE, encode_bytes(bytes));
}

PhraseSeed seed_from_phrase(std::string_view phrase) {
    PhraseSeed result;
    if (!is_valid_utf8(phrase)) {
        result.error = PhraseError::invalid_utf8;
        return result;
    }
    std::size_t begin = 0;
    std::size_t end = phrase.size();
    while (begin < end && is_ascii_space(static_cast<std::uint8_t>(phrase[begin]))) {
        ++begin;
    }
    while (end > begin && is_ascii_space(static_cast<std::uint8_t>(phrase[end - 1U]))) {
        --end;
    }
    const std::string_view trimmed = phrase.substr(begin, end - begin);
    if (trimmed.empty()) {
        result.error = PhraseError::empty;
        return result;
    }
    if (trimmed.size() > kMaxPhrase) {
        result.error = PhraseError::too_long;
        return result;
    }
    result.phrase = std::string(trimmed);
    result.seed = phrase_seed(trimmed);
    return result;
}

}  // namespace worldgen
