#pragma once

// Seed streams and hashing (spec P0-01 section 7): mixing primitives,
// encoding rules, domain tags, derivations, stream-name grammar and text
// phrase seeds.

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace worldgen {

// --- Mixing primitives (P0-01 7.1) ------------------------------------------

// 0x9E3779B97F4A7C15 (P0-01 7.1).
inline constexpr std::uint64_t GOLDEN = 0x9E3779B97F4A7C15ULL;

// SplitMix64 finalizer, a bijection on uint64 (P0-01 7.1).
[[nodiscard]] std::uint64_t mix64(std::uint64_t z) noexcept;

// mix64((h ^ w) + GOLDEN), mod 2^64 (P0-01 7.1).
[[nodiscard]] std::uint64_t absorb(std::uint64_t h, std::uint64_t w) noexcept;

// h = tag; h = absorb(h, w[i]) for each word in order; return absorb(h, n)
// with n the number of words (P0-01 7.1).
[[nodiscard]] std::uint64_t hash_words(std::uint64_t tag, std::span<const std::uint64_t> words) noexcept;
// Same, for a braced list: hash_words(tag, {a, b, c}).
[[nodiscard]] std::uint64_t hash_words(std::uint64_t tag, std::initializer_list<std::uint64_t> words) noexcept;

// --- Encoding rules (P0-01 7.2) ---------------------------------------------

// An int64 as its two's-complement bit pattern (P0-01 7.2). Smaller signed
// integers are sign-extended to int64 first by the caller.
[[nodiscard]] constexpr std::uint64_t encode_i64(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

// A byte string (P0-01 7.2): one word holding the length in bytes, then the
// bytes packed 8 per word little-endian (byte i goes to bits 8*(i mod 8) of
// word i/8), the last word zero-padded. "" encodes as [0].
[[nodiscard]] std::vector<std::uint64_t> encode_bytes(std::span<const std::uint8_t> bytes);
// Same, for text (each char reinterpreted as a uint8 byte).
[[nodiscard]] std::vector<std::uint64_t> encode_bytes(std::string_view text);

// --- Domain tags (P0-01 7.2) ------------------------------------------------

inline constexpr std::uint64_t TAG_STREAM = 0x4D5254533A4D4E55ULL;  // "UNM:STRM"
inline constexpr std::uint64_t TAG_SUB = 0x534255533A4D4E55ULL;     // "UNM:SUBS"
inline constexpr std::uint64_t TAG_CELL2 = 0x324C45433A4D4E55ULL;   // "UNM:CEL2"
inline constexpr std::uint64_t TAG_CELL3 = 0x334C45433A4D4E55ULL;   // "UNM:CEL3"
inline constexpr std::uint64_t TAG_PHRASE = 0x535248503A4D4E55ULL;  // "UNM:PHRS"

// --- Derivations (P0-01 7.3) ------------------------------------------------

// True if name matches [a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)* and is 1 to 64
// bytes (P0-01 7.3, stream names).
[[nodiscard]] bool is_valid_stream_name(std::string_view name) noexcept;

// hash_words(TAG_STREAM, [seed, len(name), packed name words...])
// (P0-01 7.3). Fatal if name is not a valid stream name.
[[nodiscard]] std::uint64_t stream_key(std::uint64_t seed, std::string_view name);

// hash_words(TAG_SUB, [key, index]) (P0-01 7.3).
[[nodiscard]] std::uint64_t substream(std::uint64_t key, std::uint64_t index) noexcept;

// hash_words(TAG_CELL2, [key, x, y]) with x, y encoded as int64 bit patterns
// (P0-01 7.3).
[[nodiscard]] std::uint64_t hash_at(std::uint64_t key, std::int64_t x, std::int64_t y) noexcept;

// hash_words(TAG_CELL3, [key, x, y, z]) (P0-01 7.3).
[[nodiscard]] std::uint64_t hash_at3(std::uint64_t key, std::int64_t x, std::int64_t y,
                                     std::int64_t z) noexcept;

// --- Text phrases (P0-01 7.3) -----------------------------------------------

// Why a text phrase was rejected (P0-01 7.3).
enum class PhraseError : std::uint8_t {
    none = 0,      // success
    invalid_utf8,  // not valid UTF-8 (strict: no overlong forms, no
                   // surrogates U+D800..U+DFFF, nothing above U+10FFFF)
    empty,         // nothing left after trimming
    too_long,      // more than 256 bytes after trimming
};

// Result of phrase_seed. When ok(), phrase holds the trimmed phrase (to be
// recorded in meta) and seed the derived world seed; otherwise phrase is
// empty and seed is 0.
struct PhraseSeed {
    std::string phrase;
    std::uint64_t seed = 0;
    PhraseError error = PhraseError::none;

    [[nodiscard]] bool ok() const noexcept { return error == PhraseError::none; }
};

// Text phrase to world seed (P0-01 7.3). Validates UTF-8, removes leading and
// trailing ASCII whitespace (space, tab, CR, LF) and nothing else, requires
// 1 to 256 bytes after trimming, then
//   seed = hash_words(TAG_PHRASE, encode_bytes(trimmed))
//        = hash_words(TAG_PHRASE, [len, packed words...]).
// The phrase is user data, so errors are returned, never thrown.
[[nodiscard]] PhraseSeed phrase_seed(std::string_view phrase);

}  // namespace worldgen
