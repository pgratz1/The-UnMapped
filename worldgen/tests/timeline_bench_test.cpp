// Spec P4-01 13.5 item 9: the timeline bench, a skipped suite (as engine-bench), run by hand:
//
//     worldgen-tests -ts=timeline-bench --no-skip
//
// On the 1024 world of seed 42 (sample-42-1024, generated into the per-process scratch directory): the time
// of load_history, of year_state for every year 0 to E (both overloads), of TerritoryBase::build and of
// render_territory for a new fill, printed against the budgets of P4-00 8 (a year answer 2 ms, a territory
// image 40 ms at 1024). Wall-clock time is used here only: printed by a skipped test, never stored and
// never a pass condition (hard rule 1; timing is the validation note's business).

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "timeline_support.hpp"

namespace {

using namespace tltest;
using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

}  // namespace

TEST_SUITE("timeline-bench" * doctest::skip()) {

TEST_CASE("P4-01 13.5 item 9: load_history, year_state and render_territory at 1024, seed 42") {
    const WorldSpec spec = world_cases_1024()[3];
    REQUIRE(spec.name == "sample-42-1024");
    const std::string path = world(spec);
    std::cout << "timeline-bench: input " << path << "\n";
    const db::WorldReader r = open_reader(path);

    Clock::time_point t0 = Clock::now();
    const wh::LoadedHistory h = load(r);
    const double load_ms = ms_since(t0);

    const std::int64_t E = h.tables.end_year;
    std::uint64_t sink = 0;
    t0 = Clock::now();
    for (std::int64_t y = 0; y <= E; ++y) sink += h.timeline.year_state(y).events.size();
    const double all_returning = ms_since(t0);
    wh::YearState st;
    t0 = Clock::now();
    for (std::int64_t y = 0; y <= E; ++y) {
        h.timeline.year_state(y, st);
        sink += st.relations.size();
    }
    const double all_reusing = ms_since(t0);

    const WorldLayers layers = read_layers(r);
    t0 = Clock::now();
    const wi::TerritoryBase base = territory_base(layers);
    const double base_ms = ms_since(t0);
    const std::vector<wi::Rgb> colours = wi::civilization_colours(h.tables);
    std::vector<std::uint8_t> img(3U * std::size_t{base.width()} * base.height());
    const std::int64_t years[] = {0, E / 4, E / 2, (3 * E) / 4, E};
    double render_max = 0;
    for (const std::int64_t y : years) {
        const auto fill = wi::territory_fill(h.timeline, colours, y);
        t0 = Clock::now();
        wi::render_territory(base, fill, 1, img);
        const double ms = ms_since(t0);
        render_max = ms > render_max ? ms : render_max;
        sink += img[0];
    }

    const double per_year = all_returning / static_cast<double>(E + 1);
    const double per_year_reusing = all_reusing / static_cast<double>(E + 1);
    std::cout << "timeline-bench: load_history " << load_ms << " ms\n"
              << "timeline-bench: year_state, " << (E + 1) << " years: " << all_returning << " ms ("
              << per_year << " ms per year; budget 2 ms)\n"
              << "timeline-bench: year_state reusing its buffers: " << all_reusing << " ms (" << per_year_reusing
              << " ms per year)\n"
              << "timeline-bench: TerritoryBase::build " << base_ms << " ms (once per open)\n"
              << "timeline-bench: render_territory, worst of 5 years: " << render_max << " ms (budget 40 ms)\n"
              << "timeline-bench: (checksum " << sink << ")\n";
#ifndef NDEBUG
    std::cout << "timeline-bench: a build without NDEBUG; the budgets of P4-00 8 apply to release builds\n";
#endif
    CHECK(sink > 0U);
}

}  // TEST_SUITE
