#pragma once

// Parallel execution of fixed blocks (spec P0-01 section 4.2 as amended in
// section 15; P1-00 section 8, rules D1, D2, D8 and D10).
//
// The caller splits its work into blocks whose boundaries depend only on the
// data (for geography: the G6 row blocks of worldgen/geo/blocks.hpp) and
// supplies the thread count. The thread count only schedules blocks: it is
// never stored and never changes a result, provided each block writes only
// its own outputs and reads only values fixed before the call (P0-01 4.2).
//
// There is no thread pool and no global state: each call starts its worker
// threads (std::thread) and joins them before it returns. With one thread,
// or one block, the blocks run inline on the calling thread in ascending
// order and no thread is created.
//
// Failure (P1-00 rule D10): if blocks raise exceptions (the fatal error,
// P0-01 4.1, or anything else), the exception rethrown to the caller is the
// one raised by the lowest-numbered failing block. Blocks above a failed
// block may be skipped; every block below it runs to completion.

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace worldgen {

// Largest thread count a caller may pass (P1-00 10.1: --threads 1 to 256).
inline constexpr std::uint32_t MAX_THREADS = 256U;

namespace detail {
using BlockCall = void (*)(void* context, std::uint32_t block);
// Implementation of run_blocks (src/core/parallel.cpp).
void run_blocks_impl(std::uint32_t block_count, std::uint32_t threads, void* context, BlockCall call);
}  // namespace detail

// Calls fn(b) once for every block b in [0, block_count), on up to `threads`
// threads (P0-01 4.2, P1-00 D10). fn must be safe to call concurrently for
// different blocks: it writes only block b's own outputs. With threads = 1
// (or block_count <= 1) the calls are made inline, in ascending b, and the
// first exception stops the loop. Otherwise blocks are handed out in
// ascending order to the workers (the calling thread is one of them) and the
// exception of the lowest failing block is rethrown after every worker has
// stopped. Fatal (FatalError) if threads is 0 or above MAX_THREADS. If the
// system refuses to start a thread, the remaining workers (at least the
// calling thread) do all the blocks; the result is the same.
template <typename Fn>
void run_blocks(std::uint32_t block_count, std::uint32_t threads, Fn&& fn) {
    using F = std::remove_reference_t<Fn>;  // possibly const
    void* const context = const_cast<void*>(static_cast<const void*>(std::addressof(fn)));
    detail::run_blocks_impl(block_count, threads, context, [](void* ctx, std::uint32_t block) {
        (*static_cast<F*>(ctx))(block);
    });
}

// Runs fn(b) for every block as run_blocks does and returns the results
// indexed by block: result[b] = fn(b). This is the reduction helper of
// P1-00 rule D8 and P0-01 4.2 rule 3: the caller combines result[0],
// result[1], ... in ascending block order (lists are concatenated in block
// order; an argmin or argmax breaks ties by the lowest index). R must be
// default-constructible and move-assignable, and not bool (std::vector<bool>
// elements cannot be written from several threads).
template <typename R, typename Fn>
[[nodiscard]] std::vector<R> map_blocks(std::uint32_t block_count, std::uint32_t threads, Fn&& fn) {
    static_assert(!std::is_same_v<R, bool>, "map_blocks: use std::uint8_t instead of bool");
    std::vector<R> results(block_count);
    run_blocks(block_count, threads, [&results, &fn](std::uint32_t block) { results[block] = fn(block); });
    return results;
}

}  // namespace worldgen
