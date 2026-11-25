// Spec P4-04 10.4 and section 12: invariants 1 to 9 by brute force over generated worlds: 512 seeds 0 to 3 by
// default; in the skipped suite "markers-slow" 512 seeds 4 to 19 and 1024 seeds 0 to 9. Every year 0 ... E is
// visited; the expected values are computed here from the stored rows (census, events, participants, region
// seed cells, pass rows) by the definitions of section 3, not from the index.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "markers_support.hpp"

namespace {

using namespace mktest;
using wi::SettlementMarker;

struct Expect {
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> seed;  // region -> seed cell
    std::map<std::int64_t, std::int64_t> ruined;                         // R(s), first census row of status 3
    std::map<std::int64_t, std::int64_t> ruin_event;                     // 3.3
    std::map<std::int64_t, std::int64_t> faded_year;                     // the year of s's first ruin_faded event
};

Expect expect_of(const db::WorldReader& r, const wh::LoadedHistory& h) {
    Expect x;
    auto regions = db::read_region_rows(r);
    REQUIRE(regions.ok());
    for (const db::RegionRow& g : regions.value()) x.seed[g.id] = {g.seed_x, g.seed_y};
    std::map<std::int64_t, std::int64_t> faded_by;
    for (const wh::HistoryView::Event& e : h.view.events()) {
        if (kind_key(h, e.row.id) == "ruin_faded" && e.row.settlement != 0 && !faded_by.count(e.row.settlement)) {
            faded_by[e.row.settlement] = e.row.id;
            x.faded_year[e.row.settlement] = e.row.year;
        }
    }
    for (const db::SettlementRow& s : h.tables.settlements) {
        std::optional<std::int64_t> first;
        for (const db::SettlementCensusRow& c : h.tables.census) {
            if (c.settlement == s.id && c.status == 3 && (!first || c.year < *first)) first = c.year;
        }
        x.ruined[s.id] = first ? *first : -1;
        std::int64_t ev = s.ruin_event;
        if (ev == 0 && faded_by.count(s.id)) {
            const wh::HistoryView::Event* f = h.view.event(faded_by[s.id]);
            ev = f->causes.empty() ? 0 : *std::min_element(f->causes.begin(), f->causes.end());
        }
        x.ruin_event[s.id] = ev;
    }
    return x;
}

void check_world(const tltest::WorldSpec& spec) {
    CAPTURE(spec.name);
    const std::string path = tltest::world(spec);
    const Indexed o = open_indexed(path);
    const wh::LoadedHistory& h = o.h;
    const wi::MarkerIndex& ix = o.index;
    const Expect x = expect_of(o.reader, h);
    const std::int64_t E = h.tables.end_year;
    const std::vector<db::SettlementRow>& rows = h.tables.settlements;

    for (const db::SettlementRow& s : rows) {
        CHECK(ix.ruined_year(s.id) == x.ruined.at(s.id));
        CHECK(ix.ruin_event(s.id) == x.ruin_event.at(s.id));
    }
    std::set<std::int64_t> present;
    wh::YearState st;
    wi::YearMarkers reuse;
    std::int64_t civ_years = 0;
    for (std::int64_t y = 0; y <= E; ++y) {
        h.timeline.year_state(y, st);
        ix.markers_at(h, st, ALL, reuse);
        const wi::YearMarkers& m = reuse;
        // invariant 8 at a sample of years: the returning form, and the default options
        if (y % 97 == 0 || y == E) {
            CHECK(ix.markers_at(h, y, ALL) == m);
        }
        const wi::YearMarkers nf = ix.markers_at(h, y, wi::MarkerOptions{});
        CHECK(nf.year == y);
        // invariant 1
        std::vector<std::int64_t> want, got, want_nf, got_nf;
        for (const db::SettlementRow& s : rows) {
            if (s.founded_year > y) continue;
            want.push_back(s.id);
            // P5-06 25.1 (e): without the option, no faded row and no old ruin (OLD_RUIN_YEARS or more)
            const std::int64_t stt = h.timeline.settlement_state(s.id, y).status;
            const bool old = stt == 3 && s.ruined_year && y - *s.ruined_year >= wi::OLD_RUIN_YEARS;
            if (stt != 4 && !old) want_nf.push_back(s.id);
        }
        for (const SettlementMarker& s : m.settlements) got.push_back(s.settlement);
        for (const SettlementMarker& s : nf.settlements) got_nf.push_back(s.settlement);
        CHECK(got == want);
        CHECK(got_nf == want_nf);
        for (const std::int64_t p : present) CHECK(std::find(got.begin(), got.end(), p) != got.end());
        present = std::set<std::int64_t>(got.begin(), got.end());
        // fields; invariants 3 and 4
        std::map<std::int64_t, int> capitals;
        const std::set<std::int64_t> alive(st.civilizations.begin(), st.civilizations.end());
        for (const SettlementMarker& s : m.settlements) {
            const wh::SettlementState ss = h.timeline.settlement_state(s.settlement, y);
            const db::SettlementRow& row = rows[static_cast<std::size_t>(s.settlement - 1)];
            if (!(s.owner == ss.owner && s.kind == ss.kind && s.status == ss.status && s.population == ss.population &&
                  s.since == ss.since && s.x == row.x && s.y == row.y && s.founded == row.founded_year)) {
                FAIL_CHECK("settlement " << s.settlement << " at " << y << " differs from its state");
            }
            const std::int64_t R = x.ruined.at(s.settlement);
            const bool standing = s.status == 1 || s.status == 2;
            if (standing) {
                CHECK((R == -1 || y < R));
                if (s.ruined_year != -1 || s.ruin_event != 0) FAIL_CHECK("standing s" << s.settlement << " at " << y);
            } else {
                CHECK(R != -1);
                CHECK(y >= R);
                const auto f = x.faded_year.find(s.settlement);
                const std::int64_t want_status = (f != x.faded_year.end() && y >= f->second) ? 4 : 3;
                if (s.status != want_status) {
                    FAIL_CHECK("s" << s.settlement << " at " << y << ": status " << s.status << ", want " << want_status);
                }
                CHECK(s.ruined_year == R);
                CHECK(s.ruin_event == x.ruin_event.at(s.settlement));
                CHECK_FALSE(s.capital);
            }
            if (s.capital) {
                CHECK(standing);
                CHECK(alive.count(s.owner) == 1U);
                ++capitals[s.owner];
                CHECK(ix.capital(h, s.owner, y) == s.settlement);
            }
        }
        for (const std::int64_t c : st.civilizations) {
            ++civ_years;
            if (capitals[c] != 1) FAIL_CHECK("civilization " << c << " at " << y << ": " << capitals[c] << " capitals");
        }
        // invariant 6
        std::vector<std::int64_t> placed, in_places;
        for (const std::int64_t e : st.events) {
            const wh::HistoryView::Event* ev = h.view.event(e);
            if (ev->row.settlement != 0 || ev->row.region != 0) placed.push_back(e);
        }
        std::int64_t last_first = 0;
        for (const wi::PlaceMarker& p : m.places) {
            REQUIRE_FALSE(p.events.empty());
            CHECK(std::is_sorted(p.events.begin(), p.events.end()));
            CHECK(p.events.front() > last_first);
            last_first = p.events.front();
            wi::EventSymbol top = wi::EventSymbol::plain;
            for (const std::int64_t e : p.events) {
                in_places.push_back(e);
                const wh::HistoryView::Event* ev = h.view.event(e);
                std::pair<std::int64_t, std::int64_t> cell;
                if (ev->row.settlement != 0) {
                    const db::SettlementRow& sr = rows[static_cast<std::size_t>(ev->row.settlement - 1)];
                    cell = {sr.x, sr.y};
                } else {
                    cell = x.seed.at(ev->row.region);
                }
                CHECK(cell == std::make_pair(p.x, p.y));
                top = std::max(top, wi::event_symbol(kind_key(h, e)));
            }
            CHECK(p.symbol == top);
        }
        for (std::size_t i = 0; i < m.places.size(); ++i) {
            for (std::size_t j = i + 1; j < m.places.size(); ++j) {
                CHECK_FALSE((m.places[i].x == m.places[j].x && m.places[i].y == m.places[j].y));
            }
        }
        std::sort(in_places.begin(), in_places.end());
        CHECK(in_places == placed);
        CHECK(nf.places == m.places);
        // invariant 7
        CHECK(m.passes.size() == ix.passes().size());
        if (y == 0 || y == E) {
            for (std::size_t i = 0; i < m.passes.size() && i < ix.passes().size(); ++i) {
                const db::PassRow& p = ix.passes()[i];
                CHECK(m.passes[i] == wi::PassMarker{p.id, p.col_x, p.col_y, p.kind});
            }
        }
    }
    MESSAGE(spec.name << ": " << civ_years << " civilization-years");
    // invariant 2, 4 at E and 5
    const wi::YearMarkers end = ix.markers_at(h, E, ALL);
    REQUIRE(end.settlements.size() == rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const SettlementMarker& s = end.settlements[i];
        CHECK(s.owner == rows[i].owner_civ);
        CHECK(s.kind == rows[i].kind);
        CHECK(s.status == rows[i].status);
        CHECK(s.population == rows[i].population);
        if (rows[i].status == 3) {
            REQUIRE(rows[i].ruined_year.has_value());
            CHECK(s.ruined_year == *rows[i].ruined_year);
        }
        if (rows[i].status == 4) {
            REQUIRE(s.ruin_event != 0);
            CHECK(h.view.event(s.ruin_event)->row.year == s.ruined_year);
        }
    }
    for (const db::CivilizationRow& c : h.tables.civilizations) {
        if (!c.ended_year) CHECK(ix.capital(h, c.id, E) == c.capital);
    }
    // ids out of range of the passes span are ascending
    for (std::size_t i = 1; i < ix.passes().size(); ++i) CHECK(ix.passes()[i - 1].id < ix.passes()[i].id);
    // invariant 9 on the world: scale 2 equals scale 1 scaled; every changed pixel is a marker colour
    const tltest::WorldLayers layers = tltest::read_layers(o.reader);
    const wi::TerritoryBase base = tltest::territory_base(layers);
    const std::vector<wi::Rgb> colours = wi::civilization_colours(h.tables);
    const std::int64_t mid = E / 2;
    const std::vector<std::uint8_t> plain = wi::render_territory(base, h.timeline, colours, mid, 1);
    const std::vector<std::uint8_t> one = overlay(base, h, ix, mid, 1);
    std::set<std::tuple<int, int, int>> allowed = {{0, 0, 0}, {240, 240, 240}, {150, 150, 150}, {128, 128, 128}};
    for (const wi::Rgb& c : colours) allowed.insert({c.r, c.g, c.b});
    for (const wi::EventSymbol sy : {wi::EventSymbol::plain, wi::EventSymbol::plague, wi::EventSymbol::battle,
                                     wi::EventSymbol::siege, wi::EventSymbol::sack}) {
        const wi::Rgb c = wi::event_symbol_colour(sy);
        allowed.insert({c.r, c.g, c.b});
    }
    std::size_t changed = 0;
    for (std::size_t i = 0; i + 2 < one.size(); i += 3) {
        if (one[i] == plain[i] && one[i + 1] == plain[i + 1] && one[i + 2] == plain[i + 2]) continue;
        ++changed;
        if (!allowed.count({one[i], one[i + 1], one[i + 2]})) {
            FAIL_CHECK("pixel " << i / 3 << " changed to a colour no marker has");
            break;
        }
    }
    CHECK(changed > 0U);
    const std::vector<std::uint8_t> two = overlay(base, h, ix, mid, 2);
    const std::uint32_t w = base.width(), hh = base.height();
    bool same = two.size() == one.size() * 4;
    for (std::uint32_t yy = 0; same && yy < hh * 2; ++yy) {
        for (std::uint32_t xx = 0; same && xx < w * 2; ++xx) {
            const std::size_t a = 3 * (std::size_t{yy} * w * 2 + xx), b = 3 * (std::size_t{yy / 2} * w + xx / 2);
            same = two[a] == one[b] && two[a + 1] == one[b + 1] && two[a + 2] == one[b + 2];
        }
    }
    CHECK(same);
}

}  // namespace

TEST_SUITE("markers") {

TEST_CASE("P4-04 section 12: invariants 1 to 9 on 512 worlds, seeds 0 to 3") {
    for (std::uint64_t seed = 0; seed < 4; ++seed) check_world(tltest::property_world(seed, 512));
}

}  // TEST_SUITE

TEST_SUITE("markers-slow" * doctest::skip()) {

TEST_CASE("P4-04 section 12: invariants 1 to 9 on 512 worlds, seeds 4 to 19") {
    for (std::uint64_t seed = 4; seed < 20; ++seed) check_world(tltest::property_world(seed, 512));
}

TEST_CASE("P4-04 section 12: invariants 1 to 9 on 1024 worlds, seeds 0 to 9") {
    for (std::uint64_t seed = 0; seed < 10; ++seed) check_world(tltest::property_world(seed, 1024));
}

}  // TEST_SUITE
