// Spec P4-04 10.2 items 3 and 5 on fixture T2 (worldgen/tests/data/markers/t2-markers.toml): settlements before
// founding, standing, declining, ruined in R (standing at R - 1), faded in D (ruin at D - 1, absent or faded at
// D), a faded ruin whose ruin event is found only through ruin_faded, an owner change, capitals founded, moved
// and by secession, a civilization alive without a capital and one ended; events with a settlement, with only a
// region and with neither; two events at one cell (battle and capture: one place, sack); multi-year events in
// the middle of their span; Y = 0, E - 1 and E; the options; the passes; the two forms of markers_at
// (invariant 8); capital, ruined_year, ruin_event; the FatalErrors of section 4; MarkerIndex::load's errors
// (a malformed pass row with the report's message, region ids differing from the history's).
//
// Written from the spec only; the expected entries are worked out by hand from the fixture's rows (and agree
// with reference/p4_markers_reference.py check).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "markers_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"

namespace {

using namespace mktest;
using wi::EventSymbol;
using wi::PassMarker;
using wi::PlaceMarker;
using wi::SettlementMarker;
using V = std::vector<std::int64_t>;

template <class F>
std::string fatal_text(F&& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

const Indexed& t2() {
    static const Indexed o = open_indexed(t2_world());
    return o;
}

// settlement, x, y, kind, status, owner, population, since, founded, capital, ruined_year, ruin_event
SettlementMarker sm(std::int64_t s, std::int64_t x, std::int64_t y, std::int64_t kind, std::int64_t status,
                    std::int64_t owner, std::int64_t pop, std::int64_t since, std::int64_t founded, bool capital,
                    std::int64_t ruined = -1, std::int64_t ruin_event = 0) {
    SettlementMarker m;
    m.settlement = s;
    m.x = x;
    m.y = y;
    m.kind = kind;
    m.status = status;
    m.owner = owner;
    m.population = pop;
    m.since = since;
    m.founded = founded;
    m.capital = capital;
    m.ruined_year = ruined;
    m.ruin_event = ruin_event;
    return m;
}

const std::vector<PassMarker>& t2_pass_markers() {
    static const std::vector<PassMarker> v = {PassMarker{1, 250, 250, 1}, PassMarker{2, 10, 500, 0}};
    return v;
}

wi::YearMarkers at(std::int64_t y, const wi::MarkerOptions& opt = ALL) { return t2().index.markers_at(t2().h, y, opt); }

std::vector<std::int64_t> ids(const wi::YearMarkers& m) {
    std::vector<std::int64_t> v;
    for (const SettlementMarker& s : m.settlements) v.push_back(s.settlement);
    return v;
}

const SettlementMarker& entry(const wi::YearMarkers& m, std::int64_t s) {
    for (const SettlementMarker& e : m.settlements) {
        if (e.settlement == s) return e;
    }
    FAIL("no entry for settlement " << s << " at " << m.year);
    return m.settlements.front();
}

}  // namespace

TEST_SUITE("markers") {

TEST_CASE("P4-04 10.2 item 3: T2 loads; E and the passes") {
    CHECK(t2().h.tables.end_year == 2000);
    const auto ps = t2().index.passes();
    REQUIRE(ps.size() == 2U);
    CHECK(ps[0].id == 1);
    CHECK(ps[0].col_x == 250);
    CHECK(ps[0].col_y == 250);
    CHECK(ps[0].kind == 1);
    CHECK(ps[1].id == 2);
    CHECK(ps[1].col_x == 10);
    CHECK(ps[1].col_y == 500);
    CHECK(ps[1].kind == 0);
    // passes are independent of Y (invariant 7)
    for (const std::int64_t y : {0, 1, 999, 1999, 2000}) CHECK(at(y).passes == t2_pass_markers());
}

TEST_CASE("P4-04 10.2 item 3: T2 at Y = 0, before founding") {
    const wi::YearMarkers m = at(0);
    CHECK(m.year == 0);
    CHECK(m.settlements == std::vector<SettlementMarker>{sm(1, 100, 100, 1, 1, 1, 100, 0, 0, true)});
    // e1 and e2 at r1's seed cell (40, 50), e3 at s1's cell
    CHECK(m.places == std::vector<PlaceMarker>{PlaceMarker{40, 50, EventSymbol::plain, {1, 2}},
                                               PlaceMarker{100, 100, EventSymbol::plain, {3}}});
    CHECK(m.passes == t2_pass_markers());
    CHECK(ids(at(99)) == V{1});
    CHECK(ids(at(100)) == V{1, 2});
    CHECK(ids(at(299)) == V{1, 2, 5});
    CHECK(ids(at(1899)) == V{1, 2, 3, 4, 5, 6});
    CHECK(ids(at(1900)) == V{1, 2, 3, 4, 5, 6, 7});
}

TEST_CASE("P4-04 10.2 item 3: T2 at 799, the last year before the first ruin") {
    const wi::YearMarkers m = at(799);
    CHECK(m.settlements == std::vector<SettlementMarker>{
                               sm(1, 100, 100, 3, 1, 1, 1500, 400, 0, true), sm(2, 300, 100, 2, 1, 2, 300, 100, 100, true),
                               sm(3, 150, 300, 1, 1, 1, 80, 600, 600, false), sm(4, 60, 200, 5, 1, 1, 300, 500, 500, false),
                               sm(5, 320, 150, 1, 1, 2, 50, 200, 200, false), sm(6, 220, 420, 2, 1, 1, 200, 300, 300, false)});
    CHECK(m.places.empty());
}

TEST_CASE("P4-04 10.2 item 3: ruined in R, faded in D, the ruin event found through ruin_faded") {
    // s6: sacked in 800 (e11), faded in 1100 (e15, caused by e11); its row has ruined_year NULL, ruin_event 0
    CHECK(entry(at(799), 6) == sm(6, 220, 420, 2, 1, 1, 200, 300, 300, false));
    CHECK(entry(at(800), 6) == sm(6, 220, 420, 2, 3, 0, 0, 800, 300, false, 800, 11));
    CHECK(entry(at(1099), 6) == sm(6, 220, 420, 2, 3, 0, 0, 800, 300, false, 800, 11));
    CHECK(entry(at(1100), 6) == sm(6, 220, 420, 2, 4, 0, 0, 1100, 300, false, 800, 11));
    CHECK(entry(at(2000), 6) == sm(6, 220, 420, 2, 4, 0, 0, 1100, 300, false, 800, 11));
    // without the faded option the faded ruin is absent
    const wi::MarkerOptions no_faded{false, true, true};
    CHECK(ids(at(1099, no_faded)) == V{1, 2, 3, 4, 5, 6});
    CHECK(ids(at(1100, no_faded)) == V{1, 2, 3, 4, 5});
    CHECK(ids(at(2000, no_faded)) == V{1, 2, 3, 4, 5, 7});
    // the sack at 800 is the place of that year
    CHECK(at(800).places == std::vector<PlaceMarker>{PlaceMarker{220, 420, EventSymbol::sack, {11}}});
    // s5: abandoned in 1300 (e17, the row's ruin_event)
    CHECK(entry(at(1299), 5) == sm(5, 320, 150, 1, 1, 2, 50, 200, 200, false));
    CHECK(entry(at(1300), 5) == sm(5, 320, 150, 1, 3, 0, 0, 1300, 200, false, 1300, 17));
    CHECK(entry(at(2000), 5) == sm(5, 320, 150, 1, 3, 0, 0, 1300, 200, false, 1300, 17));
    // the index's answers
    const wi::MarkerIndex& ix = t2().index;
    const V ruined = {ix.ruined_year(1), ix.ruined_year(2), ix.ruined_year(3), ix.ruined_year(4),
                      ix.ruined_year(5), ix.ruined_year(6), ix.ruined_year(7)};
    CHECK(ruined == V{-1, -1, -1, -1, 1300, 800, -1});
    const V events = {ix.ruin_event(1), ix.ruin_event(2), ix.ruin_event(3), ix.ruin_event(4),
                      ix.ruin_event(5), ix.ruin_event(6), ix.ruin_event(7)};
    CHECK(events == V{0, 0, 0, 0, 17, 11, 0});
}

TEST_CASE("P4-04 10.2 item 3: declining and recovering; an owner change") {
    CHECK(entry(at(999), 1) == sm(1, 100, 100, 3, 1, 1, 1500, 400, 0, false));
    CHECK(entry(at(1000), 1) == sm(1, 100, 100, 3, 2, 1, 900, 1000, 0, false));
    CHECK(entry(at(1599), 1) == sm(1, 100, 100, 3, 2, 1, 900, 1000, 0, false));
    CHECK(entry(at(1600), 1) == sm(1, 100, 100, 3, 1, 1, 1000, 1600, 0, false));
    // s2 is taken by c1 in 1400: c2's capital until then
    CHECK(entry(at(1399), 2) == sm(2, 300, 100, 2, 1, 2, 300, 100, 100, true));
    CHECK(entry(at(1400), 2) == sm(2, 300, 100, 2, 1, 1, 200, 1400, 100, false));
}

TEST_CASE("P4-04 10.2 item 3: capitals founded, moved, by secession; a civilization without one, one ended") {
    const wi::MarkerIndex& ix = t2().index;
    const wh::LoadedHistory& h = t2().h;
    const std::int64_t years[] = {0, 99, 100, 899, 900, 1199, 1200, 1399, 1400, 1499, 1500, 1999, 2000};
    const V c1 = {1, 1, 1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    const V c2 = {0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0};  // 1400 ... 1499: alive, its seat owned by c1
    const V c3 = {0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3};
    V g1, g2, g3;
    for (const std::int64_t y : years) {
        g1.push_back(ix.capital(h, 1, y));
        g2.push_back(ix.capital(h, 2, y));
        g3.push_back(ix.capital(h, 3, y));
    }
    CHECK(g1 == c1);
    CHECK(g2 == c2);
    CHECK(g3 == c3);
    // the capital flag of the entries: at 900 the seat of c1 moves from s1 to s4
    CHECK(entry(at(899), 1).capital);
    CHECK_FALSE(entry(at(899), 4).capital);
    CHECK_FALSE(entry(at(900), 1).capital);
    CHECK(entry(at(900), 4).capital);
    // secession in 1200: s3 becomes c3's and its capital
    CHECK(entry(at(1199), 3) == sm(3, 150, 300, 2, 1, 1, 400, 1000, 600, false));
    CHECK(entry(at(1200), 3) == sm(3, 150, 300, 2, 1, 3, 450, 1200, 600, true));
    // at E: the capitals equal civilization.capital of the alive civilizations (c1 s4, c3 s3)
    const wi::YearMarkers e = at(2000);
    V caps;
    for (const SettlementMarker& s : e.settlements) {
        if (s.capital) caps.push_back(s.settlement);
    }
    CHECK(caps == V{3, 4});
    for (const db::CivilizationRow& c : h.tables.civilizations) {
        if (!c.ended_year) CHECK(ix.capital(h, c.id, 2000) == c.capital);
    }
}

TEST_CASE("P4-04 10.2 item 3: event places") {
    // 1400: e19 (claim, region r4 only: its seed cell), e20 (omen: no place), e21 battle and e22 capture at s2's
    // cell: one place, symbol sack; places in ascending smallest event id
    CHECK(at(1400).places == std::vector<PlaceMarker>{PlaceMarker{450, 450, EventSymbol::plain, {19}},
                                                      PlaceMarker{300, 100, EventSymbol::sack, {21, 22}}});
    // a multi-year siege, 1380 to 1399, in the middle of its span and at both ends
    for (const std::int64_t y : {1380, 1390, 1399}) {
        CAPTURE(y);
        CHECK(at(y).places == std::vector<PlaceMarker>{PlaceMarker{300, 100, EventSymbol::siege, {18}}});
    }
    CHECK(at(1379).places.empty());
    // 1000: an omen (no place) and a dwindling at s1
    CHECK(at(1000).places == std::vector<PlaceMarker>{PlaceMarker{100, 100, EventSymbol::plain, {14}}});
    // 1500: civ_ended without a place, the plague outbreak (1500-1502) at s1, a pilgrimage at the faded s6's cell
    CHECK(at(1500).places == std::vector<PlaceMarker>{PlaceMarker{100, 100, EventSymbol::plague, {24}},
                                                      PlaceMarker{220, 420, EventSymbol::plain, {25}}});
    CHECK(at(1501).places == std::vector<PlaceMarker>{PlaceMarker{100, 100, EventSymbol::plague, {24}}});
    CHECK(at(1503).places.empty());
    // the ring sits at the faded ruin's cell even when the ruin is not drawn (17 item 4)
    const wi::YearMarkers no_faded = t2().index.markers_at(t2().h, 1500, wi::MarkerOptions{false, true, true});
    CHECK(no_faded.places == at(1500).places);
    CHECK(ids(no_faded) == V{1, 2, 3, 4, 5});
    // battle_at_pass with only a region
    CHECK(at(1950).places == std::vector<PlaceMarker>{PlaceMarker{450, 450, EventSymbol::battle, {27}}});
    // E - 1 and E: the year E has no events (P4-01 4)
    CHECK(at(1999).places == std::vector<PlaceMarker>{PlaceMarker{150, 300, EventSymbol::plague, {28}}});
    CHECK(at(2000).places.empty());
}

TEST_CASE("P4-04 10.2 item 3: T2 at E and E - 1") {
    const std::vector<SettlementMarker> end = {
        sm(1, 100, 100, 3, 1, 1, 1000, 1600, 0, false),       sm(2, 300, 100, 2, 1, 1, 200, 1400, 100, false),
        sm(3, 150, 300, 2, 1, 3, 450, 1200, 600, true),       sm(4, 60, 200, 5, 1, 1, 300, 500, 500, true),
        sm(5, 320, 150, 1, 3, 0, 0, 1300, 200, false, 1300, 17), sm(6, 220, 420, 2, 4, 0, 0, 1100, 300, false, 800, 11),
        sm(7, 400, 300, 1, 1, 1, 40, 1900, 1900, false)};
    CHECK(at(2000).settlements == end);
    CHECK(at(1999).settlements == end);
    // invariant 2: the entries at E equal the rows' end state
    const wi::YearMarkers at_end = at(2000);
    for (const db::SettlementRow& s : t2().h.tables.settlements) {
        const SettlementMarker& m = entry(at_end, s.id);
        CHECK(m.owner == s.owner_civ);
        CHECK(m.kind == s.kind);
        CHECK(m.status == s.status);
        CHECK(m.population == s.population);
        CHECK(m.x == s.x);
        CHECK(m.y == s.y);
        CHECK(m.founded == s.founded_year);
    }
}

TEST_CASE("P4-04 10.2 item 3: the options") {
    const wi::YearMarkers all = at(1400);
    const wi::YearMarkers none = t2().index.markers_at(t2().h, 1400, wi::MarkerOptions{false, false, false});
    CHECK(none.year == 1400);
    CHECK(none.places.empty());
    CHECK(none.passes.empty());
    CHECK(ids(none) == V{1, 2, 3, 4, 5});
    const wi::YearMarkers def = t2().index.markers_at(t2().h, 1400, wi::MarkerOptions{});
    CHECK(ids(def) == V{1, 2, 3, 4, 5});
    CHECK(def.places == all.places);
    CHECK(def.passes == all.passes);
}

TEST_CASE("P4-04 invariant 8: the reusing form equals the returning one, independent of earlier calls") {
    const wi::MarkerIndex& ix = t2().index;
    const wh::LoadedHistory& h = t2().h;
    wi::YearMarkers out;
    wh::YearState st;
    // visit the years out of order with dirty buffers
    for (const std::int64_t y : {2000, 0, 1400, 800, 1100, 1999, 1, 1500, 1390, 0, 2000}) {
        CAPTURE(y);
        h.timeline.year_state(y, st);
        for (const wi::MarkerOptions& o : {ALL, wi::MarkerOptions{}, wi::MarkerOptions{false, false, false}}) {
            ix.markers_at(h, st, o, out);
            CHECK(out == ix.markers_at(h, y, o));
        }
    }
    // a second index built from the same world answers the same
    const Indexed again = open_indexed(t2_world());
    for (std::int64_t y = 0; y <= 2000; y += 37) CHECK(again.index.markers_at(again.h, y, ALL) == at(y));
}

TEST_CASE("P4-04 section 4: FatalErrors for a caller's year, id or state") {
    const wi::MarkerIndex& ix = t2().index;
    const wh::LoadedHistory& h = t2().h;
    // markers_at(history, year): as Timeline for a bad year
    for (const std::int64_t y : {std::int64_t{-1}, std::int64_t{2001}, INT64_MIN, INT64_MAX}) {
        CAPTURE(y);
        const std::string want = fatal_text([&] { (void)h.timeline.year_state(y); });
        REQUIRE_FALSE(want.empty());
        CHECK(fatal_text([&] { (void)ix.markers_at(h, y, ALL); }) == want);
        CHECK_FALSE(fatal_text([&] { (void)ix.capital(h, 1, y); }).empty());
    }
    // capital: no civilization
    CHECK(fatal_text([&] { (void)ix.capital(h, 0, 100); }) == "markers: no civilization 0");
    CHECK(fatal_text([&] { (void)ix.capital(h, 4, 100); }) == "markers: no civilization 4");
    CHECK(fatal_text([&] { (void)ix.capital(h, -1, 100); }) == "markers: no civilization -1");
    // ruined_year, ruin_event: no settlement
    CHECK(fatal_text([&] { (void)ix.ruined_year(0); }) == "markers: no settlement 0");
    CHECK(fatal_text([&] { (void)ix.ruined_year(8); }) == "markers: no settlement 8");
    CHECK(fatal_text([&] { (void)ix.ruin_event(0); }) == "markers: no settlement 0");
    CHECK(fatal_text([&] { (void)ix.ruin_event(8); }) == "markers: no settlement 8");
    // markers_at(history, state): a state of another world (settlement count) or a year out of range
    const tltest::Opened t1{tltest::open_reader(tltest::t1_world()), tltest::load(tltest::open_reader(tltest::t1_world()))};
    REQUIRE(t1.h.tables.settlements.size() != h.tables.settlements.size());
    wi::YearMarkers out;
    const std::string foreign = fatal_text([&] { ix.markers_at(h, t1.h.timeline.year_state(100), ALL, out); });
    CHECK(foreign.rfind("markers: ", 0) == 0);
    wh::YearState st = h.timeline.year_state(2000);
    st.year = 2001;
    CHECK(fatal_text([&] { ix.markers_at(h, st, ALL, out); }).rfind("markers: ", 0) == 0);
    st.year = -1;
    CHECK(fatal_text([&] { ix.markers_at(h, st, ALL, out); }).rfind("markers: ", 0) == 0);
}

TEST_CASE("P4-04 10.2 item 5: MarkerIndex::load: region ids differing from the history's") {
    // the history of T2 (regions 1 to 4) with the reader of T1 (regions 1, 2, 3, 4, 6)
    const db::WorldReader r1 = tltest::open_reader(tltest::t1_world());
    auto m = wi::MarkerIndex::load(r1, t2().h);
    REQUIRE_FALSE(m.ok());
    CHECK(m.code() == db::ErrorCode::corrupt_layer);
    CHECK(m.error().message == "corrupt table 'region': the file changed while it was read");
    // a region row removed from a copy after the history was loaded
    const std::string copy = copy_of(t2_world(), "region-gone");
    const db::WorldReader rc = tltest::open_reader(copy);
    const wh::LoadedHistory hc = tltest::load(rc);
    const db::WorldReader rc_before = tltest::open_reader(copy);
    REQUIRE(db::testing::execute_on_file(copy, "DELETE FROM region WHERE id = 4").ok());
    const db::WorldReader rc_after = tltest::open_reader(copy);
    auto m2 = wi::MarkerIndex::load(rc_after, hc);
    REQUIRE_FALSE(m2.ok());
    CHECK(m2.code() == db::ErrorCode::corrupt_layer);
    CHECK(m2.error().message == "corrupt table 'region': the file changed while it was read");
}

TEST_CASE("P4-04 10.2 item 5: MarkerIndex::load: a malformed pass row gives the report's message") {
    // on a generated world (the report checks pass rows against the elevation grid, and the index against
    // meta.param.map_size, which a generated world makes equal)
    const std::string src = tltest::world(tltest::property_world(42, 512));
    {
        const tltest::Opened o{tltest::open_reader(src), tltest::load(tltest::open_reader(src))};
        auto ok = wi::MarkerIndex::load(o.reader, o.h);
        REQUIRE(ok.ok());
        REQUIRE_FALSE(ok.value().passes().empty());
    }
    const char* updates[] = {
        "UPDATE pass SET kind = 2 WHERE id = 1",
        "UPDATE pass SET capped = 7 WHERE id = 1",
        "UPDATE pass SET kind = 0, capped = 1 WHERE id = 1",
        "UPDATE pass SET col_x = 512 WHERE id = 1",
        "UPDATE pass SET col_y = -1 WHERE id = 1",
        "UPDATE pass SET path = x'' WHERE id = 1",
        "UPDATE pass SET path = x'010203' WHERE id = 1",
        "UPDATE pass SET path = x'ffffffff' WHERE id = 1",
    };
    for (const char* sql : updates) {
        CAPTURE(std::string(sql));
        const std::string copy = copy_of(src, "pass");
        REQUIRE(db::testing::execute_on_file(copy, sql).ok());
        const db::WorldReader r = tltest::open_reader(copy);
        const wh::LoadedHistory h = tltest::load(r);  // the history does not read table pass
        auto m = wi::MarkerIndex::load(r, h);
        REQUIRE_FALSE(m.ok());
        CHECK(m.code() == db::ErrorCode::corrupt_layer);
        CHECK(m.error().message.rfind("corrupt table 'pass': ", 0) == 0);
        // the reason is the report's
        auto rep = wi::build_report(r);
        REQUIRE_FALSE(rep.ok());
        CHECK(rep.code() == db::ErrorCode::corrupt_layer);
        CHECK(m.error().message == rep.error().message);
    }
}

}  // TEST_SUITE
