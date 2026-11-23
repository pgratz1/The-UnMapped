// The generator engineer's checks of spec P4-04: the glyph drawings and
// counts of section 5, the symbol table of 3.4, the words moved to digest.hpp
// and a few drawing rules of section 6. The stage's tests are the test
// writer's markers_* files.

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/markers.hpp"

namespace {

using worldgen::inspect::GlyphKey;

// The drawing of a glyph in the letters of 5.2 ('h' halo, 'F' fill, 'K'
// ink, '.' interior, '_' none), one string per row.
std::vector<std::string> drawing(GlyphKey key) {
    const worldgen::inspect::Glyph g = worldgen::inspect::marker_glyph(key);
    const auto side = static_cast<std::size_t>(2 * g.radius + 1);
    REQUIRE(g.cells.size() == side * side);
    static constexpr std::array<char, 5> LETTER = {'_', 'h', 'F', 'K', '.'};
    std::vector<std::string> rows;
    for (std::size_t y = 0; y < side; ++y) {
        std::string row;
        for (std::size_t x = 0; x < side; ++x) {
            row += LETTER[g.cells[y * side + x]];
        }
        rows.push_back(row);
    }
    return rows;
}

std::array<std::size_t, 5> counts(GlyphKey key) {
    std::array<std::size_t, 5> n{};
    for (const std::uint8_t c : worldgen::inspect::marker_glyph(key).cells) {
        ++n[c];
    }
    return n;
}

void check_counts(GlyphKey key, std::size_t halo, std::size_t fill, std::size_t ink, std::size_t interior) {
    const std::array<std::size_t, 5> n = counts(key);
    CHECK(n[1] == halo);
    CHECK(n[2] == fill);
    CHECK(n[3] == ink);
    CHECK(n[4] == interior);
}

}  // namespace

TEST_CASE("P4-04 engineer: the reference drawings of 5.2 and 5.3") {
    CHECK(drawing(GlyphKey{1, false, false}) ==
          std::vector<std::string>{"_hhhhh_", "hhKKKhh", "hKFFFKh", "hKFFFKh", "hKFFFKh", "hhKKKhh", "_hhhhh_"});
    CHECK(drawing(GlyphKey{2, false, false}) ==
          std::vector<std::string>{"__hhhhh__", "_hhKKKhh_", "hhKFFFKhh", "hKFFFFFKh", "hKFFFFFKh", "hKFFFFFKh",
                                   "hhKFFFKhh", "_hhKKKhh_", "__hhhhh__"});
    CHECK(drawing(GlyphKey{4, false, false}) ==
          std::vector<std::string>{"hhhhhhhhhhh", "hKKKKKKKKKh", "hKFFFFFFFKh", "hKFFFFFFFKh", "hKFFKKKFFKh",
                                   "hKFFKKKFFKh", "hKFFKKKFFKh", "hKFFFFFFFKh", "hKFFFFFFFKh", "hKKKKKKKKKh",
                                   "hhhhhhhhhhh"});
    CHECK(drawing(GlyphKey{5, false, false}) ==
          std::vector<std::string>{"____hhh____", "___hhKhh___", "__hhKFKhh__", "_hhKFFFKhh_", "hhKFFFFFKhh",
                                   "hKFFFFFFFKh", "hhKFFFFFKhh", "_hhKFFFKhh_", "__hhKFKhh__", "___hhKhh___",
                                   "____hhh____"});
    CHECK(drawing(GlyphKey{2, true, false}) ==
          std::vector<std::string>{"__hhhhh__", "_hhKKKhh_", "hhKFFFKhh", "hKF...FKh", "hKF...FKh", "hKF...FKh",
                                   "hhKFFFKhh", "_hhKKKhh_", "__hhhhh__"});
    CHECK(drawing(GlyphKey{3, false, true}) ==
          std::vector<std::string>{"hhhhhhhhhhh", "hKKKKKKKKKh", "hKKKKKKKKKh", "hKKFFFFFKKh", "hKKFFFFFKKh",
                                   "hKKFFFFFKKh", "hKKFFFFFKKh", "hKKFFFFFKKh", "hKKKKKKKKKh", "hKKKKKKKKKh",
                                   "hhhhhhhhhhh"});
    CHECK(drawing(GlyphKey{8, false, false}) ==
          std::vector<std::string>{"hhh_hhh", "hKhhhKh", "hhKhKhh", "_hhKhh_", "hhKhKhh", "hKhhhKh", "hhh_hhh"});
    CHECK(drawing(GlyphKey{9, false, false}) == std::vector<std::string>{"F___F", "_F_F_", "__F__", "_F_F_", "F___F"});
    CHECK(drawing(GlyphKey{10, false, false}) ==
          std::vector<std::string>{"_______", "__hhh__", "_hhKhh_", "hhKhKhh", "hKhhhKh", "hhh_hhh", "_______"});
}

TEST_CASE("P4-04 engineer: the class counts of 5.2 to 5.4") {
    check_counts(GlyphKey{1, false, false}, 24, 9, 12, 0);
    check_counts(GlyphKey{2, false, false}, 32, 21, 16, 0);
    check_counts(GlyphKey{3, false, false}, 32, 25, 24, 0);
    check_counts(GlyphKey{4, false, false}, 40, 40, 41, 0);
    check_counts(GlyphKey{5, false, false}, 40, 25, 16, 0);
    check_counts(GlyphKey{2, true, false}, 32, 12, 16, 9);
    check_counts(GlyphKey{3, false, true}, 40, 25, 56, 0);
    check_counts(GlyphKey{8, false, false}, 36, 0, 9, 0);
    check_counts(GlyphKey{9, false, false}, 0, 9, 0, 0);
    check_counts(GlyphKey{10, false, false}, 23, 0, 5, 0);
    check_counts(GlyphKey{11, false, false}, 144, 68, 104, 0);
    CHECK(worldgen::inspect::marker_glyph(GlyphKey{11, false, false}).radius == 11);
    CHECK_THROWS_AS((void)worldgen::inspect::marker_glyph(GlyphKey{0, false, false}), worldgen::FatalError);
    CHECK_THROWS_AS((void)worldgen::inspect::marker_glyph(GlyphKey{6, false, false}), worldgen::FatalError);
    CHECK_THROWS_AS((void)worldgen::inspect::marker_glyph(GlyphKey{12, false, false}), worldgen::FatalError);
    CHECK_THROWS_AS((void)worldgen::inspect::marker_glyph(GlyphKey{8, true, false}), worldgen::FatalError);
}

TEST_CASE("P4-04 engineer: symbols and words") {
    using worldgen::inspect::EventSymbol;
    using worldgen::inspect::event_symbol;
    CHECK(event_symbol("sacked") == EventSymbol::sack);
    CHECK(event_symbol("settlement_captured") == EventSymbol::sack);
    CHECK(event_symbol("refuge_held") == EventSymbol::siege);
    CHECK(event_symbol("battle_at_pass") == EventSymbol::battle);
    CHECK(event_symbol("plague_spread") == EventSymbol::plague);
    CHECK(event_symbol("sackedx") == EventSymbol::plain);
    CHECK(event_symbol("") == EventSymbol::plain);
    CHECK(worldgen::inspect::event_symbol_word(EventSymbol::siege) == "siege");
    CHECK(worldgen::inspect::event_symbol_colour(EventSymbol::plague) == worldgen::inspect::Rgb{150, 210, 0});
    CHECK(worldgen::history::settlement_kind_word(0).empty());
    CHECK(worldgen::history::settlement_kind_word(5) == "fortress");
    CHECK(worldgen::history::settlement_kind_word(6).empty());
    CHECK(worldgen::history::settlement_status_word(4) == "faded");
    CHECK(worldgen::history::settlement_status_word(5).empty());
}

TEST_CASE("P4-04 engineer: draw_markers order, clipping and scale") {
    using namespace worldgen::inspect;
    YearMarkers m;
    // A ring at the corner (clipped), a ruin inside it and a capital town.
    m.places.push_back(PlaceMarker{0, 0, EventSymbol::battle, {1}});
    SettlementMarker ruin;
    ruin.settlement = 1;
    ruin.x = 4;
    ruin.y = 4;
    ruin.kind = 2;
    ruin.status = 3;
    SettlementMarker town;
    town.settlement = 2;
    town.x = 20;
    town.y = 15;
    town.kind = 3;
    town.status = 1;
    town.owner = 1;
    town.capital = true;
    m.settlements = {ruin, town};
    const std::array<Rgb, 1> colours = {Rgb{10, 20, 30}};
    std::vector<std::uint8_t> one(3U * 32U * 24U, 7);
    draw_markers(one, 32, 24, 1, m, colours);
    const auto px = [](const std::vector<std::uint8_t>& b, std::size_t w, std::size_t x, std::size_t y) {
        const std::size_t at = (y * w + x) * 3U;
        return Rgb{b[at], b[at + 1U], b[at + 2U]};
    };
    CHECK(px(one, 32, 20, 15) == Rgb{10, 20, 30});  // the capital's fill
    CHECK(px(one, 32, 4, 4) == MARKER_INK);         // the ruin inside the ring
    CHECK(px(one, 32, 7, 7) == MARKER_HALO);        // the ruin's halo over the ring's ink (98)
    CHECK(px(one, 32, 0, 2) == Rgb{7, 7, 7});       // inside the ring: untouched
    CHECK(px(one, 32, 0, 7) == MARKER_HALO);        // the ring's inner halo
    CHECK(px(one, 32, 9, 0) == Rgb{230, 30, 30});  // the battle band, dx 9: 81 in 73..90
    std::vector<std::uint8_t> three(3U * 96U * 72U, 7);
    draw_markers(three, 32, 24, 3, m, colours);
    bool scaled = true;
    for (std::size_t y = 0; y < 72U; ++y) {
        for (std::size_t x = 0; x < 96U; ++x) {
            scaled = scaled && px(three, 96, x, y) == px(one, 32, x / 3U, y / 3U);
        }
    }
    CHECK(scaled);
    std::vector<std::uint8_t> wrong(10, 0);
    CHECK_THROWS_AS(draw_markers(wrong, 32, 24, 1, m, colours), worldgen::FatalError);
    CHECK_THROWS_AS(draw_markers(one, 32, 24, 0, m, colours), worldgen::FatalError);
    CHECK_THROWS_AS(draw_markers(one, 32, 24, 1, m, std::span<const Rgb>{}), worldgen::FatalError);
}
