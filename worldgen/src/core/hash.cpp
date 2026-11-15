// Mixing primitives, encodings and derivations (P0-01 7).

#include "worldgen/core/hash.hpp"

#include <cstddef>
#include <utility>

#include "worldgen/core/error.hpp"

namespace worldgen {

namespace {

constexpr std::size_t kMaxStreamName = 64;

bool is_lower(char c) noexcept {
    return c >= 'a' && c <= 'z';
}

bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

constexpr std::uint8_t kSpace = 0x20U;

// Scrub steps 1 and 2 for one byte (P0-01 7.3): A-Z (0x41-0x5A) to a-z,
// a-z (0x61-0x7A) and 0-9 (0x30-0x39) kept, every other byte to a space.
// Classified on the unsigned byte value only: no char signedness, no
// locale, no <cctype>.
std::uint8_t scrub_byte(std::uint8_t b) noexcept {
    if (b >= 0x41U && b <= 0x5AU) {
        return static_cast<std::uint8_t>(b + 0x20U);
    }
    if ((b >= 0x61U && b <= 0x7AU) || (b >= 0x30U && b <= 0x39U)) {
        return b;
    }
    return kSpace;
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

std::string scrub_phrase(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool pending_space = false;  // a run of spaces seen since the last kept byte
    for (const char c : text) {
        const std::uint8_t s = scrub_byte(static_cast<std::uint8_t>(c));
        if (s == kSpace) {
            pending_space = true;
            continue;
        }
        if (pending_space && !out.empty()) {
            out.push_back(static_cast<char>(kSpace));  // one per interior run; none leading
        }
        pending_space = false;
        out.push_back(static_cast<char>(s));
    }
    return out;  // a trailing run is never written
}

PhraseSeed seed_from_phrase(std::string_view text) {
    PhraseSeed result;
    std::string scrubbed = scrub_phrase(text);
    if (scrubbed.empty()) {
        result.error = PhraseError::empty;
        return result;
    }
    if (scrubbed.size() > PHRASE_MAX_BYTES) {
        result.error = PhraseError::too_long;
        return result;
    }
    result.seed = phrase_seed(scrubbed);
    result.phrase = std::move(scrubbed);
    return result;
}

}  // namespace worldgen
