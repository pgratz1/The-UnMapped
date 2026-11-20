// Spec P3-04 r10 14.3: the engine microbenchmark. The engine alone (the
// scheduler, the tick years and quiet steps dispatched to no subsystem, the
// stub awakenings S1 and S2, bookkeeping, names, the result rows) on a 1024
// world: per simulated year below 10 us, and below 50 ms in all, in a build
// with NDEBUG. Skipped by default, not part of ctest's run:
//
//     worldgen-tests -ts=engine-bench --no-skip
//
// The input world is the file named by UNMAPPED_ENGINE_BENCH (any world
// stored with --until chronology or later), else a world generated into the
// per-process scratch directory with seed 1, map size 1024 and --until
// chronology (the geography of case geo-seed-1-1024). Wall-clock time is
// used here only: it is printed by a skipped test and never stored (hard
// rule 1). In a debug or sanitizer build the test prints and checks nothing.

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/history/engine.hpp"

namespace {

using namespace engine_test;

// An environment variable, portably (MSVC deprecates std::getenv, C4996).
std::optional<std::string> engine_bench_env(const char* name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

// The simulated years of a run: the tick years of the opening, flare-up and
// final phase rows (kinds 1, 3, 4; P3-04 4.1 `simulated_min_permille`).
std::int64_t simulated_years(const worldgen::history::HistoryResult& r) {
    std::int64_t years = 0;
    for (const auto& p : r.phases) {
        if (p.kind != 2) {
            years += p.end_year - p.start_year + 1;
        }
    }
    return years;
}

}  // namespace

TEST_SUITE("engine-bench" * doctest::skip()) {

TEST_CASE("P3-04 r10 14.3: engine microbenchmark (stub awakenings, no subsystem, no tension) at 1024") {
    std::string path;
    if (const std::optional<std::string> env = engine_bench_env("UNMAPPED_ENGINE_BENCH"); env && !env->empty()) {
        path = *env;
    } else {
        path = chrono_test::world(chrono_test::WorldKey{1, "", 1024, 6000, "all", "chronology", 1});
    }
    std::cout << "engine-bench: input " << path << "\n";
    const db::WorldReader world = open_world(path);
    const eng::HistoryInputs in = eng::inputs_from_world(world, shipped());  // outside the clock

    std::vector<double> ms;
    std::int64_t years = 0;
    for (int run = 0; run < 5; ++run) {
        Config cfg;  // no subsystem, no tension source, the empty intervention source
        cfg.handler = eng::make_stub();
        cfg.catalogue = eng::catalogue_of(shipped());
        const eng::EngineConfig c = cfg.get();
        const auto t0 = std::chrono::steady_clock::now();
        const worldgen::history::HistoryResult r = worldgen::history::run_engine(shipped(), in, c);
        const auto t1 = std::chrono::steady_clock::now();
        years = simulated_years(r);
        const double t = std::chrono::duration<double, std::milli>(t1 - t0).count();
        ms.push_back(t);
        std::cout << "engine-bench: run " << run + 1 << ": " << t << " ms, " << years << " simulated years, "
                  << r.events.size() << " events\n";
    }
    std::vector<double> sorted = ms;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[2];
    REQUIRE(years > 0);
    const double us_per_year = median * 1000.0 / static_cast<double>(years);
    std::cout << "engine-bench: median " << median << " ms, " << us_per_year << " us per simulated year\n";
#if defined(NDEBUG)
    CHECK(us_per_year < 10.0);
    CHECK(median < 50.0);
#endif
}

}  // TEST_SUITE
