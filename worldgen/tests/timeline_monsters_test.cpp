// Spec P5-02 (docs/specs/p5-02-beasts-monsters.md r0) section 13 (P4-01 r4), test 19.1 item 16: Timeline's
// sites_existing, site_at, monster_at, beast_count and SettlementState::ending at the years around each
// transition, on the fixture H13 (worldgen/tests/data/history-report/h13-v4.toml; its rows are described in
// history_report_v4_test.cpp). The names of 13 are used as declared; SettlementState::ending is the spec's text.
// Written by the test writer from the spec before the implementation.

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "timeline_support.hpp"

namespace {

using namespace tltest;

const wh::LoadedHistory& h13() {
    static const db::WorldReader r = open_reader(hrtest::fixture_world("h13-v4"));
    static const wh::LoadedHistory h = load(r);
    return h;
}

std::vector<std::int64_t> ids(std::span<const std::int64_t> s) { return std::vector<std::int64_t>(s.begin(), s.end()); }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-02 13: sites_existing(Y): the sites with origin_year <= Y, ascending id") {
    const wh::Timeline& t = h13().timeline;
    CHECK(ids(t.sites_existing(0)).empty());
    CHECK(ids(t.sites_existing(499)).empty());
    CHECK(ids(t.sites_existing(500)) == std::vector<std::int64_t>{1});
    CHECK(ids(t.sites_existing(975)) == std::vector<std::int64_t>{1, 2});
    CHECK(ids(t.sites_existing(1299)) == std::vector<std::int64_t>{1, 2});
    CHECK(ids(t.sites_existing(1300)) == std::vector<std::int64_t>{1, 2, 3});
    CHECK(ids(t.sites_existing(1470)) == std::vector<std::int64_t>{1, 2, 3, 4, 5});
    CHECK(ids(t.sites_existing(2000)) == std::vector<std::int64_t>{1, 2, 3, 4, 5});
}

TEST_CASE("P5-02 13: site_at(s, Y): open iff ended_year NULL or Y < ended_year; the ending once ended") {
    const wh::Timeline& t = h13().timeline;
    // si1 Grakh's Hollow: 500 to 700 (slain)
    CHECK(t.site_at(1, 699).open);
    CHECK_FALSE(t.site_at(1, 700).open);
    CHECK(t.site_at(1, 700).ending == 1);
    CHECK(t.site_at(1, 699).ending == 0);
    // si2: moved at 1300 (ending 3); si3 opens at 1300 at Arnwick's cell
    CHECK(t.site_at(2, 1299).open);
    CHECK_FALSE(t.site_at(2, 1300).open);
    CHECK(t.site_at(2, 1300).ending == 3);
    CHECK(t.site_at(3, 1300).open);
    const wh::SiteState s3 = t.site_at(3, 1999);
    CHECK(s3.site == 3);
    CHECK(s3.kind == 1);
    CHECK(s3.region == 1);
    CHECK(s3.x == 14);
    CHECK(s3.y == 12);
    CHECK(s3.settlement == 2);
    CHECK(s3.monster == 2);
    CHECK(s3.founder == 0);
    // si5 Bel Tower: an outpost of civilization 2, left in 1700 (quiet, 6)
    const wh::SiteState s5 = t.site_at(5, 1699);
    CHECK(s5.kind == 2);
    CHECK(s5.founder == 2);
    CHECK(s5.monster == 0);
    CHECK(s5.open);
    CHECK_FALSE(t.site_at(5, 1700).open);
    CHECK(t.site_at(5, 1700).ending == 6);
    CHECK(t.site_at(4, 2000).open);   // Arn Watch, never left
}

TEST_CASE("P5-02 13: monster_at(m, Y): alive, awake by the last of its events 44 to 52, the lair at Y") {
    const wh::Timeline& t = h13().timeline;
    // m1 Old Grakh: appears 500 (awake), slain 700
    CHECK_FALSE(t.monster_at(1, 499).alive);
    const wh::MonsterState g = t.monster_at(1, 500);
    CHECK(g.monster == 1);
    CHECK(g.kind == 4);
    CHECK(g.alive);
    CHECK(g.awake);
    CHECK(g.lair_site == 1);
    CHECK(g.region == 3);
    CHECK(t.monster_at(1, 699).alive);
    CHECK_FALSE(t.monster_at(1, 700).alive);
    // m2 Saelith: appears 975, raids and overruns 1300 (the lair moves to si3 in region 1), sleeps 1400
    const wh::MonsterState a = t.monster_at(2, 1299);
    CHECK(a.alive);
    CHECK(a.awake);
    CHECK(a.lair_site == 2);
    CHECK(a.region == 3);
    const wh::MonsterState b = t.monster_at(2, 1300);
    CHECK(b.lair_site == 3);
    CHECK(b.region == 1);
    CHECK(b.awake);
    CHECK(t.monster_at(2, 1399).awake);
    CHECK_FALSE(t.monster_at(2, 1400).awake);
    CHECK(t.monster_at(2, 1400).alive);
    CHECK(t.monster_at(2, 2000).alive);
}

TEST_CASE("P5-02 13: beast_count(r, b, Y): the row with the largest year <= Y, else 0") {
    const wh::Timeline& t = h13().timeline;
    CHECK(t.beast_count(1, 2, 0) == 40);
    CHECK(t.beast_count(1, 2, 1998) == 40);
    CHECK(t.beast_count(1, 2, 1999) == 35);
    CHECK(t.beast_count(1, 2, 2000) == 35);
    CHECK(t.beast_count(2, 2, 948) == 160);
    CHECK(t.beast_count(2, 2, 949) == 150);
    CHECK(t.beast_count(3, 1, 0) == 7);
    CHECK(t.beast_count(4, 1, 1000) == 0);   // no row
    CHECK(t.beast_count(3, 2, 1000) == 0);
}

TEST_CASE("P5-02 13, 8.3: SettlementState::ending is 0 while the settlement stands at Y, else settlement.ending") {
    const wh::Timeline& t = h13().timeline;
    CHECK(t.settlement_state(2, 1299).ending == 0);   // Arnwick standing
    CHECK(t.settlement_state(2, 1300).ending == 2);   // overrun
    CHECK(t.settlement_state(3, 399).ending == 0);
    CHECK(t.settlement_state(3, 400).ending == 3);    // plague
    CHECK(t.settlement_state(5, 450).ending == 4);    // famine
    CHECK(t.settlement_state(6, 600).ending == 0);
    CHECK(t.settlement_state(6, 601).ending == 5);    // left after a raid
    CHECK(t.settlement_state(7, 1500).ending == 1);   // sacked
    CHECK(t.settlement_state(1, 2000).ending == 0);
    CHECK(t.settlement_state(7, 59).ending == 0);     // not founded yet
}

}  // TEST_SUITE
