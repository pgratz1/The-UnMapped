// Spec P0-01 section 13.5: mixing primitives, byte packing, stream keys,
// location hashes, text phrases and the stream-name registry (section 7).

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

TEST_CASE("P0-01 13.5: mix64 and absorb") {
    CHECK(GOLDEN == 0x9E3779B97F4A7C15ULL);
    CHECK(mix64(0) == 0);
    CHECK(absorb(0, 0) == 0xE220A8397B1DCDAFULL);
    CHECK(absorb(0, GOLDEN) == 0x6E789E6AA1B965F4ULL);
    // absorb(h, w) = mix64((h ^ w) + GOLDEN).
    CHECK(absorb(0x1234, 0xFF00) == mix64((0x1234ULL ^ 0xFF00ULL) + GOLDEN));
}

TEST_CASE("P0-01 7.1: hash_words is the absorb chain followed by the length") {
    const std::vector<std::uint64_t> w = {3, 1, 4, 1, 5};
    std::uint64_t h = TAG_CELL2;
    for (const std::uint64_t x : w) {
        h = absorb(h, x);
    }
    h = absorb(h, w.size());
    CHECK(hash_words(TAG_CELL2, w) == h);
    CHECK(hash_words(TAG_CELL2, {3, 1, 4, 1, 5}) == h);
    CHECK(hash_words(0, std::span<const std::uint64_t>{}) == absorb(0, 0));
}

TEST_CASE("P0-01 7.2: domain tags are the ASCII names packed little-endian") {
    auto pack8 = [](std::string_view s) {
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(static_cast<unsigned char>(s[i])) << (8 * i);
        }
        return v;
    };
    CHECK(TAG_STREAM == pack8("UNM:STRM"));
    CHECK(TAG_SUB == pack8("UNM:SUBS"));
    CHECK(TAG_CELL2 == pack8("UNM:CEL2"));
    CHECK(TAG_CELL3 == pack8("UNM:CEL3"));
    CHECK(TAG_PHRASE == pack8("UNM:PHRS"));
    CHECK(TAG_STREAM == 0x4D5254533A4D4E55ULL);
    CHECK(TAG_SUB == 0x534255533A4D4E55ULL);
    CHECK(TAG_CELL2 == 0x324C45433A4D4E55ULL);
    CHECK(TAG_CELL3 == 0x334C45433A4D4E55ULL);
    CHECK(TAG_PHRASE == 0x535248503A4D4E55ULL);
}

TEST_CASE("P0-01 13.5: byte packing") {
    CHECK(encode_bytes("test.noise") ==
          std::vector<std::uint64_t>{10, 0x696F6E2E74736574ULL, 0x0000000000006573ULL});
    CHECK(encode_bytes("The Unmapped") ==
          std::vector<std::uint64_t>{12, 0x616D6E5520656854ULL, 0x0000000064657070ULL});
    CHECK(encode_bytes("") == std::vector<std::uint64_t>{0});
    CHECK(encode_bytes("12345678") == std::vector<std::uint64_t>{8, 0x3837363534333231ULL});
    const std::vector<std::uint8_t> bytes = {0xFF, 0x00, 0x80};
    CHECK(encode_bytes(std::span<const std::uint8_t>(bytes)) == std::vector<std::uint64_t>{3, 0x8000FFULL});
    CHECK(encode_i64(-1) == U64_MAX);
    CHECK(encode_i64(I64_MIN) == 0x8000000000000000ULL);
}

TEST_CASE("P0-01 13.5: stream_key is hash_words(TAG_STREAM, [seed, len, packed...])") {
    const std::vector<std::uint64_t> seeds = {0, 1, 0xFFFFFFFFFFFFFFFFULL, 0x0123456789ABCDEFULL};
    const std::vector<std::string_view> names = {"test.noise", "geo.elevation", "a", "geo.elevation.continent",
                                                 "a_1.b2_.c"};
    for (const std::uint64_t seed : seeds) {
        for (const std::string_view name : names) {
            std::vector<std::uint64_t> words = {seed};
            for (const std::uint64_t w : encode_bytes(name)) {
                words.push_back(w);
            }
            INFO("seed " << seed << " name " << std::string(name));
            CHECK(stream_key(seed, name) == hash_words(TAG_STREAM, words));
        }
    }
    // Explicit words for one case, not through encode_bytes.
    CHECK(stream_key(0, "test.noise") ==
          hash_words(TAG_STREAM, {0, 10, 0x696F6E2E74736574ULL, 0x0000000000006573ULL}));
}

TEST_CASE("P0-01 13.5: stream_key rejects names outside the grammar") {
    const std::string sixty_four(64, 'a');
    const std::string sixty_five(65, 'a');
    CHECK(is_valid_stream_name(sixty_four));
    static_cast<void>(stream_key(0, sixty_four));
    for (const std::string_view bad :
         {std::string_view{}, std::string_view{"A"}, std::string_view{"1a"}, std::string_view{"_a"},
          std::string_view{"a."}, std::string_view{".a"}, std::string_view{"a..b"}, std::string_view{"a.1"},
          std::string_view{"a-b"}, std::string_view{"a b"}, std::string_view{"geo.Elevation"},
          std::string_view{sixty_five}, std::string_view{"a._b"}, std::string_view{"\xC3\xA9"}}) {
        INFO("name \"" << std::string(bad) << "\"");
        CHECK_FALSE(is_valid_stream_name(bad));
        CHECK_FATAL(stream_key(0, bad));
    }
    for (const std::string_view good : {"a", "z9", "a_", "a.b", "a_b.c_d.e0", "test.hash"}) {
        INFO("name \"" << std::string(good) << "\"");
        CHECK(is_valid_stream_name(good));
    }
}

TEST_CASE("P0-01 7.3: substream, hash_at, hash_at3 structure") {
    CHECK(substream(1, 0) == hash_words(TAG_SUB, {1, 0}));
    CHECK(hash_at(1, -1, 2) == hash_words(TAG_CELL2, {1, U64_MAX, 2}));
    CHECK(hash_at3(1, 0, 0, 0) == hash_words(TAG_CELL3, {1, 0, 0, 0}));
    CHECK(hash_at3(7, I64_MIN, -2, I64_MAX) ==
          hash_words(TAG_CELL3, {7, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFEULL, 0x7FFFFFFFFFFFFFFFULL}));
    CHECK(phrase_seed("The Unmapped") == hash_words(TAG_PHRASE, encode_bytes("The Unmapped")));
}

TEST_CASE("P0-01 13.5: the six named hash values (golden, from the reference)") {
    // Values from worldgen/tests/golden/core_vectors.txt (independent reference).
    CHECK(stream_key(0, "test.noise") == 0x9e5e770cda7efd61ULL);
    CHECK(stream_key(0xFFFFFFFFFFFFFFFFULL, "geo.elevation") == 0x8532d0a6e80cb6f7ULL);
    CHECK(substream(1, 0) == 0x85b25d58fd2d1114ULL);
    CHECK(hash_at(1, -1, 2) == 0xe2201850a63a2021ULL);
    CHECK(hash_at3(1, 0, 0, 0) == 0xa5609bda3d71cd4cULL);
    CHECK(phrase_seed("The Unmapped") == 0x71875b1752d5abb2ULL);
}

// --- Text phrases (P0-01 7.3 "Text phrases", 13.5) ---------------------------

namespace {

// Fixed key for the phrase property test ("1HRPTSET"); seeds Rng directly.
constexpr std::uint64_t KEY_PHRASE_PROPS = 0x5445535450524831ULL;

// The scrub of P0-01 7.3 written out step by step from the spec, as an
// oracle for the property tests (not a copy of the implementation).
std::string spec_scrub(std::string_view text) {
    // Steps 1 and 2: byte by byte.
    std::string mapped;
    for (const char ch : text) {
        auto b = static_cast<std::uint8_t>(ch);
        if (b >= 0x41 && b <= 0x5A) {
            b = static_cast<std::uint8_t>(b + 0x20);
        }
        const bool keep = (b >= 0x61 && b <= 0x7A) || (b >= 0x30 && b <= 0x39);
        mapped.push_back(keep ? static_cast<char>(b) : ' ');
    }
    // Step 3: runs of spaces become one space.
    std::string collapsed;
    for (const char c : mapped) {
        if (c == ' ' && !collapsed.empty() && collapsed.back() == ' ') {
            continue;
        }
        collapsed.push_back(c);
    }
    // Step 4: one leading and one trailing space removed.
    if (!collapsed.empty() && collapsed.front() == ' ') {
        collapsed.erase(0, 1);
    }
    if (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }
    return collapsed;
}

// The output alphabet of 7.3: only a-z, 0-9 and single interior spaces.
bool is_scrubbed_form(std::string_view s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto b = static_cast<std::uint8_t>(s[i]);
        const bool alnum = (b >= 0x61 && b <= 0x7A) || (b >= 0x30 && b <= 0x39);
        if (alnum) {
            continue;
        }
        if (b != 0x20 || i == 0 || i + 1 == s.size() || s[i - 1] == ' ') {
            return false;
        }
    }
    return true;
}

std::string bytes_of(std::initializer_list<std::uint8_t> bytes) {
    std::string out;
    for (const std::uint8_t b : bytes) {
        out.push_back(static_cast<char>(b));
    }
    return out;
}

void check_phrase(std::string_view text, std::string_view expected) {
    INFO("phrase of " << text.size() << " bytes, expected \"" << std::string(expected) << "\"");
    const PhraseSeed p = seed_from_phrase(text);
    REQUIRE(p.ok());
    CHECK(p.error == PhraseError::none);
    CHECK(p.phrase == expected);
    CHECK(p.seed == phrase_seed(expected));
    CHECK(scrub_phrase(text) == expected);
}

void check_phrase_error(std::string_view text, PhraseError e) {
    INFO("phrase of " << text.size() << " bytes");
    const PhraseSeed p = seed_from_phrase(text);
    CHECK_FALSE(p.ok());
    CHECK(p.error == e);
    CHECK(p.seed == 0);
    CHECK(p.phrase.empty());
}

}  // namespace

TEST_CASE("P0-01 13.5: seed_from_phrase scrubs: the examples of 7.3 and 13.5") {
    CHECK(PHRASE_MAX_BYTES == 256);

    // Five spellings of one world.
    const PhraseSeed base = seed_from_phrase("ashen marches");
    REQUIRE(base.ok());
    CHECK(base.phrase == "ashen marches");
    CHECK(base.seed == phrase_seed("ashen marches"));
    // Value from worldgen/tests/golden/core_vectors.txt (independent reference).
    CHECK(base.seed == 0xdfd13228861bdf55ULL);
    for (const std::string_view spelling :
         {std::string_view{"Ashen Marches!"}, std::string_view{"ashen-marches"}, std::string_view{"ASHEN  MARCHES"},
          std::string_view{"  ashen\tmarches\n"}, std::string_view{"ashen marches"}}) {
        INFO("spelling \"" << std::string(spelling) << "\"");
        check_phrase(spelling, "ashen marches");
        CHECK(seed_from_phrase(spelling).seed == base.seed);
    }

    check_phrase("The Unmapped", "the unmapped");
    CHECK(seed_from_phrase("The Unmapped").seed != phrase_seed("The Unmapped"));
    check_phrase("a1B2", "a1b2");

    // "Ünmapped Wörld": every byte of a multi-byte UTF-8 character is a space.
    const std::string unmapped_world = "\xC3\x9Cnmapped W\xC3\xB6rld";
    check_phrase(unmapped_world, "nmapped w rld");
    CHECK(seed_from_phrase(unmapped_world).seed == seed_from_phrase("nmapped w rld").seed);

    // Bytes 0x00, 0x7F, 0xFF and an invalid UTF-8 sequence inside a phrase
    // each act as a space. NUL is a separator, never a terminator.
    check_phrase(bytes_of({'a', 0x00, 'b'}), "a b");
    check_phrase(bytes_of({'a', 0x7F, 'b'}), "a b");
    check_phrase(bytes_of({'a', 0xFF, 'b'}), "a b");
    check_phrase(bytes_of({'a', 0xC0, 0xAF, 'b'}), "a b");        // overlong '/'
    check_phrase(bytes_of({'a', 0xED, 0xA0, 0x80, 'b'}), "a b");  // surrogate U+D800
    check_phrase(bytes_of({'a', 0xC3, 'b'}), "a b");              // truncated sequence
    check_phrase(bytes_of({0x00, 'x', 0x00}), "x");

    // No normalization or accent folding.
    check_phrase("Caf\xC3\xA9", "caf");       // precomposed U+00E9
    check_phrase("Cafe\xCC\x81", "cafe");     // e + combining U+0301
    CHECK(seed_from_phrase("Caf\xC3\xA9").seed != seed_from_phrase("Cafe\xCC\x81").seed);

    // A non-Latin phrase keeps only its ASCII letters and digits.
    check_phrase("\xE6\x9D\xB1\xE4\xBA\xAC 2024", "2024");  // "東京 2024"

    // A phrase is never a number.
    check_phrase("42", "42");
    CHECK(seed_from_phrase("42").seed != 42);
    CHECK(seed_from_phrase("0").seed != 0);
}

TEST_CASE("P0-01 13.5: seed_from_phrase errors: empty and too long only") {
    // Scrubs to nothing.
    check_phrase_error("", PhraseError::empty);
    check_phrase_error("   ", PhraseError::empty);
    check_phrase_error(" \t\r\n", PhraseError::empty);  // all whitespace
    check_phrase_error("!!!", PhraseError::empty);      // all punctuation
    check_phrase_error("-_.,;:'\"?/\\|", PhraseError::empty);
    check_phrase_error(bytes_of({0x00}), PhraseError::empty);
    check_phrase_error(bytes_of({0x00, 0x7F, 0x00}), PhraseError::empty);
    check_phrase_error(bytes_of({0x80, 0xFF, 0xC3}), PhraseError::empty);  // only non-ASCII bytes
    check_phrase_error("\xE6\x9D\xB1\xE4\xBA\xAC", PhraseError::empty);     // "東京"
    check_phrase_error("\xD0\x9C\xD0\xB8\xD1\x80", PhraseError::empty);     // "Мир"
    check_phrase_error("\xF0\x9F\x97\xBA", PhraseError::empty);             // U+1F5FA
    check_phrase_error("\xFF", PhraseError::empty);  // invalid UTF-8 is not an error of its own

    // Scrubbed form of 257 bytes.
    check_phrase_error(std::string(257, 'a'), PhraseError::too_long);
    check_phrase_error(std::string(257, 'A'), PhraseError::too_long);
    check_phrase_error(std::string(100000, 'z'), PhraseError::too_long);
}

TEST_CASE("P0-01 13.5: the 256-byte limit applies to the scrubbed phrase") {
    const std::string a256(256, 'a');
    const PhraseSeed ok256 = seed_from_phrase(a256);
    REQUIRE(ok256.ok());
    CHECK(ok256.phrase == a256);
    CHECK(ok256.seed == phrase_seed(a256));
    check_phrase_error(std::string(257, 'a'), PhraseError::too_long);

    // Padding is removed before the length is counted.
    check_phrase(" " + a256 + "\n", a256);
    check_phrase(std::string(1000, '!') + a256 + std::string(1000, ' '), a256);

    // A scrubbed phrase with interior spaces of exactly 256 and 257 bytes, from
    // inputs that are much longer: "x" words separated by runs of separators.
    auto spaced = [](std::size_t words, std::string_view separator) {
        std::string text;
        for (std::size_t i = 0; i < words; ++i) {
            if (i > 0) {
                text += separator;
            }
            text += (i % 2 == 0) ? "X" : "y";
        }
        return text;
    };
    // n words give n + (n - 1) bytes: 128 words -> 255, 129 words -> 257.
    const std::string in255 = spaced(128, "--  \t");
    const std::string s255 = scrub_phrase(in255);
    CHECK(s255.size() == 255);
    CHECK(seed_from_phrase(in255).ok());
    const std::string in257 = spaced(129, "!?\n");
    CHECK(scrub_phrase(in257).size() == 257);
    CHECK(in257.size() == 129 + 128 * 3);
    check_phrase_error(in257, PhraseError::too_long);
    // 256: 128 words then one more digit glued to the last word.
    const std::string in256 = in255 + "7";
    REQUIRE(scrub_phrase(in256).size() == 256);
    check_phrase(in256, s255 + "7");
    check_phrase_error(in256 + "-8", PhraseError::too_long);  // 256 + " 8" = 258

    // The length of the text before scrubbing is not limited.
    std::string long_input(200000, '.');
    long_input.replace(100000, 5, "Short");
    check_phrase(long_input, "short");
}

TEST_CASE("P0-01 13.5: scrub_phrase on its own: no emptiness or length check") {
    CHECK(scrub_phrase("") == "");
    CHECK(scrub_phrase("!!!") == "");
    CHECK(scrub_phrase("   ") == "");
    CHECK(scrub_phrase(bytes_of({0x00, 0x00})) == "");
    CHECK(scrub_phrase(std::string(300, 'Q')) == std::string(300, 'q'));
    CHECK(scrub_phrase(std::string(5000, 'b')).size() == 5000);
    CHECK(scrub_phrase("  A  B  ") == "a b");
    CHECK(scrub_phrase("a\t\r\n\v\fb") == "a b");
    CHECK(scrub_phrase("Hello, World!") == "hello world");
    CHECK(scrub_phrase("x") == "x");
    CHECK(scrub_phrase(" x") == "x");
    CHECK(scrub_phrase("x ") == "x");
    CHECK(scrub_phrase("0123456789") == "0123456789");
    CHECK(scrub_phrase("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == "abcdefghijklmnopqrstuvwxyz");
    // The bytes just outside the kept ranges: '/' ':' '@' '[' '`' '{'.
    CHECK(scrub_phrase("a/b:c@d[e`f{g") == "a b c d e f g");

    // Every byte value between two letters (step 1 and 2 per byte).
    for (unsigned v = 0; v < 256; ++v) {
        const auto b = static_cast<std::uint8_t>(v);
        const std::string text = bytes_of({'q', b, 'q'});
        std::string expected;
        if (b >= 0x41 && b <= 0x5A) {
            expected = bytes_of({'q', static_cast<std::uint8_t>(b + 0x20), 'q'});
        } else if ((b >= 0x61 && b <= 0x7A) || (b >= 0x30 && b <= 0x39)) {
            expected = text;
        } else {
            expected = "q q";
        }
        INFO("byte " << v);
        CHECK(scrub_phrase(text) == expected);
    }
}

TEST_CASE("P0-01 13.5: scrubbing properties over random byte strings") {
    // Fixed stream, so the test is deterministic. Half of the strings draw
    // from all 256 byte values, half mostly from letters, digits and a few
    // separators, so that long words and long runs both occur.
    Rng rng(KEY_PHRASE_PROPS);
    static constexpr char common_bytes[] = "aZ09 -!\t\n\x00\xC3\xBF";  // includes a NUL
    const std::string_view common(common_bytes, sizeof(common_bytes) - 1);
    std::size_t ok_count = 0;
    std::size_t empty_count = 0;
    std::size_t long_count = 0;
    for (int i = 0; i < 2000; ++i) {
        const std::uint64_t len = (i % 25 == 0) ? 250 + rng.uniform_u64(400) : rng.uniform_u64(64);
        std::string text;
        for (std::uint64_t j = 0; j < len; ++j) {
            std::uint8_t b = 0;
            if (i % 2 == 0) {
                b = static_cast<std::uint8_t>(rng.uniform_u64(256));
            } else {
                const std::uint64_t pick = rng.uniform_u64(common.size() + 26);
                b = pick < common.size() ? static_cast<std::uint8_t>(common[static_cast<std::size_t>(pick)])
                                         : static_cast<std::uint8_t>('a' + (pick - common.size()));
            }
            text.push_back(static_cast<char>(b));
        }
        INFO("string " << i << " of " << len << " bytes");
        const std::string s = scrub_phrase(text);
        CHECK(s == spec_scrub(text));
        CHECK(is_scrubbed_form(s));
        CHECK(scrub_phrase(s) == s);  // idempotent
        const PhraseSeed p = seed_from_phrase(text);
        if (s.empty()) {
            ++empty_count;
            CHECK(p.error == PhraseError::empty);
        } else if (s.size() > PHRASE_MAX_BYTES) {
            ++long_count;
            CHECK(p.error == PhraseError::too_long);
        } else {
            ++ok_count;
            REQUIRE(p.ok());
            CHECK(p.phrase == s);
            CHECK(p.seed == phrase_seed(s));
            const PhraseSeed again = seed_from_phrase(s);
            REQUIRE(again.ok());
            CHECK(again.phrase == s);
            CHECK(again.seed == p.seed);
        }
        if (!p.ok()) {
            CHECK(p.phrase.empty());
            CHECK(p.seed == 0);
        }
    }
    // The stream exercises all three outcomes.
    CHECK(ok_count > 0);
    CHECK(empty_count > 0);
    CHECK(long_count > 0);
}

TEST_CASE("P0-01 13.5: phrase_seed hashes exactly the bytes given") {
    CHECK(phrase_seed(" x") != phrase_seed("x"));
    CHECK(phrase_seed("x ") != phrase_seed("x"));
    CHECK(phrase_seed("X") != phrase_seed("x"));
    // No scrubbing or validation either: invalid UTF-8 and empty input are hashed.
    CHECK(phrase_seed("") == hash_words(TAG_PHRASE, {0}));
    const std::vector<std::uint8_t> bad = {0xFF};
    CHECK(phrase_seed(std::span<const std::uint8_t>(bad)) == phrase_seed("\xFF"));
    CHECK(phrase_seed("\xFF") == hash_words(TAG_PHRASE, {1, 0xFF}));
}

TEST_CASE("P0-01 13.5: sensitivity of hash_at and stream_key") {
    Rng rng(KEY_HASH_PROPS);
    for (int i = 0; i < 10000; ++i) {
        const std::uint64_t k = rng.next();
        const std::int64_t x = rng.uniform_i64(-100000, 100000);
        std::int64_t y = rng.uniform_i64(-100000, 100000);
        if (y == x) {
            y = x + 1;
        }
        REQUIRE(hash_at(k, x, y) != hash_at(k, y, x));
        REQUIRE(stream_key(k, "a") != stream_key(k, "a.b"));
    }
    // Small fixed coordinates too, including the swap across zero.
    for (std::uint64_t k = 0; k < 64; ++k) {
        for (std::int64_t x = -4; x <= 4; ++x) {
            for (std::int64_t y = -4; y <= 4; ++y) {
                if (x != y) {
                    REQUIRE(hash_at(k, x, y) != hash_at(k, y, x));
                }
            }
        }
    }
}

TEST_CASE("P0-01 13.5: stream-name registry is valid and has no duplicates") {
    const auto& names = streams::all_stream_names;
    REQUIRE(names.size() >= 2);
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO("name \"" << std::string(names[i]) << "\"");
        CHECK(is_valid_stream_name(names[i]));
        for (std::size_t j = i + 1; j < names.size(); ++j) {
            CHECK(names[i] != names[j]);
        }
    }
    // The P0 names of 7.3 and the 13.3 test name are registered.
    auto listed = [&](std::string_view n) {
        for (const std::string_view s : names) {
            if (s == n) {
                return true;
            }
        }
        return false;
    };
    CHECK(listed("test.hash"));
    CHECK(listed("test.noise"));
    CHECK(listed("test.wide"));
    CHECK(streams::test_hash == "test.hash");
    CHECK(streams::test_noise == "test.noise");
    CHECK(streams::test_wide == "test.wide");
}
