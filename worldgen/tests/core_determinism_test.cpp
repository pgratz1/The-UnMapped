// Spec P0-01 sections 4, 7.3 and 11: determinism. Every function is a pure
// function of its arguments (and, for Rng, its state); results do not depend
// on call order across different keys. P0 code is single-threaded (4.2), so
// there is no thread-count test here.

#include <vector>

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

namespace {

std::vector<std::uint64_t> draws(std::uint64_t key, int n) {
    Rng rng(key);
    std::vector<std::uint64_t> out;
    for (int i = 0; i < n; ++i) {
        out.push_back(rng.next());
    }
    return out;
}

}  // namespace

TEST_CASE("P0-01 6.2: two generators from the same key give the same stream") {
    const std::uint64_t key = stream_key(0, streams::test_hash);
    CHECK(draws(key, 1000) == draws(key, 1000));
    CHECK(draws(key, 8) != draws(key ^ 1U, 8));
}

TEST_CASE("P0-01 11: interleaving generators with different keys does not change either stream") {
    const std::uint64_t ka = stream_key(1, streams::test_hash);
    const std::uint64_t kb = stream_key(1, streams::test_noise);
    const std::vector<std::uint64_t> a_alone = draws(ka, 500);
    const std::vector<std::uint64_t> b_alone = draws(kb, 500);
    Rng a(ka);
    Rng b(kb);
    std::vector<std::uint64_t> a_mixed;
    std::vector<std::uint64_t> b_mixed;
    // a gets one draw per turn; b gets one or two, in a different rhythm.
    for (int i = 0; i < 200; ++i) {
        b_mixed.push_back(b.next());
        if (i % 3 == 0) {
            b_mixed.push_back(b.next());
        }
        a_mixed.push_back(a.next());
    }
    REQUIRE(b_mixed.size() <= b_alone.size());
    CHECK(std::vector<std::uint64_t>(a_alone.begin(), a_alone.begin() + 200) == a_mixed);
    CHECK(std::vector<std::uint64_t>(b_alone.begin(), b_alone.begin() + static_cast<std::ptrdiff_t>(b_mixed.size())) ==
          b_mixed);
}

TEST_CASE("P0-01 7.3 / 11: location hashes and noise do not depend on evaluation order") {
    const std::uint64_t key = stream_key(42, streams::test_noise);
    const std::int64_t n = 48;
    std::vector<std::uint64_t> fwd_hash;
    std::vector<Fx> fwd_noise;
    std::vector<Fx> fwd_fbm;
    for (std::int64_t y = 0; y < n; ++y) {
        for (std::int64_t x = 0; x < n; ++x) {
            fwd_hash.push_back(hash_at(key, x - 7, y - 9));
            const Fx fx = Fx::from_ratio(x * 5 - 100, 11);
            const Fx fy = Fx::from_ratio(y * 3 - 50, 13);
            fwd_noise.push_back(noise2(key, fx, fy));
            fwd_fbm.push_back(fbm2(key, fx, fy, 5, Fx::from_ratio(1, 32), Fx::from_int(2), Fx::HALF));
        }
    }
    // Reverse order, column-major, with unrelated calls on another key mixed in.
    const std::uint64_t other = stream_key(43, streams::test_noise);
    for (std::int64_t x = n - 1; x >= 0; --x) {
        for (std::int64_t y = n - 1; y >= 0; --y) {
            static_cast<void>(noise2(other, Fx::from_int(x), Fx::from_ratio(y, 3)));
            const std::size_t idx = static_cast<std::size_t>(y * n + x);
            REQUIRE(hash_at(key, x - 7, y - 9) == fwd_hash[idx]);
            const Fx fx = Fx::from_ratio(x * 5 - 100, 11);
            const Fx fy = Fx::from_ratio(y * 3 - 50, 13);
            REQUIRE(noise2(key, fx, fy) == fwd_noise[idx]);
            REQUIRE(fbm2(key, fx, fy, 5, Fx::from_ratio(1, 32), Fx::from_int(2), Fx::HALF) == fwd_fbm[idx]);
        }
    }
}

TEST_CASE("P0-01 11: stream keys and phrase seeds are repeatable") {
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        REQUIRE(stream_key(seed, streams::test_hash) == stream_key(seed, streams::test_hash));
        REQUIRE(substream(seed, 3) == substream(seed, 3));
        REQUIRE(hash_at3(seed, -1, 2, -3) == hash_at3(seed, -1, 2, -3));
    }
    CHECK(seed_from_phrase("The Unmapped").seed == seed_from_phrase("The Unmapped").seed);
    CHECK(sha256_hex("The Unmapped") == sha256_hex("The Unmapped"));
}
