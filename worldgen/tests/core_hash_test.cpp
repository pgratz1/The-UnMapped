// Spec P0-01 section 13.5: mixing primitives, byte packing, stream keys,
// location hashes, text phrases and the stream-name registry (section 7).

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

TEST_CASE("P0-01 13.5: seed_from_phrase trims and validates") {
    const PhraseSeed base = seed_from_phrase("The Unmapped");
    REQUIRE(base.ok());
    CHECK(base.phrase == "The Unmapped");
    CHECK(base.seed == phrase_seed("The Unmapped"));

    const PhraseSeed padded = seed_from_phrase("  The Unmapped\n");
    REQUIRE(padded.ok());
    CHECK(padded.seed == base.seed);
    CHECK(padded.phrase == "The Unmapped");

    const PhraseSeed tabs = seed_from_phrase("\t\r\nThe Unmapped \t");
    REQUIRE(tabs.ok());
    CHECK(tabs.seed == base.seed);

    const PhraseSeed lower = seed_from_phrase("the unmapped");
    REQUIRE(lower.ok());
    CHECK(lower.seed != base.seed);

    // Only space, tab, CR, LF are trimmed: vertical tab is kept.
    const PhraseSeed vt = seed_from_phrase("\vThe Unmapped");
    REQUIRE(vt.ok());
    CHECK(vt.seed != base.seed);
    CHECK(vt.phrase == "\vThe Unmapped");

    // Interior whitespace is kept.
    const PhraseSeed inner = seed_from_phrase("The  Unmapped");
    REQUIRE(inner.ok());
    CHECK(inner.seed != base.seed);

    // NUL and other control characters inside are accepted.
    const std::string with_nul("a\0b", 3);
    const PhraseSeed nul = seed_from_phrase(with_nul);
    REQUIRE(nul.ok());
    CHECK(nul.phrase == with_nul);
    CHECK(nul.seed == phrase_seed(with_nul));

    // Valid multi-byte UTF-8 is accepted.
    CHECK(seed_from_phrase("\xC3\x9Cnmapped").ok());
    CHECK(seed_from_phrase("\xF0\x9F\x97\xBA").ok());      // U+1F5FA
    CHECK(seed_from_phrase("\xF4\x8F\xBF\xBF").ok());      // U+10FFFF
    CHECK(seed_from_phrase("\xED\x9F\xBF").ok());          // U+D7FF
    CHECK(seed_from_phrase("\xEE\x80\x80").ok());          // U+E000
}

TEST_CASE("P0-01 13.5: seed_from_phrase errors") {
    auto expect_error = [](std::string_view text, PhraseError e) {
        const PhraseSeed p = seed_from_phrase(text);
        INFO("phrase of " << text.size() << " bytes");
        CHECK_FALSE(p.ok());
        CHECK(p.error == e);
        CHECK(p.seed == 0);
        CHECK(p.phrase.empty());
    };
    expect_error("", PhraseError::empty);
    expect_error("   ", PhraseError::empty);
    expect_error(" \t\r\n", PhraseError::empty);
    expect_error("\xFF", PhraseError::invalid_utf8);
    expect_error("\xC0\xAF", PhraseError::invalid_utf8);          // overlong '/'
    expect_error("\xE0\x80\xAF", PhraseError::invalid_utf8);      // overlong 3-byte
    expect_error("\xF0\x80\x80\xAF", PhraseError::invalid_utf8);  // overlong 4-byte
    expect_error("\xED\xA0\x80", PhraseError::invalid_utf8);      // surrogate U+D800
    expect_error("\xED\xBF\xBF", PhraseError::invalid_utf8);      // surrogate U+DFFF
    expect_error("\xF4\x90\x80\x80", PhraseError::invalid_utf8);  // U+110000
    expect_error("\xF5\x80\x80\x80", PhraseError::invalid_utf8);
    expect_error("a\x80", PhraseError::invalid_utf8);             // stray continuation
    expect_error("\xC3", PhraseError::invalid_utf8);              // truncated
    expect_error("\xE2\x82", PhraseError::invalid_utf8);          // truncated
    expect_error("  \xC3 ", PhraseError::invalid_utf8);

    const std::string max_len(256, 'a');
    const PhraseSeed ok256 = seed_from_phrase(max_len);
    CHECK(ok256.ok());
    expect_error(std::string(257, 'a'), PhraseError::too_long);
    // Length is counted after trimming.
    CHECK(seed_from_phrase(" " + max_len + "\n").ok());
    CHECK(seed_from_phrase(" " + max_len + "\n").seed == ok256.seed);
}

TEST_CASE("P0-01 13.5: phrase_seed hashes exactly the bytes given") {
    CHECK(phrase_seed(" x") != phrase_seed("x"));
    CHECK(phrase_seed("x ") != phrase_seed("x"));
    // No validation either: invalid UTF-8 and empty input are hashed.
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
