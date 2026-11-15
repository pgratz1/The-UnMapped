// The block-parallel facility: P0-01 4.2 as amended in section 15 (rules 3
// to 5), P1-00 section 8 (D1, D8, D10) and the run_blocks / map_blocks
// contract of worldgen/core/parallel.hpp; and the G6 row-block drivers
// for_each_row_block / map_row_blocks (P1-00 7.6).
//
// Written from the specs and the public headers only. No test depends on
// timing: where a test tries to provoke a particular interleaving (a higher
// block failing before a lower one), it does so with a bounded spin that
// only makes the interleaving likely; the assertion holds for every
// interleaving.

#include <doctest/doctest.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "core_test_util.hpp"
#include "worldgen/core/core.hpp"
#include "worldgen/geo/blocks.hpp"

namespace {

using worldgen::MAX_THREADS;

// A value that depends on the block index only, through a non-trivial
// computation (so a block mix-up is visible).
std::uint64_t block_value(std::uint32_t b) {
    std::uint64_t h = worldgen::mix64(0x50415241ULL + b);
    for (int i = 0; i < 50; ++i) {
        h = worldgen::mix64(h ^ static_cast<std::uint64_t>(i));
    }
    return h;
}

// Thread counts every comparison runs with: 1, 2, 5 (P1-00 D10), others,
// and the maximum.
const std::vector<std::uint32_t>& thread_counts() {
    static const std::vector<std::uint32_t> counts = {1U, 2U, 3U, 5U, 8U, 17U, 64U, MAX_THREADS};
    return counts;
}

const std::vector<std::uint32_t>& block_counts() {
    static const std::vector<std::uint32_t> counts = {0U, 1U, 2U, 3U, 7U, 16U, 33U, 100U};
    return counts;
}

// An exception type of the test, to check that the rethrown exception is
// the very one a block raised (type and content), not only FatalError.
struct BlockFailure {
    std::uint32_t block = 0;
};

// Spin until flag is set or a bounded number of yields has passed. Never an
// assertion: it only makes a particular order of events more likely.
void wait_a_little_for(const std::atomic<bool>& flag) {
    for (int i = 0; i < 20000 && !flag.load(std::memory_order_acquire); ++i) {
        std::this_thread::yield();
    }
}

}  // namespace

TEST_CASE("parallel P0-01 4.2: map_blocks results are identical at every thread count") {
    for (const std::uint32_t blocks : block_counts()) {
        std::vector<std::uint64_t> expected(blocks);
        for (std::uint32_t b = 0; b < blocks; ++b) {
            expected[b] = block_value(b);
        }
        for (const std::uint32_t threads : thread_counts()) {
            CAPTURE(blocks);
            CAPTURE(threads);
            const std::vector<std::uint64_t> got =
                worldgen::map_blocks<std::uint64_t>(blocks, threads, [](std::uint32_t b) { return block_value(b); });
            CHECK(got == expected);
        }
    }
}

TEST_CASE("parallel P0-01 4.2: run_blocks calls every block exactly once at every thread count") {
    for (const std::uint32_t blocks : block_counts()) {
        for (const std::uint32_t threads : thread_counts()) {
            CAPTURE(blocks);
            CAPTURE(threads);
            std::vector<std::atomic<std::uint32_t>> calls(blocks);
            for (auto& c : calls) {
                c.store(0U);
            }
            std::vector<std::uint64_t> out(blocks, 0U);
            std::atomic<bool> out_of_range{false};
            worldgen::run_blocks(blocks, threads, [&](std::uint32_t b) {
                if (b >= blocks) {
                    out_of_range.store(true);
                    return;
                }
                calls[b].fetch_add(1U);
                out[b] = block_value(b);  // each block writes only its own slot
            });
            CHECK_FALSE(out_of_range.load());
            for (std::uint32_t b = 0; b < blocks; ++b) {
                CHECK(calls[b].load() == 1U);
                CHECK(out[b] == block_value(b));
            }
        }
    }
}

TEST_CASE("parallel D10: more threads than blocks") {
    for (const std::uint32_t threads : {4U, 10U, 100U, MAX_THREADS}) {
        CAPTURE(threads);
        const auto got = worldgen::map_blocks<std::uint64_t>(3U, threads, [](std::uint32_t b) { return block_value(b); });
        REQUIRE(got.size() == 3U);
        CHECK(got[0] == block_value(0));
        CHECK(got[1] == block_value(1));
        CHECK(got[2] == block_value(2));
    }
}

TEST_CASE("parallel: one thread runs the blocks inline, in ascending order") {
    const std::thread::id caller = std::this_thread::get_id();
    std::vector<std::uint32_t> seen;
    bool all_on_caller = true;
    worldgen::run_blocks(40U, 1U, [&](std::uint32_t b) {
        // Single thread: no synchronisation needed.
        seen.push_back(b);
        all_on_caller = all_on_caller && (std::this_thread::get_id() == caller);
    });
    REQUIRE(seen.size() == 40U);
    for (std::uint32_t b = 0; b < 40U; ++b) {
        CHECK(seen[b] == b);
    }
    CHECK(all_on_caller);
}

TEST_CASE("parallel: one block runs inline on the calling thread whatever the thread count") {
    const std::thread::id caller = std::this_thread::get_id();
    for (const std::uint32_t threads : {1U, 2U, 5U, MAX_THREADS}) {
        CAPTURE(threads);
        int calls = 0;
        bool on_caller = false;
        std::uint32_t block_seen = 99U;
        worldgen::run_blocks(1U, threads, [&](std::uint32_t b) {
            block_seen = b;
            ++calls;
            on_caller = std::this_thread::get_id() == caller;
        });
        CHECK(calls == 1);
        CHECK(block_seen == 0U);
        CHECK(on_caller);
    }
}

TEST_CASE("parallel: zero blocks calls nothing and map_blocks returns an empty vector") {
    for (const std::uint32_t threads : thread_counts()) {
        CAPTURE(threads);
        int calls = 0;
        worldgen::run_blocks(0U, threads, [&](std::uint32_t) { ++calls; });
        CHECK(calls == 0);
        const auto got = worldgen::map_blocks<int>(0U, threads, [](std::uint32_t) { return 1; });
        CHECK(got.empty());
    }
}

TEST_CASE("parallel D10: an invalid thread count is fatal") {
    for (const std::uint32_t blocks : {0U, 1U, 5U}) {
        CAPTURE(blocks);
        int calls = 0;
        const auto fn = [&](std::uint32_t) { ++calls; };
        CHECK_FATAL(worldgen::run_blocks(blocks, 0U, fn));
        CHECK_FATAL(worldgen::run_blocks(blocks, MAX_THREADS + 1U, fn));
        CHECK_FATAL(worldgen::run_blocks(blocks, 0xFFFFFFFFU, fn));
        CHECK_FATAL(worldgen::map_blocks<int>(blocks, 0U, [](std::uint32_t) { return 0; }));
        CHECK_FATAL(worldgen::map_blocks<int>(blocks, MAX_THREADS + 1U, [](std::uint32_t) { return 0; }));
        CHECK(calls == 0);
    }
    CHECK(MAX_THREADS == 256U);
    // The bounds themselves are valid.
    int calls = 0;
    worldgen::run_blocks(3U, MAX_THREADS, [&](std::uint32_t) {});
    worldgen::run_blocks(3U, 1U, [&](std::uint32_t) { ++calls; });
    CHECK(calls == 3);
}

TEST_CASE("parallel D10: with several failing blocks the lowest block's exception is rethrown, at every thread count") {
    // Blocks 5, 9, 20 and 31 fail; 5 is the lowest. Block 5 waits a little
    // for block 31 to have failed first, to make "a higher block fails
    // earlier in time" likely when threads > 1.
    const std::uint32_t blocks = 40U;
    for (const std::uint32_t threads : thread_counts()) {
        CAPTURE(threads);
        for (int repeat = 0; repeat < 5; ++repeat) {
            std::atomic<bool> high_failed{false};
            std::vector<std::atomic<std::uint8_t>> done(blocks);
            for (auto& d : done) {
                d.store(0U);
            }
            bool caught = false;
            std::uint32_t caught_block = 0xFFFFFFFFU;
            try {
                worldgen::run_blocks(blocks, threads, [&](std::uint32_t b) {
                    if (b == 5U) {
                        wait_a_little_for(high_failed);
                        throw BlockFailure{5U};
                    }
                    if (b == 9U) {
                        worldgen::fatal("block 9");
                    }
                    if (b == 20U) {
                        throw std::runtime_error("block 20");
                    }
                    if (b == 31U) {
                        high_failed.store(true, std::memory_order_release);
                        throw BlockFailure{31U};
                    }
                    done[b].store(1U);
                });
            } catch (const BlockFailure& e) {
                caught = true;
                caught_block = e.block;
            } catch (...) {
                caught = true;  // wrong type: caught_block stays unset
            }
            CHECK(caught);
            CHECK(caught_block == 5U);
            // Every block below the lowest failing one ran to completion.
            for (std::uint32_t b = 0; b < 5U; ++b) {
                CHECK(done[b].load() == 1U);
            }
        }
    }
}

TEST_CASE("parallel D10: the lowest failing block's FatalError keeps its message") {
    for (const std::uint32_t threads : thread_counts()) {
        CAPTURE(threads);
        std::string message;
        try {
            worldgen::run_blocks(30U, threads, [](std::uint32_t b) {
                if (b == 29U || b == 12U || b == 13U) {
                    throw worldgen::FatalError("failed in block " + std::to_string(b));
                }
            });
        } catch (const worldgen::FatalError& e) {
            message = e.what();
        }
        CHECK(message.find("failed in block 12") != std::string::npos);
    }
}

TEST_CASE("parallel: with one thread the first exception stops the loop") {
    std::vector<std::uint32_t> ran;
    CHECK_THROWS_AS(worldgen::run_blocks(10U, 1U,
                                         [&](std::uint32_t b) {
                                             ran.push_back(b);
                                             if (b == 4U) {
                                                 throw BlockFailure{4U};
                                             }
                                         }),
                    BlockFailure);
    CHECK(ran == std::vector<std::uint32_t>{0U, 1U, 2U, 3U, 4U});
}

TEST_CASE("parallel D8: block results folded in block order give the sequential result") {
    // A fold that is not associative or commutative (a hash chain), and a
    // concatenation of variable-length lists: both must equal the
    // sequential computation at every thread count when the per-block
    // results of map_blocks are combined in ascending block order.
    const std::uint32_t blocks = 37U;
    const auto list_of = [](std::uint32_t b) {
        std::vector<std::uint64_t> l;
        for (std::uint32_t i = 0; i < (b * 7U) % 5U; ++i) {
            l.push_back(block_value(b) + i);
        }
        return l;
    };
    std::uint64_t seq_chain = 0;
    std::vector<std::uint64_t> seq_list;
    for (std::uint32_t b = 0; b < blocks; ++b) {
        seq_chain = worldgen::mix64(seq_chain ^ block_value(b));
        const auto l = list_of(b);
        seq_list.insert(seq_list.end(), l.begin(), l.end());
    }
    for (const std::uint32_t threads : thread_counts()) {
        CAPTURE(threads);
        const auto values = worldgen::map_blocks<std::uint64_t>(blocks, threads, [](std::uint32_t b) { return block_value(b); });
        const auto lists = worldgen::map_blocks<std::vector<std::uint64_t>>(blocks, threads, list_of);
        std::uint64_t chain = 0;
        std::vector<std::uint64_t> concat;
        for (std::uint32_t b = 0; b < blocks; ++b) {
            chain = worldgen::mix64(chain ^ values[b]);
            concat.insert(concat.end(), lists[b].begin(), lists[b].end());
        }
        CHECK(chain == seq_chain);
        CHECK(concat == seq_list);
    }
}

TEST_CASE("parallel: a const callable is accepted") {
    const auto fn = [](std::uint32_t b) { return static_cast<int>(b) * 3; };
    const auto got = worldgen::map_blocks<int>(4U, 2U, fn);
    CHECK(got == std::vector<int>{0, 3, 6, 9});
}

// --- G6 row-block drivers (P1-00 7.6, 8) ------------------------------------------

TEST_CASE("G6: for_each_row_block covers every row exactly once, for heights not a multiple of 64") {
    for (const std::int32_t height : {1, 2, 63, 64, 65, 127, 128, 129, 200, 513}) {
        for (const std::uint32_t threads : {1U, 2U, 5U, 13U}) {
            CAPTURE(height);
            CAPTURE(threads);
            std::vector<std::atomic<std::uint32_t>> row_hits(static_cast<std::size_t>(height));
            for (auto& r : row_hits) {
                r.store(0U);
            }
            std::atomic<bool> bad_bounds{false};
            worldgen::geo::for_each_row_block(height, threads, [&](const worldgen::geo::RowBlock& blk) {
                const std::int32_t begin = static_cast<std::int32_t>(blk.index) * 64;
                const std::int32_t end = begin + 64 < height ? begin + 64 : height;
                if (blk.y_begin != begin || blk.y_end != end || blk.y_begin < 0 || blk.y_end > height) {
                    bad_bounds.store(true);
                    return;
                }
                for (std::int32_t y = blk.y_begin; y < blk.y_end; ++y) {
                    row_hits[static_cast<std::size_t>(y)].fetch_add(1U);
                }
            });
            CHECK_FALSE(bad_bounds.load());
            for (std::int32_t y = 0; y < height; ++y) {
                CHECK(row_hits[static_cast<std::size_t>(y)].load() == 1U);
            }
        }
    }
}

TEST_CASE("G6, D1: a per-cell function computed in row blocks is identical at 1, 2 and 5 threads") {
    // A D1 step on a non-square 37 x 300 grid (5 blocks, the last of 44 rows).
    const std::int32_t w = 37;
    const std::int32_t h = 300;
    const auto compute = [&](std::uint32_t threads) {
        std::vector<std::uint64_t> grid(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0U);
        worldgen::geo::for_each_row_block(h, threads, [&](const worldgen::geo::RowBlock& blk) {
            for (std::int32_t y = blk.y_begin; y < blk.y_end; ++y) {
                for (std::int32_t x = 0; x < w; ++x) {
                    grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] =
                        worldgen::hash_at(0x47364431ULL, x, y);
                }
            }
        });
        return grid;
    };
    const auto one = compute(1U);
    CHECK(compute(2U) == one);
    CHECK(compute(5U) == one);
    CHECK(compute(MAX_THREADS) == one);
    CHECK(one[0] == worldgen::hash_at(0x47364431ULL, 0, 0));
    CHECK(one.back() == worldgen::hash_at(0x47364431ULL, w - 1, h - 1));
}

TEST_CASE("G6, D8: map_row_blocks returns one result per block, in block order") {
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        const auto rows = worldgen::geo::map_row_blocks<std::int64_t>(
            130, threads, [](const worldgen::geo::RowBlock& blk) { return static_cast<std::int64_t>(blk.y_end - blk.y_begin); });
        CHECK(rows == std::vector<std::int64_t>{64, 64, 2});
        const auto ids = worldgen::geo::map_row_blocks<std::uint32_t>(
            300, threads, [](const worldgen::geo::RowBlock& blk) { return blk.index; });
        CHECK(ids == std::vector<std::uint32_t>{0U, 1U, 2U, 3U, 4U});
    }
}

TEST_CASE("G6, D10: row-block drivers report the lowest failing block and reject bad thread counts") {
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        std::uint32_t caught = 0xFFFFFFFFU;
        try {
            worldgen::geo::for_each_row_block(1000, threads, [](const worldgen::geo::RowBlock& blk) {
                if (blk.index == 3U || blk.index == 14U || blk.index == 15U) {
                    throw BlockFailure{blk.index};
                }
            });
        } catch (const BlockFailure& e) {
            caught = e.block;
        }
        CHECK(caught == 3U);
    }
    CHECK_FATAL(worldgen::geo::for_each_row_block(100, 0U, [](const worldgen::geo::RowBlock&) {}));
    CHECK_FATAL(worldgen::geo::for_each_row_block(100, MAX_THREADS + 1U, [](const worldgen::geo::RowBlock&) {}));
    CHECK_FATAL(worldgen::geo::map_row_blocks<int>(100, 0U, [](const worldgen::geo::RowBlock&) { return 0; }));
}
