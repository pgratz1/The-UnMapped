// P4-05 12.1 F3 and F5 replace the ruin and faded ruin glyphs and the plague colour.
// Spec P4-04 10.2 item 4: draw_markers on a 32 x 24 canvas: each class colour (halo, fill by owner, no owner,
// faded, ring by symbol, ink), clipping at all four edges (against a larger canvas with the markers shifted),
// scale 1 and 3 (scale 3 equals scale 1 scaled; invariant 9), the order of 6.1 where a ring, a ruin and two
// settlements overlap, class 0 and 4 cells writing nothing, and the FatalErrors of section 4. The pixel
// expectations are the same as reference/p4_markers_reference.py check_draw_order.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "markers_support.hpp"
#include "worldgen/core/error.hpp"

namespace {

using namespace mktest;
using wi::EventSymbol;
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

constexpr Rgb BG{7, 77, 177};
const std::vector<Rgb> COLOURS = {Rgb{10, 20, 30}, Rgb{40, 50, 60}};

std::vector<std::uint8_t> canvas(std::uint32_t w, std::uint32_t h, std::uint32_t k, Rgb c = BG) {
    std::vector<std::uint8_t> v;
    v.reserve(std::size_t{3} * w * h * k * k);
    for (std::size_t i = 0; i < std::size_t{w} * h * k * k; ++i) {
        v.push_back(c.r);
        v.push_back(c.g);
        v.push_back(c.b);
    }
    return v;
}

Rgb px(const std::vector<std::uint8_t>& img, std::uint32_t w, std::int64_t x, std::int64_t y) {
    const std::size_t i = 3 * (static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x));
    return Rgb{img[i], img[i + 1], img[i + 2]};
}

wi::SettlementMarker st(std::int64_t id, std::int64_t x, std::int64_t y, std::int64_t kind, std::int64_t status,
                        std::int64_t owner, bool capital) {
    wi::SettlementMarker m;
    m.settlement = id;
    m.x = x;
    m.y = y;
    m.kind = kind;
    m.status = status;
    m.owner = owner;
    m.capital = capital;
    if (status >= 3) m.ruined_year = 0;
    return m;
}

// The scene of the reference's check_draw_order: a ring (sack) at (10, 10) under a capital village there, a
// declining town of c2 at (17, 10), a ruin at (3, 16), an unowned hamlet at (0, 23) and a pass at (30, 1).
wi::YearMarkers scene(std::int64_t dx = 0, std::int64_t dy = 0) {
    wi::YearMarkers m;
    m.year = 0;
    m.passes.push_back(wi::PassMarker{1, 30 + dx, 1 + dy, 1});
    m.places.push_back(wi::PlaceMarker{10 + dx, 10 + dy, EventSymbol::sack, {5}});
    m.settlements.push_back(st(1, 10 + dx, 10 + dy, 2, 1, 1, true));
    m.settlements.push_back(st(2, 17 + dx, 10 + dy, 3, 2, 2, false));
    m.settlements.push_back(st(3, 3 + dx, 16 + dy, 1, 3, 0, false));
    m.settlements.push_back(st(4, 0 + dx, 23 + dy, 1, 1, 0, false));
    return m;
}

std::vector<std::uint8_t> upscale(const std::vector<std::uint8_t>& img, std::uint32_t w, std::uint32_t h,
                                  std::uint32_t k) {
    std::vector<std::uint8_t> out;
    out.reserve(img.size() * k * k);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t j = 0; j < k; ++j) {
            for (std::uint32_t x = 0; x < w; ++x) {
                for (std::uint32_t i = 0; i < k; ++i) {
                    const std::size_t o = 3 * (std::size_t{y} * w + x);
                    out.push_back(img[o]);
                    out.push_back(img[o + 1]);
                    out.push_back(img[o + 2]);
                }
            }
        }
    }
    return out;
}

}  // namespace

TEST_SUITE("markers") {

TEST_CASE("P4-04 10.2 item 4: the order of 6.1 and each class colour on a 32 x 24 canvas") {
    std::vector<std::uint8_t> img = canvas(32, 24, 1);
    wi::draw_markers(img, 32, 24, 1, scene(), COLOURS);
    struct P {
        std::int64_t x, y;
        Rgb want;
        const char* why;
    };
    const P want[] = {
        {10, 10, COLOURS[0], "capital village centre: its owner's colour"},
        {13, 10, wi::MARKER_INK, "capital ink over the town's halo (capitals drawn last)"},
        {15, 10, wi::MARKER_HALO, "capital halo over the town's fill"},
        {16, 10, BG, "declining town interior over the ring's inside: nothing written"},
        {17, 10, wi::MARKER_HALO, "declining town interior shows the ring's inner halo"},
        {18, 10, wi::MARKER_INK, "declining town interior shows the ring's ink"},
        {19, 10, COLOURS[1], "town fill over the ring's colour band: no ring covers a settlement"},
        {20, 10, wi::MARKER_INK, "town outline"},
        {21, 10, wi::MARKER_HALO, "town halo"},
        {10, 1, Rgb{200, 0, 200}, "ring band, sack colour"},
        {10, 0, wi::MARKER_INK, "ring outer ink (dy = -10)"},
        {3, 16, wi::MARKER_INK, "ruin over the ring's band"},
        {4, 16, wi::MARKER_INK, "ruin: the thick X's bar (dx = 1, P4-05 12.1 F3)"},
        {5, 16, wi::MARKER_HALO, "ruin halo over the ring's ink"},
        {0, 23, wi::MARKER_NO_OWNER, "unowned hamlet, clipped at two edges"},
        {30, 0, wi::MARKER_INK, "pass clipped at the top"},
        {31, 1, wi::MARKER_INK, "pass clipped at the right"},
        {31, 0, wi::MARKER_HALO, "pass halo"},
        {0, 0, BG, "untouched"},
        {25, 20, BG, "untouched"},
    };
    for (const P& p : want) {
        CAPTURE(p.why);
        CAPTURE(p.x);
        CAPTURE(p.y);
        const Rgb got = px(img, 32, p.x, p.y);
        CHECK(static_cast<int>(got.r) == static_cast<int>(p.want.r));
        CHECK(static_cast<int>(got.g) == static_cast<int>(p.want.g));
        CHECK(static_cast<int>(got.b) == static_cast<int>(p.want.b));
    }
    // invariant 9: every changed pixel is a marker colour
    const std::set<std::tuple<int, int, int>> allowed = {{0, 0, 0},     {240, 240, 240}, {128, 128, 128},
                                                         {200, 0, 200}, {10, 20, 30},    {40, 50, 60},
                                                         {7, 77, 177}};
    for (std::int64_t y = 0; y < 24; ++y) {
        for (std::int64_t x = 0; x < 32; ++x) {
            const Rgb c = px(img, 32, x, y);
            CHECK(allowed.count({c.r, c.g, c.b}) == 1U);
        }
    }
}

TEST_CASE("P4-04 10.2 item 4: faded ruins and every ring colour") {
    const std::pair<EventSymbol, Rgb> symbols[] = {{EventSymbol::plain, Rgb{255, 255, 255}},
                                                   {EventSymbol::plague, Rgb{0, 190, 120}},  // P4-05 F5
                                                   {EventSymbol::battle, Rgb{230, 30, 30}},
                                                   {EventSymbol::siege, Rgb{255, 150, 0}},
                                                   {EventSymbol::sack, Rgb{200, 0, 200}}};
    for (const auto& [sym, colour] : symbols) {
        wi::YearMarkers m;
        m.places.push_back(wi::PlaceMarker{16, 12, sym, {1}});
        m.settlements.push_back(st(1, 16, 12, 1, 4, 0, false));  // a faded ruin inside the ring
        std::vector<std::uint8_t> img = canvas(32, 24, 1);
        wi::draw_markers(img, 32, 24, 1, m, COLOURS);
        CHECK(px(img, 32, 25, 12) == colour);           // the band, dx = 9
        CHECK(px(img, 32, 16, 12) == wi::MARKER_FADED);  // the faded cross
        CHECK(px(img, 32, 18, 14) == wi::MARKER_FADED);
        // P4-05 12.1 F3: a halo on every empty cell next to the cross, none in the four gaps
        CHECK(px(img, 32, 17, 12) == wi::MARKER_HALO);
        CHECK(px(img, 32, 18, 13) == wi::MARKER_HALO);
        CHECK(px(img, 32, 19, 15) == wi::MARKER_HALO);
        CHECK(px(img, 32, 16, 9) == BG);
        CHECK(px(img, 32, 19, 12) == BG);
    }
}

TEST_CASE("P4-04 10.2 item 4: clipping at all four edges equals a crop of a larger canvas") {
    constexpr std::int64_t OFF = 12;  // more than the largest radius (11)
    std::vector<std::uint8_t> small = canvas(32, 24, 1);
    wi::draw_markers(small, 32, 24, 1, scene(), COLOURS);
    std::vector<std::uint8_t> big = canvas(32 + 2 * OFF, 24 + 2 * OFF, 1);
    wi::draw_markers(big, 32 + 2 * OFF, 24 + 2 * OFF, 1, scene(OFF, OFF), COLOURS);
    for (std::int64_t y = 0; y < 24; ++y) {
        for (std::int64_t x = 0; x < 32; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            CHECK(px(small, 32, x, y) == px(big, 32 + 2 * OFF, x + OFF, y + OFF));
        }
    }
    // markers wholly outside the canvas, also at negative cells, write nothing
    wi::YearMarkers out;
    out.settlements.push_back(st(1, -20, 5, 4, 1, 1, true));
    out.settlements.push_back(st(2, 60, 5, 4, 1, 1, false));
    out.settlements.push_back(st(3, 5, -20, 4, 1, 2, false));
    out.settlements.push_back(st(4, 5, 50, 4, 1, 2, false));
    out.places.push_back(wi::PlaceMarker{-12, -12, EventSymbol::battle, {1}});
    out.passes.push_back(wi::PassMarker{1, 40, 40, 0});
    std::vector<std::uint8_t> img = canvas(32, 24, 1);
    wi::draw_markers(img, 32, 24, 1, out, COLOURS);
    CHECK(img == canvas(32, 24, 1));
    // a city half outside the right edge at scale 2
    wi::YearMarkers half;
    half.settlements.push_back(st(1, 31, 12, 4, 1, 2, false));
    std::vector<std::uint8_t> s2 = canvas(32, 24, 2);
    wi::draw_markers(s2, 32, 24, 2, half, COLOURS);
    CHECK(px(s2, 64, 63, 24) == wi::MARKER_INK);  // S(1) is ink
    CHECK(px(s2, 64, 62, 24) == wi::MARKER_INK);
    CHECK(px(s2, 64, 59, 24) == COLOURS[1]);  // dx = -2: fill
}

TEST_CASE("P4-04 10.2 item 4: scale 3 equals scale 1 scaled (invariant 9)") {
    std::vector<std::uint8_t> one = canvas(32, 24, 1);
    wi::draw_markers(one, 32, 24, 1, scene(), COLOURS);
    for (const std::uint32_t k : {2U, 3U, 16U}) {
        CAPTURE(k);
        std::vector<std::uint8_t> big = canvas(32, 24, k);
        wi::draw_markers(big, 32, 24, k, scene(), COLOURS);
        CHECK(big == upscale(one, 32, 24, k));
    }
}

TEST_CASE("P4-04 10.2 item 4: an empty marker set changes nothing") {
    std::vector<std::uint8_t> img = canvas(32, 24, 1);
    wi::draw_markers(img, 32, 24, 1, wi::YearMarkers{}, COLOURS);
    CHECK(img == canvas(32, 24, 1));
    // no colours are needed when no standing settlement has an owner
    wi::YearMarkers m;
    m.settlements.push_back(st(1, 5, 5, 2, 1, 0, false));
    m.settlements.push_back(st(2, 20, 5, 2, 3, 0, false));
    std::vector<std::uint8_t> img2 = canvas(32, 24, 1);
    wi::draw_markers(img2, 32, 24, 1, m, std::span<const Rgb>{});
    CHECK(px(img2, 32, 5, 5) == wi::MARKER_NO_OWNER);
    CHECK(px(img2, 32, 20, 5) == wi::MARKER_INK);
}

TEST_CASE("P4-04 10.2 item 4: draw_markers FatalErrors") {
    const wi::YearMarkers m = scene();
    std::vector<std::uint8_t> img = canvas(32, 24, 1);
    // buffer size
    std::vector<std::uint8_t> short_buf(img.size() - 1);
    CHECK(fatal_text([&] { wi::draw_markers(short_buf, 32, 24, 1, m, COLOURS); }).rfind("draw_markers: ", 0) == 0);
    std::vector<std::uint8_t> long_buf(img.size() + 3);
    CHECK(fatal_text([&] { wi::draw_markers(long_buf, 32, 24, 1, m, COLOURS); }).rfind("draw_markers: ", 0) == 0);
    CHECK(fatal_text([&] { wi::draw_markers(img, 32, 24, 2, m, COLOURS); }).rfind("draw_markers: ", 0) == 0);
    // scale outside 1 ... 16
    std::vector<std::uint8_t> empty;
    CHECK(fatal_text([&] { wi::draw_markers(empty, 32, 24, 0, m, COLOURS); }).rfind("draw_markers: ", 0) == 0);
    std::vector<std::uint8_t> k17 = canvas(32, 24, 17);
    CHECK(fatal_text([&] { wi::draw_markers(k17, 32, 24, 17, m, COLOURS); }).rfind("draw_markers: ", 0) == 0);
    // an owner without a colour
    const std::vector<Rgb> one = {COLOURS[0]};
    CHECK(fatal_text([&] { wi::draw_markers(img, 32, 24, 1, m, one); }).rfind("draw_markers: ", 0) == 0);
    // the accepted cases do not raise
    CHECK(fatal_text([&] { wi::draw_markers(img, 32, 24, 1, m, COLOURS); }).empty());
    std::vector<std::uint8_t> k16 = canvas(32, 24, 16);
    CHECK(fatal_text([&] { wi::draw_markers(k16, 32, 24, 16, m, COLOURS); }).empty());
}

}  // TEST_SUITE
