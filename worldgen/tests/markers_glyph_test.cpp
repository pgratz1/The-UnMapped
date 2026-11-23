// Spec P4-04 10.2 items 1, 2 and 6: every glyph of section 5 against the drawings of 5.2 and 5.3 and the
// class counts of 5.2 to 5.4, the variants not drawn there from the rule (written here as predicates),
// marker_glyph's FatalErrors; event_symbol for every key of 3.4, near misses and the empty key, the symbol
// words and colours; glyph_of; the marker colour constants; settlement_kind_word and settlement_status_word for
// 0 ... 6 and their agreement with the history report's words.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "markers_support.hpp"
#include "worldgen/core/error.hpp"

namespace {

using namespace mktest;
using wi::GlyphKey;
using wi::Rgb;

template <class F>
std::string fatal_text(F&& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

// A drawing of the spec (h halo, F fill, K ink, . interior, - none) as class codes.
std::vector<std::uint8_t> cells_of(const std::vector<std::string>& rows) {
    std::vector<std::uint8_t> out;
    for (const std::string& r : rows) {
        REQUIRE(r.size() == rows.size());
        for (const char ch : r) {
            switch (ch) {
                case 'h': out.push_back(1); break;
                case 'F': out.push_back(2); break;
                case 'K': out.push_back(3); break;
                case '.': out.push_back(4); break;
                case '-': out.push_back(0); break;
                default: FAIL("bad drawing character " << ch);
            }
        }
    }
    return out;
}

std::string drawing_of(const wi::Glyph& g) {
    const auto n = static_cast<std::size_t>(2 * g.radius + 1);
    std::string s;
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < n; ++i) s += "-hFK."[g.cells[j * n + i]];
        s += "\n";
    }
    return s;
}

void check_drawing(GlyphKey k, const std::vector<std::string>& rows) {
    CAPTURE(glyph_name(k));
    const wi::Glyph g = wi::marker_glyph(k);
    CHECK(2 * g.radius + 1 == static_cast<std::int32_t>(rows.size()));
    CHECK_MESSAGE(g.cells == cells_of(rows), "got\n" << drawing_of(g));
}

std::array<int, 4> counts(const wi::Glyph& g) {
    std::array<int, 4> c{};
    for (const std::uint8_t v : g.cells) {
        if (v >= 1 && v <= 4) ++c[v - 1U];
    }
    return c;
}

// The rule of 5.1 and 5.2, written as predicates.
bool in_shape(std::uint8_t base, std::int64_t r, std::int64_t dx, std::int64_t dy) {
    if (r < 0) return false;
    const std::int64_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (base <= 2) return dx * dx + dy * dy <= r * r + r;
    if (base <= 4) return (ax > ay ? ax : ay) <= r;
    return ax + ay <= r;
}

std::vector<std::uint8_t> rule_cells(GlyphKey k) {
    const std::int64_t r = k.base == 1 ? 1 : k.base == 2 ? 2 : k.base == 3 ? 2 : 3;
    const std::int64_t o = r + 1 + (k.capital ? 1 : 0);
    const std::int64_t R = o + 1;
    const auto n = static_cast<std::size_t>(2 * R + 1);
    std::vector<std::uint8_t> c(n * n, 0);
    for (std::int64_t dy = -R; dy <= R; ++dy) {
        for (std::int64_t dx = -R; dx <= R; ++dx) {
            const auto i = static_cast<std::size_t>((dy + R) * (2 * R + 1) + dx + R);
            if (in_shape(k.base, r, dx, dy)) {
                c[i] = (k.declining && in_shape(k.base, r - 1, dx, dy)) ? 4 : 2;
            } else if (in_shape(k.base, o, dx, dy)) {
                c[i] = 3;
            }
            if (k.base == 4 && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) c[i] = 3;
        }
    }
    std::vector<std::uint8_t> h = c;
    const auto N = static_cast<std::int64_t>(n);
    for (std::int64_t j = 0; j < N; ++j) {
        for (std::int64_t i = 0; i < N; ++i) {
            if (c[static_cast<std::size_t>(j * N + i)] != 0) continue;
            for (std::int64_t b = -1; b <= 1; ++b) {
                for (std::int64_t a = -1; a <= 1; ++a) {
                    if ((a != 0 || b != 0) && i + a >= 0 && i + a < N && j + b >= 0 && j + b < N &&
                        c[static_cast<std::size_t>((j + b) * N + i + a)] == 3) {
                        h[static_cast<std::size_t>(j * N + i)] = 1;
                    }
                }
            }
        }
    }
    return h;
}

}  // namespace

TEST_SUITE("markers") {

TEST_CASE("P4-04 10.2 item 1: the drawings of 5.2 and 5.3") {
    check_drawing({1, false, false}, {"-hhhhh-", "hhKKKhh", "hKFFFKh", "hKFFFKh", "hKFFFKh", "hhKKKhh", "-hhhhh-"});
    check_drawing({2, false, false}, {"--hhhhh--", "-hhKKKhh-", "hhKFFFKhh", "hKFFFFFKh", "hKFFFFFKh", "hKFFFFFKh",
                                      "hhKFFFKhh", "-hhKKKhh-", "--hhhhh--"});
    check_drawing({3, false, false}, {"hhhhhhhhh", "hKKKKKKKh", "hKFFFFFKh", "hKFFFFFKh", "hKFFFFFKh", "hKFFFFFKh",
                                      "hKFFFFFKh", "hKKKKKKKh", "hhhhhhhhh"});
    check_drawing({4, false, false}, {"hhhhhhhhhhh", "hKKKKKKKKKh", "hKFFFFFFFKh", "hKFFFFFFFKh", "hKFFKKKFFKh",
                                      "hKFFKKKFFKh", "hKFFKKKFFKh", "hKFFFFFFFKh", "hKFFFFFFFKh", "hKKKKKKKKKh",
                                      "hhhhhhhhhhh"});
    check_drawing({5, false, false}, {"----hhh----", "---hhKhh---", "--hhKFKhh--", "-hhKFFFKhh-", "hhKFFFFFKhh",
                                      "hKFFFFFFFKh", "hhKFFFFFKhh", "-hhKFFFKhh-", "--hhKFKhh--", "---hhKhh---",
                                      "----hhh----"});
    check_drawing({2, true, false}, {"--hhhhh--", "-hhKKKhh-", "hhKFFFKhh", "hKF...FKh", "hKF...FKh", "hKF...FKh",
                                     "hhKFFFKhh", "-hhKKKhh-", "--hhhhh--"});
    check_drawing({3, false, true}, {"hhhhhhhhhhh", "hKKKKKKKKKh", "hKKKKKKKKKh", "hKKFFFFFKKh", "hKKFFFFFKKh",
                                     "hKKFFFFFKKh", "hKKFFFFFKKh", "hKKFFFFFKKh", "hKKKKKKKKKh", "hKKKKKKKKKh",
                                     "hhhhhhhhhhh"});
    check_drawing({8, false, false}, {"hhh-hhh", "hKhhhKh", "hhKhKhh", "-hhKhh-", "hhKhKhh", "hKhhhKh", "hhh-hhh"});
    check_drawing({9, false, false}, {"F---F", "-F-F-", "--F--", "-F-F-", "F---F"});
    check_drawing({10, false, false}, {"-------", "--hhh--", "-hhKhh-", "hhKhKhh", "hKhhhKh", "hhh-hhh", "-------"});
}

TEST_CASE("P4-04 10.2 item 1: class counts of 5.2 to 5.4") {
    struct C {
        GlyphKey k;
        std::array<int, 4> want;  // halo, fill, ink, interior
    };
    const C cases[] = {{{1, false, false}, {24, 9, 12, 0}},   {{2, false, false}, {32, 21, 16, 0}},
                       {{3, false, false}, {32, 25, 24, 0}},  {{4, false, false}, {40, 40, 41, 0}},
                       {{5, false, false}, {40, 25, 16, 0}},  {{2, true, false}, {32, 12, 16, 9}},
                       {{3, false, true}, {40, 25, 56, 0}},   {{8, false, false}, {36, 0, 9, 0}},
                       {{9, false, false}, {0, 9, 0, 0}},     {{10, false, false}, {23, 0, 5, 0}},
                       {{11, false, false}, {144, 68, 104, 0}}};
    for (const C& c : cases) {
        CAPTURE(glyph_name(c.k));
        CHECK(counts(wi::marker_glyph(c.k)) == c.want);
    }
}

TEST_CASE("P4-04 10.2 item 1: every settlement variant from the rule of 5.2; radii") {
    for (std::uint8_t b = 1; b <= 5; ++b) {
        for (const bool d : {false, true}) {
            for (const bool cap : {false, true}) {
                const GlyphKey k{b, d, cap};
                CAPTURE(glyph_name(k));
                const wi::Glyph g = wi::marker_glyph(k);
                const std::int32_t r = b == 1 ? 1 : b == 2 ? 2 : b == 3 ? 2 : 3;
                CHECK(g.radius == r + 2 + (cap ? 1 : 0));
                CHECK_MESSAGE(g.cells == rule_cells(k), "got\n" << drawing_of(g));
            }
        }
    }
    CHECK(wi::marker_glyph({8, false, false}).radius == 3);
    CHECK(wi::marker_glyph({9, false, false}).radius == 2);
    CHECK(wi::marker_glyph({10, false, false}).radius == 3);
    CHECK(wi::marker_glyph({11, false, false}).radius == 11);
    // the largest glyph is the ring: 23 x 23 cells (4 cost note)
    CHECK(wi::marker_glyph({11, false, false}).cells.size() == 23U * 23U);
}

TEST_CASE("P4-04 10.2 item 1: the place ring of 5.4 by its predicates") {
    const wi::Glyph g = wi::marker_glyph({11, false, false});
    REQUIRE(g.radius == 11);
    auto disc = [](std::int64_t r, std::int64_t dx, std::int64_t dy) { return dx * dx + dy * dy <= r * r + r; };
    for (std::int64_t dy = -11; dy <= 11; ++dy) {
        for (std::int64_t dx = -11; dx <= 11; ++dx) {
            const std::uint8_t c = g.cells[static_cast<std::size_t>((dy + 11) * 23 + dx + 11)];
            CAPTURE(dx);
            CAPTURE(dy);
            if ((disc(10, dx, dy) && !disc(9, dx, dy)) || (disc(8, dx, dy) && !disc(7, dx, dy))) {
                CHECK(c == 3);
            } else if (disc(9, dx, dy) && !disc(8, dx, dy)) {
                CHECK(c == 2);
            } else {
                CHECK((c == 0 || c == 1));
            }
            // the colour band (73 ... 90) lies outside the body and halo of a capital city (S(6))
            if (c == 2) CHECK((dx * dx > 36 || dy * dy > 36));
        }
    }
    // the centre and the deep inside stay 0, so the settlement under a ring shows
    CHECK(g.cells[11 * 23 + 11] == 0);
    CHECK(g.cells[11 * 23 + 11 + 6] == 0);
}

TEST_CASE("P4-04 10.2 item 1: marker_glyph refuses keys section 5 does not define") {
    for (const GlyphKey k : {GlyphKey{0, false, false}, GlyphKey{6, false, false}, GlyphKey{7, false, false},
                             GlyphKey{12, false, false}, GlyphKey{16, false, false}, GlyphKey{31, false, false},
                             GlyphKey{255, false, false}, GlyphKey{8, true, false}, GlyphKey{8, false, true},
                             GlyphKey{9, true, false}, GlyphKey{10, false, true}, GlyphKey{11, true, true}}) {
        CAPTURE(static_cast<int>(k.base));
        CAPTURE(k.declining);
        CAPTURE(k.capital);
        CHECK_FALSE(fatal_text([&] { (void)wi::marker_glyph(k); }).empty());
    }
}

TEST_CASE("P4-04 10.2 item 1: the glyph dump of 10.1 against the golden file") {
    const std::string d = glyph_dump();
    CHECK_MESSAGE(sha(d) == golden().glyphs, d.substr(0, 3000));
}

TEST_CASE("P4-04 10.2 item 2: event_symbol, its words and colours") {
    using wi::EventSymbol;
    struct K {
        std::string_view key;
        EventSymbol s;
    };
    const K keys[] = {{"sacked", EventSymbol::sack},         {"settlement_captured", EventSymbol::sack},
                      {"siege", EventSymbol::siege},         {"refuge_held", EventSymbol::siege},
                      {"battle", EventSymbol::battle},       {"battle_at_pass", EventSymbol::battle},
                      {"plague", EventSymbol::plague},       {"plague_outbreak", EventSymbol::plague},
                      {"plague_spread", EventSymbol::plague}, {"sackedx", EventSymbol::plain},
                      {"", EventSymbol::plain},              {"Sacked", EventSymbol::plain},
                      {"sacked ", EventSymbol::plain},       {"battl", EventSymbol::plain},
                      {"settlement_founded", EventSymbol::plain}, {"ruin_faded", EventSymbol::plain},
                      {"capital_moved", EventSymbol::plain}};
    for (const K& k : keys) {
        CAPTURE(std::string(k.key));
        CHECK(wi::event_symbol(k.key) == k.s);
    }
    // keys compared as bytes: an embedded NUL is not a match
    CHECK(wi::event_symbol(std::string_view("siege\0", 6)) == EventSymbol::plain);
    // the order of the enum is the priority
    CHECK(static_cast<int>(EventSymbol::plain) == 0);
    CHECK(static_cast<int>(EventSymbol::plague) == 1);
    CHECK(static_cast<int>(EventSymbol::battle) == 2);
    CHECK(static_cast<int>(EventSymbol::siege) == 3);
    CHECK(static_cast<int>(EventSymbol::sack) == 4);
    CHECK(wi::event_symbol_word(EventSymbol::plain) == "plain");
    CHECK(wi::event_symbol_word(EventSymbol::plague) == "plague");
    CHECK(wi::event_symbol_word(EventSymbol::battle) == "battle");
    CHECK(wi::event_symbol_word(EventSymbol::siege) == "siege");
    CHECK(wi::event_symbol_word(EventSymbol::sack) == "sack");
    CHECK(wi::event_symbol_colour(EventSymbol::sack) == Rgb{200, 0, 200});
    CHECK(wi::event_symbol_colour(EventSymbol::siege) == Rgb{255, 150, 0});
    CHECK(wi::event_symbol_colour(EventSymbol::battle) == Rgb{230, 30, 30});
    CHECK(wi::event_symbol_colour(EventSymbol::plague) == Rgb{150, 210, 0});
    CHECK(wi::event_symbol_colour(EventSymbol::plain) == Rgb{255, 255, 255});
}

TEST_CASE("P4-04 section 4: marker constants, enum codes and glyph_of") {
    CHECK(wi::MARKER_INK == Rgb{0, 0, 0});
    CHECK(wi::MARKER_HALO == Rgb{240, 240, 240});
    CHECK(wi::MARKER_FADED == Rgb{150, 150, 150});
    CHECK(wi::MARKER_NO_OWNER == Rgb{128, 128, 128});
    CHECK(static_cast<int>(wi::MarkerType::pass) == 1);
    CHECK(static_cast<int>(wi::MarkerType::place) == 2);
    CHECK(static_cast<int>(wi::MarkerType::faded) == 3);
    CHECK(static_cast<int>(wi::MarkerType::ruin) == 4);
    CHECK(static_cast<int>(wi::MarkerType::standing) == 5);
    CHECK(static_cast<int>(wi::MarkerType::site) == 6);
    CHECK(static_cast<int>(wi::MarkerType::figure) == 7);
    const wi::MarkerOptions def;
    CHECK_FALSE(def.faded);
    CHECK(def.events);
    CHECK(def.passes);

    wi::SettlementMarker s;
    s.settlement = 1;
    for (std::int64_t kind = 1; kind <= 5; ++kind) {
        s.kind = kind;
        s.status = 1;
        s.capital = false;
        CHECK(wi::glyph_of(s) == GlyphKey{static_cast<std::uint8_t>(kind), false, false});
        s.status = 2;
        CHECK(wi::glyph_of(s) == GlyphKey{static_cast<std::uint8_t>(kind), true, false});
        s.capital = true;
        CHECK(wi::glyph_of(s) == GlyphKey{static_cast<std::uint8_t>(kind), true, true});
        s.status = 1;
        CHECK(wi::glyph_of(s) == GlyphKey{static_cast<std::uint8_t>(kind), false, true});
        s.capital = false;
        s.status = 3;
        CHECK(wi::glyph_of(s) == GlyphKey{8, false, false});
        s.status = 4;
        CHECK(wi::glyph_of(s) == GlyphKey{9, false, false});
    }
}

TEST_CASE("P4-04 10.2 item 6: settlement_kind_word and settlement_status_word") {
    const std::string_view kinds[] = {"", "hamlet", "village", "town", "city", "fortress", ""};
    const std::string_view status[] = {"", "thriving", "declining", "ruined", "faded", "", ""};
    for (std::int64_t v = 0; v <= 6; ++v) {
        CAPTURE(v);
        CHECK(WORDS_NS::settlement_kind_word(v) == kinds[static_cast<std::size_t>(v)]);
        CHECK(WORDS_NS::settlement_status_word(v) == status[static_cast<std::size_t>(v)]);
    }
    for (const std::int64_t v : {std::int64_t{-1}, std::int64_t{7}, std::int64_t{1000}, INT64_MIN, INT64_MAX}) {
        CHECK(WORDS_NS::settlement_kind_word(v).empty());
        CHECK(WORDS_NS::settlement_status_word(v).empty());
    }
}

}  // TEST_SUITE
