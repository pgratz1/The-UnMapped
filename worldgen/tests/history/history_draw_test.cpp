// Keyed draws and their helpers (spec P3-02 4.1; P3-00 4.6): TAG_HIST, draw,
// pick, chance, weighted. Spec 9.1 items 1 and 2, golden vectors of 9.3
// (lines tag, stream, draw, pick, chance, weighted of
// worldgen/tests/golden/p3-02-names.txt, written by
// reference/p3_names_reference.py), helper edge cases, exact bracket
// properties, and distribution sanity with integer bounds on fixed keys.
// The library is reached only through p3_02_names_api.hpp.

#include <doctest/doctest.h>

#include <array>
#include <climits>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"

using names_test::golden_lines;
using names_test::hex;
using names_test::HALF_RAW;
using names_test::M64;
using names_test::ONE_RAW;
using names_test::split;

namespace {

std::vector<std::int64_t> weights_of(const std::string& s) {
    std::vector<std::int64_t> w;
    if (s == "-") return w;
    std::size_t pos = 0;
    while (pos <= s.size()) {
        const std::size_t comma = s.find(',', pos);
        const std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        w.push_back(std::stoll(tok));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return w;
}

// The smallest high word h with pick((h << 32), n) >= i: ceil(i * 2^32 / n).
std::uint64_t pick_start(std::uint64_t i, std::uint64_t n) { return ((i << 32) + n - 1) / n; }

}  // namespace

TEST_SUITE("history-draw") {

TEST_CASE("P3-02 4.1, 9.1 test 1: TAG_HIST is tag_value(\"UNM:HIST\") and the golden tag line") {
    // P0-01 7.2's packing: byte i of the ASCII text at bits 8i.
    const std::string text = "UNM:HIST";
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(static_cast<unsigned char>(text[i])) << (8 * i);
    }
    CHECK(v == 0x545349483A4D4E55ULL);
    CHECK(names_api::tag_hist() == v);
    // It differs from every P0-01 tag.
    CHECK(names_api::tag_hist() != worldgen::TAG_STREAM);
    CHECK(names_api::tag_hist() != worldgen::TAG_SUB);
    CHECK(names_api::tag_hist() != worldgen::TAG_CELL2);
    CHECK(names_api::tag_hist() != worldgen::TAG_CELL3);
    CHECK(names_api::tag_hist() != worldgen::TAG_PHRASE);
    const auto& lines = golden_lines("tag");
    REQUIRE(lines.size() == 1);
    const auto t = split(lines[0], 3);
    CHECK(t[1] == "TAG_HIST");
    CHECK(hex(t[2]) == v);
}

TEST_CASE("P3-02 4.5, 8: the three hist.names streams are registered and keyed as the reference says") {
    for (const std::string_view name : {std::string_view("hist.names.god"), std::string_view("hist.names.civilization"),
                                        std::string_view("hist.names.settlement")}) {
        bool found = false;
        for (const std::string_view s : worldgen::streams::all_stream_names) {
            found = found || s == name;
        }
        CHECK_MESSAGE(found, name);
    }
    // Revised 8: appended after geo.regions.cost in the order god, civilization, settlement;
    // P0-01 amendment 20 (P1-06 r4, generator 0.17.0) puts geo.regions.name
    // directly after geo.regions.cost, so the three follow it.
    const auto& all = worldgen::streams::all_stream_names;
    std::size_t at = all.size();
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i] == "geo.regions.cost") at = i;
    }
    REQUIRE(at + 4 < all.size());
    CHECK(all[at + 1] == "geo.regions.name");
    CHECK(all[at + 2] == "hist.names.god");
    CHECK(all[at + 3] == "hist.names.civilization");
    CHECK(all[at + 4] == "hist.names.settlement");
    const auto& lines = golden_lines("stream");
    REQUIRE(lines.size() == 15);   // P5-02 9.1: hist.names.monster and hist.names.site (0.23.0)
    for (const std::string& line : lines) {
        const auto t = split(line, 4);
        CAPTURE(line);
        CHECK(worldgen::stream_key(names_test::dec_u(t[1]), t[2]) == hex(t[3]));
    }
    // 4.6: S = stream_key(0, "hist.names.settlement").
    CHECK(worldgen::stream_key(0, "hist.names.settlement") == 0x969C418DDF3C5EDCULL);
}

TEST_CASE("P3-02 9.1 test 1: draw against the reference for 16 tuples") {
    const auto& lines = golden_lines("draw");
    REQUIRE(lines.size() == 16);
    for (const std::string& line : lines) {
        const auto t = split(line, 7);
        CAPTURE(line);
        CHECK(names_api::draw(hex(t[1]), hex(t[2]), hex(t[3]), hex(t[4]), hex(t[5])) == hex(t[6]));
    }
}

TEST_CASE("P3-02 4.1: draw is hash_words(TAG_HIST, [S, w0, a, b, k]) and every word matters") {
    const std::uint64_t s = worldgen::stream_key(0, "hist.names.settlement");
    // 4.6's table: u(j) = draw(S, 0, 2, 1, j).
    CHECK(names_api::draw(s, 0, 2, 1, 0) == 0xAB9A949FA699954CULL);
    CHECK(names_api::draw(s, 0, 2, 1, 6) == 0x7347EA42EAED2BB2ULL);
    const std::array<std::uint64_t, 5> base = {s, 0, 2, 1, 0};
    const std::uint64_t u0 = names_api::draw(base[0], base[1], base[2], base[3], base[4]);
    for (std::size_t i = 0; i < 5; ++i) {
        std::array<std::uint64_t, 5> w = base;
        w[i] ^= 1;
        CHECK(names_api::draw(w[0], w[1], w[2], w[3], w[4]) != u0);
    }
    // Pure: the same words, the same value, whatever was drawn in between.
    (void)names_api::draw(1, 2, 3, 4, 5);
    CHECK(names_api::draw(s, 0, 2, 1, 0) == u0);
}

TEST_CASE("P3-02 9.1 test 2: pick at n = 1, u = 0 and u = 2^64 - 1 with n = 6") {
    for (const std::uint64_t u : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{1} << 63, M64}) {
        CHECK(names_api::pick(u, 1) == 0);
    }
    CHECK(names_api::pick(0, 6) == 0);
    CHECK(names_api::pick(M64, 6) == 5);
    // n = 2^32 returns the high word itself.
    CHECK(names_api::pick(M64, std::uint64_t{1} << 32) == 0xFFFFFFFFULL);
    CHECK(names_api::pick(0x1234567800000000ULL, std::uint64_t{1} << 32) == 0x12345678ULL);
    // The low word never matters.
    CHECK(names_api::pick(0x00000000FFFFFFFFULL, 7) == 0);
    CHECK(names_api::pick(0xFFFFFFFF00000000ULL, 7) == 6);
}

TEST_CASE("P3-02 9 seams: pick is fatal for n = 0 and n > 2^32") {
    CHECK_THROWS_AS((void)names_api::pick(0, 0), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::pick(M64, 0), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::pick(0, (std::uint64_t{1} << 32) + 1), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::pick(0, M64), worldgen::FatalError);
    CHECK_NOTHROW((void)names_api::pick(0, std::uint64_t{1} << 32));
}

TEST_CASE("P3-02 4.1: pick golden vectors") {
    const auto& lines = golden_lines("pick");
    REQUIRE(lines.size() >= 10);
    for (const std::string& line : lines) {
        const auto t = split(line, 4);
        CAPTURE(line);
        if (t[3] == "FATAL") {
            CHECK_THROWS_AS((void)names_api::pick(hex(t[1]), names_test::dec_u(t[2])), worldgen::FatalError);
        } else {
            CHECK(names_api::pick(hex(t[1]), names_test::dec_u(t[2])) == names_test::dec_u(t[3]));
        }
    }
}

TEST_CASE("P3-02 4.1: pick splits the 2^32 high words into n runs of floor or ceil(2^32 / n), exactly") {
    // For each index i the first high word that gives i is ceil(i * 2^32 / n),
    // the one before it gives i - 1; so each index has floor or ceil(2^32/n)
    // high words (no modulo bias beyond 2^-32, P3-00 4.6).
    for (const std::uint64_t n : {std::uint64_t{2}, std::uint64_t{3}, std::uint64_t{5}, std::uint64_t{6},
                                  std::uint64_t{7}, std::uint64_t{13}, std::uint64_t{64}, std::uint64_t{1000},
                                  std::uint64_t{65537}, (std::uint64_t{1} << 32) - 1}) {
        CAPTURE(n);
        const std::uint64_t lo = (std::uint64_t{1} << 32) / n;
        const std::uint64_t hi = lo + (((std::uint64_t{1} << 32) % n) != 0 ? 1 : 0);
        const std::uint64_t step = n <= 64 ? 1 : n / 37;  // a sample of the indices for large n
        for (std::uint64_t i = 1; i < n; i += step) {
            const std::uint64_t h = pick_start(i, n);
            CHECK(names_api::pick(h << 32, n) == i);
            CHECK(names_api::pick(((h - 1) << 32) | 0xFFFFFFFFULL, n) == i - 1);
            const std::uint64_t len = (i + 1 < n ? pick_start(i + 1, n) : (std::uint64_t{1} << 32)) - h;
            CHECK(len >= lo);
            CHECK(len <= hi);
        }
    }
}

TEST_CASE("P3-02 9.1 test 2: chance at p = 0 (never), ONE (always), HALF at the boundary") {
    for (const std::uint64_t u : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{1} << 32, std::uint64_t{1} << 63, M64}) {
        CHECK_FALSE(names_api::chance(u, 0));
        CHECK(names_api::chance(u, ONE_RAW));
    }
    const std::uint64_t below = static_cast<std::uint64_t>(HALF_RAW - 1) << 32;
    const std::uint64_t at = static_cast<std::uint64_t>(HALF_RAW) << 32;
    CHECK(names_api::chance(below, HALF_RAW));
    CHECK(names_api::chance(below | 0xFFFFFFFFULL, HALF_RAW));  // the low word never matters
    CHECK_FALSE(names_api::chance(at, HALF_RAW));
    // The smallest positive p: true only for high word 0.
    CHECK(names_api::chance(0xFFFFFFFFULL, 1));
    CHECK_FALSE(names_api::chance(std::uint64_t{1} << 32, 1));
    // ONE - 1: false only for the largest high word.
    CHECK_FALSE(names_api::chance(M64, ONE_RAW - 1));
    CHECK(names_api::chance(0xFFFFFFFEFFFFFFFFULL, ONE_RAW - 1));
}

TEST_CASE("P3-02 9 seams: chance is fatal for p outside [0, ONE]") {
    CHECK_THROWS_AS((void)names_api::chance(0, -1), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::chance(0, ONE_RAW + 1), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::chance(M64, -ONE_RAW), worldgen::FatalError);
}

TEST_CASE("P3-02 4.1: chance golden vectors and its exact count") {
    for (const std::string& line : golden_lines("chance")) {
        const auto t = split(line, 4);
        CAPTURE(line);
        if (t[3] == "FATAL") {
            CHECK_THROWS_AS((void)names_api::chance(hex(t[1]), names_test::dec_i(t[2])), worldgen::FatalError);
        } else {
            CHECK(names_api::chance(hex(t[1]), names_test::dec_i(t[2])) == (t[3] == "true"));
        }
    }
    // Exactly p.raw high words give true: the boundary is h = p.raw.
    for (const std::int64_t p : {std::int64_t{1}, std::int64_t{12345}, HALF_RAW / 3, HALF_RAW, ONE_RAW - 2}) {
        const std::uint64_t h = static_cast<std::uint64_t>(p);
        CHECK(names_api::chance((h - 1) << 32, p));
        CHECK_FALSE(names_api::chance(h << 32, p));
    }
}

TEST_CASE("P3-02 9.1 test 2: weighted (0, 3, 1) at the boundary values of u") {
    const std::vector<std::int64_t> w = {0, 3, 1};
    // W = 4: index 1 while h * 4 < 2^32 * 3, i.e. h < 3 * 2^30; index 2 from there; never index 0.
    const std::uint64_t b = std::uint64_t{3} << 30;
    CHECK(names_api::weighted(0, w) == 1);
    CHECK(names_api::weighted(((b - 1) << 32) | 0xFFFFFFFFULL, w) == 1);
    CHECK(names_api::weighted(b << 32, w) == 2);
    CHECK(names_api::weighted(M64, w) == 2);
}

TEST_CASE("P3-02 4.1: weighted gives a zero weight no u, and a single positive weight every u") {
    const std::vector<std::int64_t> w = {0, 0, 7, 0};
    for (const std::uint64_t u : {std::uint64_t{0}, std::uint64_t{1} << 63, M64, std::uint64_t{0x123456789ABCDEF0ULL}}) {
        CHECK(names_api::weighted(u, w) == 2);
        CHECK(names_api::weighted(u, std::vector<std::int64_t>{5}) == 0);
    }
    // A trailing zero after the last positive weight is never chosen.
    CHECK(names_api::weighted(M64, std::vector<std::int64_t>{1, 0}) == 0);
    CHECK(names_api::weighted(0, std::vector<std::int64_t>{0, 1}) == 1);
    // Large weights: W = 2^63 - 1, the largest allowed sum; products in uint128.
    const std::vector<std::int64_t> big = {std::int64_t{1} << 62, (std::int64_t{1} << 62) - 2, 1};
    CHECK(names_api::weighted(0, big) == 0);
    // Index 1 starts at high word ceil(2^94 / (2^63 - 1)) = 2^31 + 1.
    CHECK(names_api::weighted(((std::uint64_t{1} << 31) << 32) | 0xFFFFFFFFULL, big) == 0);
    CHECK(names_api::weighted(((std::uint64_t{1} << 31) + 1) << 32, big) == 1);
    // The last weight's share 1 / (2^63 - 1) is below 2^-32: not reached by the largest high word.
    CHECK(names_api::weighted(M64, big) == 1);
}

TEST_CASE("P3-02 4.1 (revised): the sum may be up to 2^63 - 1; a sum above it is fatal") {
    constexpr std::int64_t MAX = INT64_MAX;
    // At the bound.
    CHECK(names_api::weighted(M64, std::vector<std::int64_t>{MAX}) == 0);
    CHECK(names_api::weighted(M64, std::vector<std::int64_t>{MAX, 0}) == 0);
    CHECK(names_api::weighted(0, std::vector<std::int64_t>{std::int64_t{1} << 62, (std::int64_t{1} << 62) - 1}) == 0);
    CHECK(names_api::weighted(M64, std::vector<std::int64_t>{std::int64_t{1} << 62, (std::int64_t{1} << 62) - 1}) == 1);
    // Above it, by one.
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{MAX, 1}), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{std::int64_t{1} << 62, std::int64_t{1} << 62}),
                    worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::weighted(M64, std::vector<std::int64_t>{std::int64_t{1} << 62, std::int64_t{1} << 62, 1}),
                    worldgen::FatalError);
    // Far above it (a sum that would wrap int64 twice).
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{MAX, MAX, MAX}), worldgen::FatalError);
}

TEST_CASE("P3-02 4.1 (revised): reachability of small shares") {
    // A share of at least 2^-32 is always reachable: the middle weight of
    // (2^31, 1, 2^31 - 1) has share exactly 2^-32 and is chosen at high word 2^31.
    const std::vector<std::int64_t> exact = {std::int64_t{1} << 31, 1, (std::int64_t{1} << 31) - 1};
    CHECK(names_api::weighted(std::uint64_t{1} << 63, exact) == 1);
    CHECK(names_api::weighted((std::uint64_t{1} << 63) - 1, exact) == 0);
    CHECK(names_api::weighted((std::uint64_t{1} << 63) + (std::uint64_t{1} << 32), exact) == 2);
    // A share below 2^-32 is chosen for at most one high word, possibly none:
    // (2^61, 1, 2^61), W = 2^62 + 1: index 1 needs 2^93 <= h * W < 2^93 + 2^32, a
    // range of h shorter than 1, here only h = ceil(2^93 / W) = 2^31.
    const std::vector<std::int64_t> tiny = {std::int64_t{1} << 61, 1, std::int64_t{1} << 61};
    const std::uint64_t h0 = std::uint64_t{1} << 31;
    int hits = 0;
    for (std::uint64_t h = h0 - 3; h <= h0 + 3; ++h) hits += names_api::weighted(h << 32, tiny) == 1 ? 1 : 0;
    CHECK(hits == 1);
    CHECK(names_api::weighted(h0 << 32, tiny) == 1);
    // With (2^61, 1, 2^61 - 2^32), W = 2^62 - 2^32 + 1, no high word gives index 1 at all:
    // ceil(2^93 / W) * W >= 2^93 + 2^32 (the candidate is 2^31 + 3, inside the window).
    const std::vector<std::int64_t> none = {std::int64_t{1} << 61, 1, (std::int64_t{1} << 61) - (std::int64_t{1} << 32)};
    hits = 0;
    for (std::uint64_t h = h0 - 3; h <= h0 + 8; ++h) hits += names_api::weighted(h << 32, none) == 1 ? 1 : 0;
    CHECK(hits == 0);
    // A positive w[0] is chosen at high word 0 whatever its share.
    CHECK(names_api::weighted(0, std::vector<std::int64_t>{1, std::int64_t{1} << 62}) == 0);
    CHECK(names_api::weighted(0x00000000FFFFFFFFULL, std::vector<std::int64_t>{1, std::int64_t{1} << 62}) == 0);
    CHECK(names_api::weighted(std::uint64_t{1} << 32, std::vector<std::int64_t>{1, std::int64_t{1} << 62}) == 1);
}

TEST_CASE("P3-02 9 seams: weighted is fatal for an all-zero list, an empty list and a negative weight") {
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{}), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{0}), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::weighted(M64, std::vector<std::int64_t>{0, 0, 0}), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{1, -1, 3}), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::weighted(0, std::vector<std::int64_t>{-1}), worldgen::FatalError);
}

TEST_CASE("P3-02 4.1: weighted golden vectors") {
    const auto& lines = golden_lines("weighted");
    REQUIRE(lines.size() >= 15);
    for (const std::string& line : lines) {
        const auto t = split(line, 4);
        CAPTURE(line);
        const std::vector<std::int64_t> w = weights_of(t[2]);
        if (t[3] == "FATAL") {
            CHECK_THROWS_AS((void)names_api::weighted(hex(t[1]), w), worldgen::FatalError);
        } else {
            CHECK(names_api::weighted(hex(t[1]), w) == names_test::dec_u(t[3]));
        }
    }
}

TEST_CASE("P3-02 4.1: weighted boundaries are exact: index i starts at ceil(2^32 * prefix(i-1) / W)") {
    const std::vector<std::vector<std::int64_t>> cases = {{1, 2, 3, 0, 4}, {1, 1, 1}, {5, 0, 0, 5}, {7}, {1, 999999}};
    for (const auto& w : cases) {
        std::int64_t total = 0;
        for (const std::int64_t x : w) total += x;
        std::uint64_t prefix = 0;
        for (std::size_t i = 0; i < w.size(); ++i) {
            CAPTURE(i);
            if (w[i] == 0) continue;  // never chosen; prefix unchanged
            const std::uint64_t start = ((prefix << 32) + static_cast<std::uint64_t>(total) - 1) / static_cast<std::uint64_t>(total);
            CHECK(names_api::weighted(start << 32, w) == i);
            if (start > 0) {
                const std::uint64_t before = names_api::weighted(((start - 1) << 32) | 0xFFFFFFFFULL, w);
                CHECK(before < i);
                CHECK(w[before] > 0);
            }
            prefix += static_cast<std::uint64_t>(w[i]);
        }
    }
}

TEST_CASE("P3-02 4.1: distribution sanity of pick, chance and weighted over 6000 fixed keyed draws") {
    // u_k = draw(stream_key(0, "hist.names.god"), 0, 7, 9, k), k = 0 .. 5999.
    // Expected counts 1000 per pick bin, 1500 for chance(1/4) and 600, 1200,
    // 1800, 0, 2400 for weights (1, 2, 3, 0, 4); the bounds are fixed
    // integers, about four standard deviations wide. The reference gives
    // pick 1043 973 1012 997 1000 975, chance 1525, weighted 626 1190 1813 0 2371.
    const std::uint64_t key = worldgen::stream_key(0, "hist.names.god");
    std::array<int, 6> p{};
    int c = 0;
    std::array<int, 5> w{};
    const std::vector<std::int64_t> wt = {1, 2, 3, 0, 4};
    for (std::uint64_t k = 0; k < 6000; ++k) {
        const std::uint64_t u = names_api::draw(key, 0, 7, 9, k);
        ++p[names_api::pick(u, 6)];
        c += names_api::chance(u, ONE_RAW / 4) ? 1 : 0;
        ++w[names_api::weighted(u, wt)];
    }
    CHECK(p == std::array<int, 6>{1043, 973, 1012, 997, 1000, 975});
    for (const int x : p) {
        CHECK(x >= 880);
        CHECK(x <= 1120);
    }
    CHECK(c == 1525);
    CHECK(c >= 1350);
    CHECK(c <= 1650);
    CHECK(w == std::array<int, 5>{626, 1190, 1813, 0, 2371});
    CHECK(w[3] == 0);
}

}  // TEST_SUITE
