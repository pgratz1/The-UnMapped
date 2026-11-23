// Spec P4-01 13.2 items 1 to 6: the civilization colours of 5.5 (the worked table, an unknown key and the
// empty key, ordinals wrapping at 4), civilization_colours on T1 (two races interleaved by id), every row of
// section 4's tables on T1 at the years that decide it, effects and the entity lists on an event with two
// causes and a participant repeated across roles, the FatalError texts of 5.1, and load_history's errors
// (5.2) against the history report's.
//
// P4-03 (docs/specs/p4-03-history-view.md 7) replaces 5.5's colours for the six shipped race keys: item 1
// below now checks 5.5's shade rule on P4-01's base colours given explicitly (the rule that still serves
// every key without a row of P4-03 7.2), and item 2 the colours of P4-03 7.2. The table of 7.2 itself, the
// distance rule and the P4-03 library items are in timeline_borders_test.cpp.
//
// The T1 table hashes against the reference's copy (13.3) and the golden dumps are in
// timeline_golden_test.cpp. Written from the spec only; the expected values below were worked out by hand
// from worldgen/tests/data/timeline/t1-edges.toml and are also checked by
// `p4_timeline_reference.py check`.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "timeline_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

using namespace tltest;
using wi::Rgb;
using V = std::vector<std::int64_t>;

V vec(std::span<const std::int64_t> s) { return V(s.begin(), s.end()); }

std::vector<std::size_t> rel(const wh::Timeline& t, std::int64_t y) { return t.year_state(y).relations; }

// The message of the FatalError `f` raises, or "" when it raises none.
template <class F>
std::string fatal_text(F&& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

const Opened& t1() {
    static const Opened o = [] {
        db::WorldReader r = open_reader(t1_world());
        wh::LoadedHistory h = load(r);
        return Opened{std::move(r), std::move(h)};
    }();
    return o;
}

void check_holding(const wh::RegionHolding& h, std::int64_t region, std::int64_t civ, std::int64_t since,
                   std::int64_t event) {
    CHECK(h.region == region);
    CHECK(h.civ == civ);
    CHECK(h.since == since);
    CHECK(h.event == event);
}

void check_state(const wh::SettlementState& s, std::int64_t id, bool exists, std::int64_t owner, std::int64_t kind,
                 std::int64_t status, std::int64_t population, std::int64_t since) {
    CHECK(s.settlement == id);
    CHECK(s.exists == exists);
    CHECK(s.owner == owner);
    CHECK(s.kind == kind);
    CHECK(s.status == status);
    CHECK(s.population == population);
    CHECK(s.since == since);
}

}  // namespace

TEST_SUITE("timeline") {

TEST_CASE("P4-01 13.2 item 1 (P4-03 7.2): civilization_shade on P4-01's base colours; keys without a row") {
    CHECK(wi::TAG_COLOUR == 0x524C4F433A4D4E55ULL);
    struct Row {
        const char* key;
        Rgb s[4];
    };
    const Row rows[] = {
        {"elf", {{52, 150, 70}, {153, 202, 162}, {32, 93, 43}, {22, 65, 30}}},
        {"goblin", {{190, 160, 50}, {222, 207, 152}, {118, 100, 31}, {83, 70, 21}}},
        {"dwarf", {{160, 82, 45}, {207, 168, 150}, {100, 51, 28}, {70, 35, 19}}},
        {"faerie", {{150, 90, 200}, {202, 172, 227}, {93, 56, 125}, {65, 39, 87}}},
        {"human", {{50, 105, 200}, {152, 180, 227}, {31, 65, 125}, {21, 45, 87}}},
        {"orc", {{200, 40, 35}, {227, 147, 145}, {125, 25, 21}, {87, 17, 15}}},
    };
    for (const Row& r : rows) {
        CAPTURE(std::string(r.key));
        // P4-03 7.2: the six shipped keys no longer take these bases; the shade rule is unchanged
        const Rgb base = r.s[0];
        CHECK_FALSE(wi::race_base_colour(r.key) == base);
        for (std::int64_t k = 0; k < 4; ++k) {
            CAPTURE(k);
            CHECK(wi::civilization_shade(base, k) == r.s[k]);
        }
        // a fifth and sixth civilization of one race take the base and the light shade again
        CHECK(wi::civilization_shade(base, 4) == r.s[0]);
        CHECK(wi::civilization_shade(base, 5) == r.s[1]);
        CHECK(wi::civilization_shade(base, 11) == r.s[3]);
        // invariant 8: the four shades are distinct
        for (int a = 0; a < 4; ++a) {
            for (int b = a + 1; b < 4; ++b) CHECK_FALSE(r.s[a] == r.s[b]);
        }
    }
    // keys outside the table: from the key's hash (troll: h = 0x90674847EA21F9EA; empty: 0x665E00601317AE04)
    CHECK(wi::race_base_colour("troll") == Rgb{194, 204, 68});
    CHECK(wi::race_base_colour("") == Rgb{50, 157, 62});
    // channel j = 48 + floor(((h >> 8j) & 0xFF) * 160 / 255) of the stated hashes, recomputed here
    const auto channel = [](std::uint64_t h, int j) {
        return static_cast<std::uint8_t>(48U + ((h >> (8 * j)) & 0xFFU) * 160U / 255U);
    };
    CHECK(wi::race_base_colour("troll") ==
          Rgb{channel(0x90674847EA21F9EAULL, 0), channel(0x90674847EA21F9EAULL, 1), channel(0x90674847EA21F9EAULL, 2)});
    // byte-exact comparison: no case folding, no trimming
    CHECK_FALSE(wi::race_base_colour("Elf") == Rgb{40, 190, 70});
    CHECK_FALSE(wi::race_base_colour("elf ") == Rgb{40, 190, 70});
    // the unknown examples have four distinct shades too (invariant 8)
    for (const char* key : {"troll", ""}) {
        const Rgb b = wi::race_base_colour(key);
        std::vector<Rgb> s;
        for (std::int64_t k = 0; k < 4; ++k) s.push_back(wi::civilization_shade(b, k));
        for (std::size_t a = 0; a < 4; ++a) {
            for (std::size_t c = a + 1; c < 4; ++c) CHECK_FALSE(s[a] == s[c]);
        }
    }
    // the shade rule on the channel extremes
    CHECK(wi::civilization_shade(Rgb{0, 255, 16}, 1) == Rgb{127, 255, 135});
    CHECK(wi::civilization_shade(Rgb{0, 255, 16}, 2) == Rgb{0, 159, 10});
    CHECK(wi::civilization_shade(Rgb{0, 255, 16}, 3) == Rgb{0, 111, 7});
}

TEST_CASE("P4-01 13.2 item 2: civilization_colours counts ordinals per race, not per id") {
    const wh::HistoryTables& tb = t1().h.tables;
    REQUIRE(tb.civilizations.size() == 4U);
    // c1 elf, c2 dwarf, c3 elf, c4 dwarf; P4-03 7.2 (11.1 item 3)
    const std::vector<Rgb> c = wi::civilization_colours(tb);
    REQUIRE(c.size() == 4U);
    CHECK(c[0] == Rgb{40, 190, 70});    // elf, ordinal 0
    CHECK(c[1] == Rgb{220, 105, 25});   // dwarf, ordinal 0
    CHECK(c[2] == Rgb{150, 230, 150});  // elf, ordinal 1
    CHECK(c[3] == Rgb{245, 165, 105});  // dwarf, ordinal 1
    // a function of the tables only: the same on every call
    CHECK(wi::civilization_colours(tb) == c);
    // nothing depends on race ids: the same keys under swapped ids give the same colours
    wh::HistoryTables swapped = tb;
    for (db::RaceRow& r : swapped.races) r.id = 3 - r.id;
    std::sort(swapped.races.begin(), swapped.races.end(),
              [](const db::RaceRow& a, const db::RaceRow& b) { return a.id < b.id; });
    for (db::CivilizationRow& cv : swapped.civilizations) cv.race = 3 - cv.race;
    CHECK(wi::civilization_colours(swapped) == c);
}

TEST_CASE("P4-01 13.2 item 3: holders on T1 (held, lost and retaken; never held; freed; a change of holder)") {
    const wh::Timeline& t = t1().h.timeline;
    CHECK(t.end_year() == 2000);
    CHECK_FALSE(t.valid_year(-1));
    CHECK(t.valid_year(0));
    CHECK(t.valid_year(2000));
    CHECK_FALSE(t.valid_year(2001));
    // r2: [300, 1200) c3, free [1200, 1300), c3 again from 1300
    check_holding(t.region_holder(2, 0), 2, 0, 0, 0);
    check_holding(t.region_holder(2, 299), 2, 0, 0, 0);
    check_holding(t.region_holder(2, 300), 2, 3, 300, 4);
    check_holding(t.region_holder(2, 1199), 2, 3, 300, 4);
    check_holding(t.region_holder(2, 1200), 2, 0, 1200, 0);
    check_holding(t.region_holder(2, 1299), 2, 0, 1200, 0);
    check_holding(t.region_holder(2, 1300), 2, 3, 1300, 14);
    check_holding(t.region_holder(2, 2000), 2, 3, 1300, 14);
    // r3: c2 [1000, 1600), then c1 from 1600 (no free year between)
    check_holding(t.region_holder(3, 999), 3, 0, 0, 0);
    check_holding(t.region_holder(3, 1000), 3, 2, 1000, 7);
    check_holding(t.region_holder(3, 1599), 3, 2, 1000, 7);
    check_holding(t.region_holder(3, 1600), 3, 1, 1600, 23);
    // r4 freed in 1800: free since its last to_year
    check_holding(t.region_holder(4, 1009), 4, 0, 0, 0);
    check_holding(t.region_holder(4, 1010), 4, 4, 1010, 9);
    check_holding(t.region_holder(4, 1799), 4, 4, 1010, 9);
    check_holding(t.region_holder(4, 1800), 4, 0, 1800, 0);
    check_holding(t.region_holder(4, 2000), 4, 0, 1800, 0);
    // r6 never held: free since 0 at every year
    for (const std::int64_t y : {0, 1, 1000, 1999, 2000}) {
        CAPTURE(y);
        check_holding(t.region_holder(6, y), 6, 0, 0, 0);
    }
    // r1 held from year 0
    check_holding(t.region_holder(1, 0), 1, 1, 0, 2);
    check_holding(t.region_holder(1, 2000), 1, 1, 0, 2);
    // the change years: every from_year and non-NULL to_year
    CHECK(vec(t.holder_change_years()) == V{0, 300, 1000, 1010, 1200, 1300, 1600, 1800});
    // the year state lists one holding per region row, ascending id (no region 5)
    const wh::YearState s = t.year_state(1250);
    REQUIRE(s.regions.size() == 5U);
    const V regions = {1, 2, 3, 4, 6};
    for (std::size_t i = 0; i < 5; ++i) CHECK(s.regions[i].region == regions[i]);
    check_holding(s.regions[1], 2, 0, 1200, 0);
}

TEST_CASE("P4-01 13.2 item 3: settlement states on T1 (census gap, ruined, faded, not yet founded)") {
    const wh::Timeline& t = t1().h.timeline;
    // s1: census at 0, 149 and 1500; between rows the last row holds
    check_state(t.settlement_state(1, 0), 1, true, 1, 1, 1, 200, 0);
    check_state(t.settlement_state(1, 148), 1, true, 1, 1, 1, 200, 0);
    check_state(t.settlement_state(1, 149), 1, true, 1, 2, 1, 600, 149);
    check_state(t.settlement_state(1, 1000), 1, true, 1, 2, 1, 600, 149);
    check_state(t.settlement_state(1, 1499), 1, true, 1, 2, 1, 600, 149);
    check_state(t.settlement_state(1, 1500), 1, true, 1, 3, 1, 900, 1500);
    // s2: founded 300, ruined (abandoned, owner 0) 1200, faded 1400
    check_state(t.settlement_state(2, 0), 2, false, 0, 0, 0, 0, 0);
    check_state(t.settlement_state(2, 299), 2, false, 0, 0, 0, 0, 0);
    check_state(t.settlement_state(2, 300), 2, true, 3, 1, 1, 100, 300);
    check_state(t.settlement_state(2, 1199), 2, true, 3, 1, 1, 100, 300);
    check_state(t.settlement_state(2, 1200), 2, true, 0, 1, 3, 0, 1200);
    check_state(t.settlement_state(2, 1399), 2, true, 0, 1, 3, 0, 1200);
    check_state(t.settlement_state(2, 1400), 2, true, 0, 1, 4, 0, 1400);
    check_state(t.settlement_state(2, 2000), 2, true, 0, 1, 4, 0, 1400);
    // s3 captured in 1600: the census owner changes
    check_state(t.settlement_state(3, 1599), 3, true, 2, 2, 1, 300, 1149);
    check_state(t.settlement_state(3, 1600), 3, true, 1, 2, 2, 200, 1600);
    // s5 ruined in 1800
    check_state(t.settlement_state(5, 2000), 5, true, 0, 1, 3, 0, 1800);
    // s6 founded in the last phase
    check_state(t.settlement_state(6, 1899), 6, false, 0, 0, 0, 0, 0);
    check_state(t.settlement_state(6, 1900), 6, true, 1, 1, 1, 50, 1900);
    // at E the end state of the settlement rows (invariant 3)
    for (const db::SettlementRow& row : t1().h.tables.settlements) {
        CAPTURE(row.id);
        const wh::SettlementState s = t.settlement_state(row.id, 2000);
        CHECK(s.exists);
        CHECK(s.owner == row.owner_civ);
        CHECK(s.kind == row.kind);
        CHECK(s.status == row.status);
        CHECK(s.population == row.population);
    }
    // one entry per settlement, index id - 1
    const wh::YearState st = t.year_state(1250);
    REQUIRE(st.settlements.size() == 6U);
    for (std::size_t i = 0; i < 6; ++i) CHECK(st.settlements[i].settlement == static_cast<std::int64_t>(i + 1));
    check_state(st.settlements[3], 4, false, 0, 0, 0, 0, 0);
    check_state(st.settlements[1], 2, true, 0, 1, 3, 0, 1200);
}

TEST_CASE("P4-01 13.2 item 3: relations, civilizations, phase and age on T1") {
    const wh::Timeline& t = t1().h.timeline;
    // relation rows in primary-key order: 0 war c1-c2 [1500, 1600), 1 enmity c1-c4 from 1100 (open),
    // 2 war c2-c4 1550-1550, 3 enmity c3-c4 [1300, 1999)
    REQUIRE(t.relations().size() == 4U);
    CHECK(t.relations()[0].kind == "war");
    CHECK(t.relations()[0].b_id == 2);
    CHECK(t.relations()[2].since == 1550);
    CHECK(t.relations()[2].until == std::optional<std::int64_t>(1550));
    using R = std::vector<std::size_t>;
    CHECK(rel(t, 1099) == R{});
    CHECK(rel(t, 1100) == R{1});
    CHECK(rel(t, 1299) == R{1});
    CHECK(rel(t, 1300) == R{1, 3});
    CHECK(rel(t, 1499) == R{1, 3});
    CHECK(rel(t, 1500) == R{0, 1, 3});
    CHECK(rel(t, 1550) == R{0, 1, 3});  // the war of 1550-1550 is in force in no year
    CHECK(rel(t, 1599) == R{0, 1, 3});
    CHECK(rel(t, 1600) == R{1, 3});
    CHECK(rel(t, 1998) == R{1, 3});
    CHECK(rel(t, 1999) == R{1});
    CHECK(rel(t, 2000) == R{1});  // at E exactly the open rows
    for (std::int64_t y = 0; y <= 2000; ++y) {
        const R r = rel(t, y);
        if (std::find(r.begin(), r.end(), std::size_t{2}) != r.end()) {
            FAIL_CHECK("relation 2 (until = since) in force at " << y);
        }
    }
    // civilizations alive: c2 ends in 1600 and is not alive in 1600
    CHECK(t.year_state(0).civilizations == V{1});
    CHECK(t.year_state(299).civilizations == V{1});
    CHECK(t.year_state(300).civilizations == V{1, 3});
    CHECK(t.year_state(1000).civilizations == V{1, 2, 3});
    CHECK(t.year_state(1010).civilizations == V{1, 2, 3, 4});
    CHECK(t.year_state(1599).civilizations == V{1, 2, 3, 4});
    CHECK(t.year_state(1600).civilizations == V{1, 3, 4});
    CHECK(t.year_state(2000).civilizations == V{1, 3, 4});
    // phase and age, with Y = E in no phase and in the last age
    const std::pair<std::int64_t, std::int64_t> pa[] = {{0, 1}, {149, 1}, {150, 2}, {999, 2}, {1000, 3},
                                                        {1549, 5}, {1550, 6}, {1999, 7}, {2000, 0}};
    for (const auto& [y, p] : pa) {
        CAPTURE(y);
        const wh::YearState s = t.year_state(y);
        CHECK(s.year == y);
        CHECK(s.phase == p);
    }
    CHECK(t.year_state(0).age == 1);
    CHECK(t.year_state(999).age == 1);
    CHECK(t.year_state(1000).age == 2);
    CHECK(t.year_state(1999).age == 2);
    CHECK(t.year_state(2000).age == 2);
}

TEST_CASE("P4-01 13.2 item 3: events of a year, multi-year events, previous and next event year on T1") {
    const wh::Timeline& t = t1().h.timeline;
    const auto ev = [&](std::int64_t y) { return t.year_state(y).events; };
    const auto st = [&](std::int64_t y) { return t.year_state(y).events_starting; };
    CHECK(ev(0) == V{1, 2, 3});
    CHECK(st(0) == V{1, 2, 3});
    CHECK(ev(1) == V{});
    // e12 1150-1499: at its start, in the middle with an event of its own, and at its end
    CHECK(ev(1150) == V{12});
    CHECK(st(1150) == V{12});
    CHECK(ev(1200) == V{12, 13});
    CHECK(st(1200) == V{13});
    CHECK(ev(1499) == V{12});
    CHECK(st(1499) == V{});
    CHECK(ev(1500) == V{16, 17});
    // e22 1590-1600 merged before the events starting in 1600
    CHECK(ev(1590) == V{22});
    CHECK(ev(1595) == V{22});
    CHECK(st(1595) == V{});
    CHECK(ev(1600) == V{22, 23, 24});
    CHECK(st(1600) == V{23, 24});
    CHECK(ev(1601) == V{});
    // E - 1 and E
    CHECK(ev(1999) == V{27, 28});
    CHECK(st(1999) == V{28});
    CHECK(ev(2000) == V{});
    CHECK(st(2000) == V{});
    // previous / next by start year
    CHECK(t.previous_event_year(0) == std::nullopt);
    CHECK(t.previous_event_year(1) == std::optional<std::int64_t>(0));
    CHECK(t.previous_event_year(300) == std::optional<std::int64_t>(0));
    CHECK(t.previous_event_year(1160) == std::optional<std::int64_t>(1150));
    CHECK(t.previous_event_year(1595) == std::optional<std::int64_t>(1590));
    CHECK(t.previous_event_year(1999) == std::optional<std::int64_t>(1950));
    CHECK(t.previous_event_year(2000) == std::optional<std::int64_t>(1999));
    CHECK(t.next_event_year(0) == std::optional<std::int64_t>(300));
    CHECK(t.next_event_year(1151) == std::optional<std::int64_t>(1200));
    CHECK(t.next_event_year(1595) == std::optional<std::int64_t>(1600));
    CHECK(t.next_event_year(1950) == std::optional<std::int64_t>(1999));
    CHECK(t.next_event_year(1999) == std::nullopt);
    CHECK(t.next_event_year(2000) == std::nullopt);
}

TEST_CASE("P4-01 13.2 item 4: effects and the entity lists on an event with two causes and repeated participants") {
    const wh::Timeline& t = t1().h.timeline;
    // e21: causes 17 and 19; c1 as host and guest; s1 as place and site; r1 as place, r6 as participant only
    CHECK(vec(t.effects(17)) == V{21, 22});
    CHECK(vec(t.effects(19)) == V{20, 21});
    CHECK(vec(t.effects(21)) == V{});
    CHECK(vec(t.effects(1)) == V{2, 4});
    CHECK(vec(t.effects(6)) == V{7, 9});
    CHECK(vec(t.effects(11)) == V{16, 17});
    CHECK(vec(t.effects(16)) == V{17});
    CHECK(vec(t.effects(28)) == V{});
    CHECK(vec(t.events_of_civilization(1)) == V{2, 3, 11, 12, 17, 21, 22, 23, 24, 26, 27});
    CHECK(vec(t.events_of_civilization(2)) == V{7, 8, 12, 17, 18, 19, 20, 22, 23, 24});
    CHECK(vec(t.events_of_civilization(3)) == V{4, 5, 12, 13, 14, 27});
    CHECK(vec(t.events_of_civilization(4)) == V{9, 10, 11, 12, 18, 19, 20, 21, 25, 27});
    CHECK(vec(t.events_of_region(1)) == V{1, 2, 3, 21, 26});
    CHECK(vec(t.events_of_region(2)) == V{4, 5, 13, 14, 15});
    CHECK(vec(t.events_of_region(3)) == V{6, 7, 8, 17, 22, 23, 24});
    CHECK(vec(t.events_of_region(4)) == V{9, 10, 11, 18, 19, 25});
    CHECK(vec(t.events_of_region(6)) == V{21});
    CHECK(vec(t.events_of_settlement(1)) == V{3, 21});
    CHECK(vec(t.events_of_settlement(2)) == V{5, 13, 15});
    CHECK(vec(t.events_of_settlement(3)) == V{8, 22, 23});
    CHECK(vec(t.events_of_settlement(4)) == V{14});
    CHECK(vec(t.events_of_settlement(5)) == V{10, 25});
    CHECK(vec(t.events_of_settlement(6)) == V{26});
    // e in effects(c) iff c is among e's causes (invariant 6), over the whole log
    const wh::HistoryView& view = t1().h.view;
    for (const wh::HistoryView::Event& e : view.events()) {
        for (const wh::HistoryView::Event& f : view.events()) {
            const bool caused = std::find(f.causes.begin(), f.causes.end(), e.row.id) != f.causes.end();
            const V eff = vec(t.effects(e.row.id));
            CHECK((std::find(eff.begin(), eff.end(), f.row.id) != eff.end()) == caused);
        }
    }
    // the count of a civilization's events is the digest's
    const wh::HistoryDigest d = wh::digest_history(view, t1().h.tables);
    for (const wh::CivilizationDigest& c : d.civilizations) {
        CAPTURE(c.id);
        CHECK(static_cast<std::int64_t>(t.events_of_civilization(c.id).size()) == c.events);
    }
}

TEST_CASE("P4-01 13.2 item 5: FatalError texts for a year or id outside the world") {
    const wh::Timeline& t = t1().h.timeline;
    for (const std::int64_t y : {std::int64_t{-1}, std::int64_t{2001}}) {
        const std::string want = "timeline: year " + std::to_string(y) + " outside 0 to 2000";
        CAPTURE(y);
        CHECK(fatal_text([&] { (void)t.region_holder(1, y); }) == want);
        CHECK(fatal_text([&] { (void)t.settlement_state(1, y); }) == want);
        CHECK(fatal_text([&] { (void)t.year_state(y); }) == want);
        wh::YearState out;
        CHECK(fatal_text([&] { t.year_state(y, out); }) == want);
        CHECK(fatal_text([&] { (void)t.previous_event_year(y); }) == want);
        CHECK(fatal_text([&] { (void)t.next_event_year(y); }) == want);
    }
    CHECK(fatal_text([&] { (void)t.year_state(-9223372036854775807LL - 1); }) ==
          "timeline: year -9223372036854775808 outside 0 to 2000");
    // region 5 has no region row (T1's ids are 1, 2, 3, 4, 6)
    CHECK(fatal_text([&] { (void)t.region_holder(5, 0); }) == "timeline: no region 5");
    CHECK(fatal_text([&] { (void)t.region_holder(0, 0); }) == "timeline: no region 0");
    CHECK(fatal_text([&] { (void)t.region_holder(7, 0); }) == "timeline: no region 7");
    CHECK(fatal_text([&] { (void)t.events_of_region(5); }) == "timeline: no region 5");
    CHECK(fatal_text([&] { (void)t.settlement_state(7, 0); }) == "timeline: no settlement 7");
    CHECK(fatal_text([&] { (void)t.settlement_state(0, 0); }) == "timeline: no settlement 0");
    CHECK(fatal_text([&] { (void)t.events_of_settlement(7); }) == "timeline: no settlement 7");
    CHECK(fatal_text([&] { (void)t.events_of_civilization(5); }) == "timeline: no civilization 5");
    CHECK(fatal_text([&] { (void)t.events_of_civilization(0); }) == "timeline: no civilization 0");
    CHECK(fatal_text([&] { (void)t.effects(29); }) == "timeline: no event 29");
    CHECK(fatal_text([&] { (void)t.effects(0); }) == "timeline: no event 0");
    // valid calls raise nothing
    CHECK(fatal_text([&] { (void)t.region_holder(6, 2000); }).empty());
    CHECK(fatal_text([&] { (void)t.effects(28); }).empty());
}

TEST_CASE("P4-01 13.2 item 6: load_history refuses what the report refuses, with the same message") {
    // a world without history: --until chronology
    {
        WorldSpec c;
        c.name = "until-chronology-512";
        c.seed = 1;
        c.size = 512;
        c.until = "chronology";
        const db::WorldReader r = open_reader(world(c));
        const auto h = wh::load_history(r);
        REQUIRE_FALSE(h.ok());
        CHECK(h.error().code == db::ErrorCode::history_missing);
        const auto rep = wi::build_history_report(r, wi::HistoryLevel::summary);
        REQUIRE_FALSE(rep.ok());
        CHECK(h.error().code == rep.error().code);
        CHECK(h.error().message == rep.error().message);
    }
    // corruptions of T1 that read_history_tables refuses (P3-07 5.3)
    struct Corruption {
        const char* sql;
        const char* message;
    };
    const Corruption cases[] = {
        {"UPDATE settlement_census SET population = 81 WHERE settlement = 4 AND year = 1300",
         "corrupt table 'settlement_census': settlement 4: the last row differs from the settlement's end state"},
        {"UPDATE region_control SET to_year = 1350 WHERE region = 2 AND from_year = 300",
         "corrupt table 'region_control': row (2, 1300): overlaps the previous interval"},
        {"UPDATE relation SET until = 2000 WHERE a_id = 3",
         "corrupt table 'relation': row (3, 3, 3, 4, enmity, 1300): until 2000 outside 1300 to 1999"},
    };
    for (const Corruption& c : cases) {
        CAPTURE(std::string(c.sql));
        const std::string path = t1_copy("corrupt");
        REQUIRE(db::testing::execute_on_file(path, c.sql).ok());
        const db::WorldReader r = open_reader(path);
        std::string got;
        bool threw = false;
        try {
            const auto h = wh::load_history(r);
            REQUIRE_FALSE(h.ok());
            CHECK(h.error().code == db::ErrorCode::corrupt_layer);
            CHECK(h.error().message == c.message);
            const auto rep = wi::build_history_report(r, wi::HistoryLevel::full);
            REQUIRE_FALSE(rep.ok());
            CHECK(rep.error().message == h.error().message);
        } catch (const worldgen::FatalError& e) {
            threw = true;
            FAIL_CHECK("FatalError: " << e.what());
        }
        CHECK_FALSE(threw);
    }
    // the two reads disagree (5.2 item 3): a view and tables of different worlds
    {
        const db::WorldReader r1 = open_reader(t1_world());
        const db::WorldReader r2 = open_reader(hrtest::fixture_world("h2-minimal"));
        auto v1 = wh::HistoryView::load(r1);
        auto t1t = wh::read_history_tables(r1);
        auto t2t = wh::read_history_tables(r2);
        REQUIRE(v1.ok());
        REQUIRE(t1t.ok());
        REQUIRE(t2t.ok());
        CHECK(wh::history_reads_disagree(v1.value(), t1t.value()) == std::nullopt);
        CHECK(wh::history_reads_disagree(v1.value(), t2t.value()) ==
              std::optional<std::string>("corrupt table 'event': the file changed while it was read"));
    }
    // a world the report accepts is accepted, and its parts are the two reads' (P4-00 V3)
    {
        const db::WorldReader r = open_reader(t1_world());
        const wh::LoadedHistory h = load(r);
        auto tb = wh::read_history_tables(r);
        REQUIRE(tb.ok());
        CHECK(h.tables.end_year == tb.value().end_year);
        CHECK(h.tables.region_control == tb.value().region_control);
        CHECK(h.tables.relations == tb.value().relations);
        CHECK(h.view.events().size() == 28U);
        CHECK(wi::build_history_report(r, wi::HistoryLevel::summary).ok());
    }
}

TEST_CASE("P4-01 5.1: a moved LoadedHistory stays valid; the timeline holds no reference into its inputs") {
    const db::WorldReader r = open_reader(t1_world());
    wh::LoadedHistory a = load(r);
    const std::string before = static_dump(a) + year_dump(a.timeline.year_state(1600));
    std::vector<wh::LoadedHistory> moved;
    moved.push_back(std::move(a));
    moved.reserve(8);  // reallocate: the element moves again
    CHECK(static_dump(moved.front()) + year_dump(moved.front().timeline.year_state(1600)) == before);
    // a timeline built from copies that are then destroyed still answers
    std::optional<wh::Timeline> t;
    {
        const wh::LoadedHistory b = load(r);
        t = wh::Timeline::build(b.view, b.tables);
    }
    CHECK(year_dump(t->year_state(1600)) == year_dump(moved.front().timeline.year_state(1600)));
    CHECK(vec(t->events_of_civilization(1)) == V{2, 3, 11, 12, 17, 21, 22, 23, 24, 26, 27});
}

}  // TEST_SUITE
