// Parallel execution of fixed blocks (P0-01 4.2 as amended in section 15,
// P1-00 section 8 rule D10).

#include "worldgen/core/parallel.hpp"

#include <atomic>
#include <cstddef>
#include <exception>
#include <thread>
#include <vector>

#include "worldgen/core/error.hpp"

namespace worldgen::detail {

namespace {

// Shared state of one run_blocks call. Blocks are claimed in ascending
// order from `next`; `first_failed` is the lowest block that has raised so
// far (block_count while none has).
class BlockRun {
public:
    BlockRun(std::uint32_t block_count, void* context, BlockCall call)
        : count_(block_count), context_(context), call_(call), errors_(block_count), first_failed_(block_count) {}

    // One worker's loop. Never throws: a block's exception is stored.
    void work() noexcept {
        for (;;) {
            const std::uint64_t claimed = next_.fetch_add(1U);
            if (claimed >= count_) {
                return;
            }
            const auto block = static_cast<std::uint32_t>(claimed);
            // Blocks are claimed in ascending order, so once block f has
            // failed every block below f is already claimed and will finish;
            // blocks above f cannot change which exception is reported.
            if (block > first_failed_.load()) {
                return;
            }
            try {
                call_(context_, block);
            } catch (...) {
                errors_[block] = std::current_exception();
                std::uint32_t seen = first_failed_.load();
                while (block < seen && !first_failed_.compare_exchange_weak(seen, block)) {
                }
            }
        }
    }

    // After every worker has stopped: rethrows the lowest block's exception.
    void rethrow_first() const {
        const std::uint32_t failed = first_failed_.load();
        if (failed < count_) {
            std::rethrow_exception(errors_[failed]);
        }
    }

private:
    const std::uint32_t count_;
    void* const context_;
    const BlockCall call_;
    std::vector<std::exception_ptr> errors_;  // errors_[b] written only by the worker running b
    std::atomic<std::uint64_t> next_{0};       // 64-bit: claims past count_ cannot wrap
    std::atomic<std::uint32_t> first_failed_;
};

}  // namespace

void run_blocks_impl(std::uint32_t block_count, std::uint32_t threads, void* context, BlockCall call) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("run_blocks: thread count outside [1, 256]");
    }
    const std::uint32_t workers = threads < block_count ? threads : block_count;
    if (workers <= 1U) {
        // Inline, ascending, no thread at all (also block_count 0 or 1).
        for (std::uint32_t b = 0; b < block_count; ++b) {
            call(context, b);
        }
        return;
    }

    BlockRun run(block_count, context, call);
    // Fixed storage, sized before any thread starts, so starting a thread
    // never reallocates and no joinable std::thread is ever moved or
    // destroyed. `started` counts the threads that did start.
    std::vector<std::thread> pool;
    try {
        pool.resize(static_cast<std::size_t>(workers - 1U));
    } catch (...) {
        pool.clear();  // no storage for threads: the calling thread does every block
    }
    std::size_t started = 0;
    for (std::thread& slot : pool) {
        try {
            slot = std::thread([&run] { run.work(); });
        } catch (...) {
            break;  // std::system_error, std::bad_alloc, ...: the threads already started, and this one, do every block
        }
        ++started;
    }
    run.work();  // noexcept: block exceptions are stored
    // Join every started thread before `run` (which they use) goes out of
    // scope. join() of a joinable thread on another thread cannot fail.
    for (std::size_t i = 0; i < started; ++i) {
        pool[i].join();
    }
    run.rethrow_first();
}

}  // namespace worldgen::detail
