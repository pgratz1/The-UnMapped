// The generator engineer's checks of spec P4-03 (not the stage's tests, which
// are the timeline_* files): the 4 x 2 example of 8.2, entries of the table of
// 7.2 and phase_kind_word of 9.5.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/territory.hpp"

namespace {

namespace db = worldgen::db;
namespace wi = worldgen::inspect;
using wi::Rgb;

Rgb pixel(const std::vector<std::uint8_t>& img, std::uint32_t width, std::uint32_t x, std::uint32_t y) {
    const std::size_t i = 3U * (std::size_t{y} * width + x);
    REQUIRE(i + 2U < img.size());
    return Rgb{img[i], img[i + 1U], img[i + 2U]};
}

}  // namespace

TEST_SUITE("p4-03-engineer") {

TEST_CASE("P4-03 8.2: the 4 x 2 example") {
    db::LayerGrid elevation = db::make_layer_grid(db::ElementType::i16, 4, 2);
    elevation.values = std::vector<std::int16_t>(8, 10);
    db::LayerGrid biome = db::make_layer_grid(db::ElementType::u8, 4, 2);
    biome.values = std::vector<std::uint8_t>(8, 1);
    db::LayerGrid region = db::make_layer_grid(db::ElementType::u16, 4, 2);
    region.values = std::vector<std::uint16_t>{1, 2, 3, 3, 1, 2, 3, 3};
    const std::vector<wi::BiomeColour> table = {{1, 0x808000}};
    const wi::TerritoryBase base = wi::TerritoryBase::build(elevation, nullptr, nullptr, nullptr, biome, region, table);
    const Rgb elf0 = wi::race_civilization_colour("elf", 0);
    const Rgb elf1 = wi::race_civilization_colour("elf", 1);
    std::vector<std::optional<Rgb>> fill(4);
    fill[1] = elf0;
    fill[2] = elf0;
    fill[3] = elf1;
    const std::vector<std::int64_t> holders = {0, 1, 1, 2};
    const std::vector<std::uint8_t> img = wi::render_territory(base, fill, holders, 1);
    for (std::uint32_t y = 0; y < 2U; ++y) {
        CHECK(pixel(img, 4, 0, y) == Rgb{30, 142, 52});
        CHECK(pixel(img, 4, 1, y) == Rgb{0, 0, 0});
        CHECK(pixel(img, 4, 2, y) == Rgb{150, 230, 150});
        CHECK(pixel(img, 4, 3, y) == Rgb{150, 230, 150});
    }
    // No holders: every border black (P4-01's image).
    const std::vector<std::uint8_t> plain = wi::render_territory(base, fill, {}, 1);
    CHECK(pixel(plain, 4, 0, 0) == Rgb{0, 0, 0});
    // No holders and no fill: the regions composite.
    CHECK(wi::render_territory(base, {}, {}, 1) ==
          wi::render_composite_regions(elevation, nullptr, nullptr, nullptr, biome, region, 1, table));
}

TEST_CASE("P4-03 7.2: some entries, the wrap and the fallback") {
    CHECK(wi::race_civilization_colour("human", 0) == Rgb{0, 190, 195});
    CHECK(wi::race_civilization_colour("orc", 6) == Rgb{135, 15, 20});
    CHECK(wi::race_civilization_colour("goblin", 3) == Rgb{205, 160, 60});
    CHECK(wi::race_base_colour("faerie") == Rgb{175, 70, 225});
    CHECK(wi::race_civilization_colour("troll", 2) ==
          wi::civilization_shade(wi::race_base_colour("troll"), 2));
    CHECK_THROWS_AS((void)wi::race_civilization_colour("elf", -1), worldgen::FatalError);
}

TEST_CASE("P4-03 9.5: phase_kind_word") {
    CHECK(worldgen::history::phase_kind_word(0).empty());
    CHECK(worldgen::history::phase_kind_word(1) == "opening");
    CHECK(worldgen::history::phase_kind_word(2) == "quiet");
    CHECK(worldgen::history::phase_kind_word(3) == "flare_up");
    CHECK(worldgen::history::phase_kind_word(4) == "final");
    CHECK(worldgen::history::phase_kind_word(5).empty());
}

}  // TEST_SUITE
