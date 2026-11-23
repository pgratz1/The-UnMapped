// Spec P4-04 10.4: the bench of the markers, in the skipped suite "timeline-bench" (run by hand:
// worldgen-tests -ts=timeline-bench --no-skip -s): MarkerIndex::load, markers_at for every Y and draw_markers at
// E on the 1024 world of seed 42 (sample-42-1024). Prints only; the budgets of section 4 (1 ms a year at 1024)
// are judged by the reader. Wall-clock time is read here only, never in the library.

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "markers_support.hpp"

namespace {

using namespace mktest;
using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

}  // namespace

TEST_SUITE("timeline-bench" * doctest::skip()) {

TEST_CASE("P4-04 10.4 bench: MarkerIndex::load, markers_at for every year, draw_markers at E (1024, seed 42)") {
    const std::string path = tltest::world(tltest::world_cases_1024().back());
    const db::WorldReader r = tltest::open_reader(path);
    const wh::LoadedHistory h = tltest::load(r);
    auto t0 = Clock::now();
    const wi::MarkerIndex ix = load_index(r, h);
    const double load_ms = ms_since(t0);
    wh::YearState st;
    wi::YearMarkers out;
    double worst = 0, total = 0;
    std::size_t entries = 0;
    for (std::int64_t y = 0; y <= h.tables.end_year; ++y) {
        h.timeline.year_state(y, st);
        const auto t1 = Clock::now();
        ix.markers_at(h, st, ALL, out);
        const double d = ms_since(t1);
        worst = d > worst ? d : worst;
        total += d;
        entries += out.settlements.size() + out.places.size() + out.passes.size();
    }
    const tltest::WorldLayers layers = tltest::read_layers(r);
    const wi::TerritoryBase base = tltest::territory_base(layers);
    const std::vector<wi::Rgb> colours = wi::civilization_colours(h.tables);
    std::vector<std::uint8_t> rgb = wi::render_territory(base, h.timeline, colours, h.tables.end_year, 1);
    const wi::YearMarkers end = ix.markers_at(h, h.tables.end_year, ALL);
    t0 = Clock::now();
    wi::draw_markers(rgb, base.width(), base.height(), 1, end, colours);
    const double draw_ms = ms_since(t0);
    MESSAGE("MarkerIndex::load " << load_ms << " ms; markers_at slowest " << worst << " ms, mean "
                                 << total / static_cast<double>(h.tables.end_year + 1) << " ms over "
                                 << h.tables.end_year + 1 << " years (" << entries << " entries); draw_markers at E "
                                 << draw_ms << " ms");
    CHECK(entries > 0U);
}

}  // TEST_SUITE
