// The two guards of the P3-03 determinism audit (advisory A1), written by
// the engineer: awakening_years refuses a least gap G below 1, and
// StageWriter::write_age refuses an age that ends before it starts. Neither
// is reachable with loaded data; both are reached here through their seams.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../db_test_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/stages/chronology.hpp"

TEST_CASE("P3-03 C3 guard: awakening_years is fatal when the least gap is below 1") {
    worldgen::stages::ChronologyTuning t;
    t.awakening_jitter_frac = worldgen::Fx{};
    t.awakening_min_gap_years = 100;
    t.age_name_form = "The Age of {plural}";
    const std::vector<std::int64_t> ids = {1, 2, 3, 4, 5, 6};
    const std::vector<worldgen::Fx> shares(6, worldgen::Fx{});
    // span 5 with six races: G = min(100, 5 div 6) = 0.
    CHECK_THROWS_WITH_AS((void)worldgen::stages::awakening_years(0, 5, ids, shares, t),
                         "awakening_years: the least gap is below 1 year", worldgen::FatalError);
    // span 6: G = 1, accepted.
    CHECK(worldgen::stages::awakening_years(0, 6, ids, shares, t).size() == 6U);
}

TEST_CASE("P3-03 5.3 guard: write_age refuses end_year below start_year") {
    worldgen::db::WorldWriter writer;
    dbtest::set_standard_meta(writer, 0);
    worldgen::db::StageWriter stage = writer.begin_stage("chronology");
    CHECK_THROWS_WITH_AS(stage.write_age(worldgen::db::AgeRow{1, 1, 100, 99, "A"}),
                         "StageWriter::write_age: end_year must not be below start_year", worldgen::FatalError);
    CHECK_NOTHROW(stage.write_age(worldgen::db::AgeRow{1, 1, 100, 100, "A"}));
}
