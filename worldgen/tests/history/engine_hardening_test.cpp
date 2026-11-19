// Engineer's tests of the P3-04 audit hardening (A3, A4, N1; audit report
// docs/reviews/p3-04-simulation-engine-determinism-audit.md): stored phase
// years bounded before arithmetic in [history], the region layer as a
// precondition of the engine, and a subsystem listing a step twice.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/inspect/history.hpp"

namespace {

using namespace engine_test;
using eng::Step;

std::string fatal_text(const std::function<void()>& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return "<no error>";
}

db::ColumnValue iv(std::int64_t v) {
    db::ColumnValue c;
    c.type = db::ValueType::integer;
    c.integer = v;
    return c;
}

// Two phase rows (id, age, kind, start, end, trigger) and no event: the
// first [0, e1], the second [e1 + 1, e2].
worldgen::inspect::HistoryTables two_phases(std::int64_t e1, std::int64_t e2) {
    worldgen::inspect::HistoryTables t;
    t.rows[3] = {{iv(1), iv(1), iv(1), iv(0), iv(e1), iv(0)}};
    if (e1 < std::numeric_limits<std::int64_t>::max()) {
        t.rows[3].push_back({iv(2), iv(1), iv(4), iv(e1 + 1), iv(e2), iv(0)});
    }
    return t;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 audit A3: a stored phase year beyond the span is corrupt, without overflow") {
    const std::int64_t max = std::numeric_limits<std::int64_t>::max();
    // Valid: two rows tiling [0, 6000).
    CHECK_FALSE(worldgen::inspect::history_stats(two_phases(149, 5999), 6000).problem.has_value());
    // A non-last row ending beyond E - 1 (2^63 - 1 included).
    worldgen::inspect::HistoryTables t = two_phases(149, 5999);
    t.rows[3][0][4] = iv(max);
    CHECK(worldgen::inspect::history_stats(t, 6000).problem ==
          std::optional<std::string>("corrupt table 'phase': row 1: end_year 9223372036854775807 above 5999"));
    // The last row: the 11.1 message, made before any arithmetic.
    CHECK(worldgen::inspect::history_stats(two_phases(149, max), 6000).problem ==
          std::optional<std::string>("corrupt table 'phase': the last phase ends in 9223372036854775807, expected 5999"));
    CHECK(worldgen::inspect::history_stats(two_phases(149, 6000), 6000).problem ==
          std::optional<std::string>("corrupt table 'phase': the last phase ends in 6000, expected 5999"));
}

TEST_CASE("P3-04 audit A4: a missing or wrong-shaped region layer is a precondition failure of run_engine") {
    const std::string want = "history: engine: layer 'region' is not 16 x 16 u16";
    const std::vector<std::int64_t> starts = {0, 2074};
    {
        eng::HistoryInputs in = inputs_for(starts, 6000);
        in.region_layer = db::LayerGrid{};  // missing
        Config cfg;
        CHECK(fatal_text([&] { (void)eng::run_engine(shipped(), in, cfg.get()); }) == want);
    }
    {
        eng::HistoryInputs in = inputs_for(starts, 6000);
        in.region_layer = db::make_layer_grid(db::ElementType::u16, 16, 8);  // wrong height
        Config cfg;
        CHECK(fatal_text([&] { (void)eng::run_engine(shipped(), in, cfg.get()); }) == want);
    }
    {
        eng::HistoryInputs in = inputs_for(starts, 6000);
        in.region_layer = db::make_layer_grid(db::ElementType::u8, 16, 16);  // wrong type
        Config cfg;
        CHECK(fatal_text([&] { (void)eng::run_engine(shipped(), in, cfg.get()); }) == want);
    }
    {
        const eng::HistoryInputs in = inputs_for(starts, 6000);
        Config cfg;
        CHECK(fatal_text([&] { (void)eng::run_engine(shipped(), in, cfg.get()); }) == "<no error>");
    }
}

TEST_CASE("P3-04 audit N1: a subsystem listing the same step twice is refused") {
    const eng::HistoryInputs in = inputs_for({0, 2074}, 6000);
    FnSubsystem twice({Step::t1, Step::t2, Step::t1}, [](eng::StepContext&) {});
    Config cfg;
    cfg.subsystems = {&twice};
    CHECK(fatal_text([&] { (void)eng::run_engine(shipped(), in, cfg.get()); }) ==
          "history: engine: a subsystem lists step 1 twice");
    // Two subsystems may list the same step.
    FnSubsystem a({Step::t1}, [](eng::StepContext&) {});
    FnSubsystem b({Step::t1, Step::q1}, [](eng::StepContext&) {});
    Config ok;
    ok.subsystems = {&a, &b};
    CHECK(fatal_text([&] { (void)eng::run_engine(shipped(), in, ok.get()); }) == "<no error>");
}

}  // TEST_SUITE
