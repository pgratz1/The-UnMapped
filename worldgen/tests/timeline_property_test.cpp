// Spec P4-01 13.5 item 8: the invariants of section 8 by brute force against the stored tables (linear
// scans per year, for every year 0 to E), the digest's counts and [stats], on generated worlds: 512 seeds 0
// to 19 (default suite) and 1024 seeds 0 to 9 (suite "timeline-slow"); and on every fixture.
//
// Written from the spec only. Every expected value below is computed from the HistoryTables and
// HistoryView rows with the definitions of section 4, never from the timeline's own index.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "timeline_support.hpp"

namespace {

using namespace tltest;
using V = std::vector<std::int64_t>;

bool strictly_ascending(std::span<const std::int64_t> v) {
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (v[i - 1] >= v[i]) return false;
    }
    return true;
}

// Section 4's holder of region r at y, by a linear scan of r's rows.
wh::RegionHolding brute_holder(const std::vector<db::RegionControlRow>& rows, std::int64_t r, std::int64_t y,
                               int* matches) {
    wh::RegionHolding h{r, 0, 0, 0};
    *matches = 0;
    std::int64_t free_since = 0;
    for (const db::RegionControlRow& c : rows) {
        if (c.from_year <= y && (!c.to_year || y < *c.to_year)) {
            ++*matches;
            h.civ = c.civ;
            h.since = c.from_year;
            h.event = c.event;
        }
        if (c.to_year && *c.to_year <= y) free_since = std::max(free_since, *c.to_year);
    }
    if (*matches == 0) h.since = free_since;
    return h;
}

// Section 4's state of settlement s at y, by a linear scan of s's census rows.
wh::SettlementState brute_settlement(const std::vector<db::SettlementCensusRow>& rows, const db::SettlementRow& s,
                                     std::int64_t y) {
    wh::SettlementState out;
    out.settlement = s.id;
    if (s.founded_year > y) return out;
    out.exists = true;
    const db::SettlementCensusRow* best = nullptr;
    for (const db::SettlementCensusRow& c : rows) {
        if (c.year <= y && (best == nullptr || c.year > best->year)) best = &c;
    }
    REQUIRE(best != nullptr);
    out.owner = best->owner_civ;
    out.kind = best->kind;
    out.status = best->status;
    out.population = best->population;
    out.since = best->year;
    return out;
}

bool same(const wh::RegionHolding& a, const wh::RegionHolding& b) {
    return a.region == b.region && a.civ == b.civ && a.since == b.since && a.event == b.event;
}
bool same(const wh::SettlementState& a, const wh::SettlementState& b) {
    return a.settlement == b.settlement && a.exists == b.exists && a.owner == b.owner && a.kind == b.kind &&
           a.status == b.status && a.population == b.population && a.since == b.since;
}

std::int64_t stat(const wh::HistoryDigest& d, const std::string& key) {
    for (const auto& [k, v] : d.stats) {
        if (k == key) return v;
    }
    FAIL("no [stats] key " << key);
    return 0;
}

void check_invariants(const std::string& name, const std::string& path, bool territory) {
    CAPTURE(name);
    const db::WorldReader r = open_reader(path);
    const wh::LoadedHistory h = load(r);
    const wh::HistoryTables& tb = h.tables;
    const wh::Timeline& t = h.timeline;
    const std::int64_t E = tb.end_year;
    REQUIRE(t.end_year() == E);
    const auto events = h.view.events();
    std::map<std::int64_t, std::int64_t> in_events, in_starting;  // event id -> years listed
    const std::vector<std::int64_t> change = V(t.holder_change_years().begin(), t.holder_change_years().end());
    CHECK(strictly_ascending(change));
    std::vector<std::int64_t> prev_holders(tb.regions.size(), 0);
    std::vector<bool> existed(tb.settlements.size(), false);
    std::string forward;  // D(0) ... D(E) from the returning overload, ascending years
    // the rows of each region and settlement (grouped once; the scans below are linear in them)
    std::map<std::int64_t, std::vector<db::RegionControlRow>> control;
    for (const db::RegionControlRow& c : tb.region_control) control[c.region].push_back(c);
    std::map<std::int64_t, std::vector<db::SettlementCensusRow>> census;
    for (const db::SettlementCensusRow& c : tb.census) census[c.settlement].push_back(c);
    int failures = 0;
    for (std::int64_t y = 0; y <= E && failures < 20; ++y) {
        const wh::YearState st = t.year_state(y);
        forward += year_dump(st);
        CHECK(st.year == y);
        // invariant 1: holders by linear scan; at most one row matches
        REQUIRE(st.regions.size() == tb.regions.size());
        std::vector<std::int64_t> holders;
        for (std::size_t i = 0; i < tb.regions.size(); ++i) {
            int matches = 0;
            const wh::RegionHolding want = brute_holder(control[tb.regions[i].id], tb.regions[i].id, y, &matches);
            if (matches > 1 || !same(st.regions[i], want) || !same(t.region_holder(tb.regions[i].id, y), want)) {
                ++failures;
                FAIL_CHECK("invariant 1: region " << tb.regions[i].id << " at " << y);
            }
            holders.push_back(want.civ);
        }
        // invariant 2: the change years
        const bool changed = holders != prev_holders;
        const bool listed = std::binary_search(change.begin(), change.end(), y);
        if (changed != listed) {
            ++failures;
            FAIL_CHECK("invariant 2 at " << y << ": changed " << changed << ", listed " << listed);
        }
        prev_holders = holders;
        // invariant 3: settlements, existence monotone
        REQUIRE(st.settlements.size() == tb.settlements.size());
        for (std::size_t i = 0; i < tb.settlements.size(); ++i) {
            const wh::SettlementState want = brute_settlement(census[tb.settlements[i].id], tb.settlements[i], y);
            if (!same(st.settlements[i], want) || !same(t.settlement_state(tb.settlements[i].id, y), want)) {
                ++failures;
                FAIL_CHECK("settlement " << tb.settlements[i].id << " at " << y);
            }
            if (existed[i] && !want.exists) FAIL_CHECK("invariant 3: settlement stops existing");
            existed[i] = want.exists;
        }
        // relations in force and invariant 7's until = since rule
        std::vector<std::size_t> rel;
        for (std::size_t i = 0; i < tb.relations.size(); ++i) {
            const db::RelationRow& x = tb.relations[i];
            if (x.since <= y && (!x.until || y < *x.until)) rel.push_back(i);
            if (x.until && *x.until == x.since &&
                std::find(st.relations.begin(), st.relations.end(), i) != st.relations.end()) {
                FAIL_CHECK("invariant 7: relation " << i << " with until = since in force at " << y);
            }
        }
        if (st.relations != rel) {
            ++failures;
            FAIL_CHECK("relations at " << y);
        }
        // civilizations alive
        V alive;
        for (const db::CivilizationRow& c : tb.civilizations) {
            if (c.founded_year <= y && (!c.ended_year || y < *c.ended_year)) alive.push_back(c.id);
        }
        if (st.civilizations != alive) {
            ++failures;
            FAIL_CHECK("civilizations at " << y);
        }
        // phase and age
        std::int64_t phase = 0, age = y == E ? tb.ages.back().id : 0;
        for (const db::PhaseRow& p : tb.phases) {
            if (y < E && p.start_year <= y && y <= p.end_year) phase = p.id;
        }
        for (const db::AgeRow& a : tb.ages) {
            if (y < E && a.start_year <= y && y <= a.end_year) age = a.id;
        }
        if (st.phase != phase || st.age != age) {
            ++failures;
            FAIL_CHECK("phase or age at " << y);
        }
        // invariant 5: events of Y and starting in Y, ascending; brute force over the log
        V ev, starting;
        for (const wh::HistoryView::Event& e : events) {
            if (e.row.year <= y && y <= e.row.end_year && y < E) ev.push_back(e.row.id);
            if (e.row.year == y && y < E) starting.push_back(e.row.id);
        }
        if (st.events != ev || st.events_starting != starting) {
            ++failures;
            FAIL_CHECK("events of " << y);
        }
        for (const std::int64_t e : st.events) ++in_events[e];
        for (const std::int64_t e : st.events_starting) ++in_starting[e];
        // previous / next event year by start years
        std::optional<std::int64_t> prev, next;
        for (const wh::HistoryView::Event& e : events) {
            if (e.row.year < y) prev = std::max(prev.value_or(e.row.year), e.row.year);
            if (e.row.year > y && !next) next = e.row.year;
        }
        if (t.previous_event_year(y) != prev || t.next_event_year(y) != next) {
            ++failures;
            FAIL_CHECK("previous or next event year at " << y);
        }
    }
    REQUIRE(failures == 0);
    for (const wh::HistoryView::Event& e : events) {
        CHECK(in_events[e.row.id] == e.row.end_year - e.row.year + 1);
        CHECK(in_starting[e.row.id] == 1);
    }
    // invariant 2 at E: the open rows; their count is [stats] regions_held
    const wh::HistoryDigest d = wh::digest_history(h.view, tb);
    std::int64_t open = 0;
    for (const db::RegionControlRow& c : tb.region_control) open += c.to_year ? 0 : 1;
    std::int64_t held_at_e = 0;
    for (const wh::RegionHolding& x : t.year_state(E).regions) held_at_e += x.civ != 0 ? 1 : 0;
    CHECK(held_at_e == open);
    CHECK(stat(d, "regions_held") == open);
    // invariant 3 at E: the settlement rows' end state
    for (const db::SettlementRow& s : tb.settlements) {
        const wh::SettlementState x = t.settlement_state(s.id, E);
        CHECK(x.owner == s.owner_civ);
        CHECK(x.kind == s.kind);
        CHECK(x.status == s.status);
        CHECK(x.population == s.population);
    }
    // invariant 4: E equals E - 1 in regions, settlements, relations and civilizations
    {
        const wh::YearState a = t.year_state(E), b = t.year_state(E - 1);
        for (std::size_t i = 0; i < a.regions.size(); ++i) CHECK(same(a.regions[i], b.regions[i]));
        for (std::size_t i = 0; i < a.settlements.size(); ++i) CHECK(same(a.settlements[i], b.settlements[i]));
        CHECK(a.relations == b.relations);
        CHECK(a.civilizations == b.civilizations);
        CHECK(a.phase == 0);
        CHECK(a.age == tb.ages.back().id);
        CHECK(a.events.empty());
        CHECK(a.events_starting.empty());
        // invariant 7 at E
        std::vector<std::size_t> open_rel;
        for (std::size_t i = 0; i < tb.relations.size(); ++i) {
            if (!tb.relations[i].until) open_rel.push_back(i);
        }
        CHECK(a.relations == open_rel);
        V open_civ;
        for (const db::CivilizationRow& c : tb.civilizations) {
            if (!c.ended_year) open_civ.push_back(c.id);
        }
        CHECK(a.civilizations == open_civ);
    }
    // invariant 6: entity lists by brute force, strictly ascending, counts as the digest's; effects
    for (const db::CivilizationRow& c : tb.civilizations) {
        V want;
        for (const wh::HistoryView::Event& e : events) {
            if (std::any_of(e.participants.begin(), e.participants.end(),
                            [&](const db::EventParticipantRow& p) { return p.entity_type == 3 && p.entity_id == c.id; })) {
                want.push_back(e.row.id);
            }
        }
        const auto got = t.events_of_civilization(c.id);
        CHECK(V(got.begin(), got.end()) == want);
        CHECK(static_cast<std::int64_t>(got.size()) == d.civilizations[static_cast<std::size_t>(c.id - 1)].events);
    }
    for (const wh::RegionInfo& rg : tb.regions) {
        V want;
        for (const wh::HistoryView::Event& e : events) {
            if (e.row.region == rg.id ||
                std::any_of(e.participants.begin(), e.participants.end(),
                            [&](const db::EventParticipantRow& p) { return p.entity_type == 5 && p.entity_id == rg.id; })) {
                want.push_back(e.row.id);
            }
        }
        const auto got = t.events_of_region(rg.id);
        CHECK(strictly_ascending(got));
        CHECK(V(got.begin(), got.end()) == want);
    }
    for (const db::SettlementRow& s : tb.settlements) {
        V want;
        for (const wh::HistoryView::Event& e : events) {
            if (e.row.settlement == s.id ||
                std::any_of(e.participants.begin(), e.participants.end(),
                            [&](const db::EventParticipantRow& p) { return p.entity_type == 4 && p.entity_id == s.id; })) {
                want.push_back(e.row.id);
            }
        }
        const auto got = t.events_of_settlement(s.id);
        CHECK(strictly_ascending(got));
        CHECK(V(got.begin(), got.end()) == want);
    }
    {
        std::map<std::int64_t, V> want;
        for (const wh::HistoryView::Event& f : events) {
            for (const std::int64_t c : f.causes) want[c].push_back(f.row.id);
        }
        for (const wh::HistoryView::Event& e : events) {
            const auto got = t.effects(e.row.id);
            CHECK(strictly_ascending(got));
            CHECK(V(got.begin(), got.end()) == want[e.row.id]);
        }
    }
    // invariant 8: a colour is the shade of (race key, ordinal); ordinals below 4 of one race distinct
    {
        const std::vector<wi::Rgb> colours = wi::civilization_colours(tb);
        REQUIRE(colours.size() == tb.civilizations.size());
        std::map<std::int64_t, std::int64_t> ordinal;
        std::map<std::int64_t, std::vector<wi::Rgb>> by_race;
        for (std::size_t i = 0; i < tb.civilizations.size(); ++i) {
            const db::CivilizationRow& c = tb.civilizations[i];
            const auto race = std::find_if(tb.races.begin(), tb.races.end(),
                                           [&](const db::RaceRow& x) { return x.id == c.race; });
            REQUIRE(race != tb.races.end());
            const std::int64_t k = ordinal[c.race]++;
            CHECK(colours[i] == wi::civilization_shade(wi::race_base_colour(race->key), k));
            if (k < 4) {
                for (const wi::Rgb& o : by_race[c.race]) CHECK_FALSE(o == colours[i]);
                by_race[c.race].push_back(colours[i]);
            }
        }
    }
    // invariant 10: the order of calls does not matter; the reusing overload gives the same content
    {
        std::vector<std::string> by_year(static_cast<std::size_t>(E + 1));
        wh::YearState reused;
        for (std::int64_t y = E; y >= 0; --y) {
            t.year_state(y, reused);
            by_year[static_cast<std::size_t>(y)] = year_dump(reused);
        }
        std::string backward;
        for (const std::string& s : by_year) backward += s;
        CHECK(backward == forward);
        // a fixed scrambled order, reusing one buffer
        const std::int64_t n = E + 1;
        for (std::int64_t i = 0; i < n; i += 37) {
            const std::int64_t y = (i * 7919) % n;
            t.year_state(y, reused);
            CHECK(year_dump(reused) == by_year[static_cast<std::size_t>(y)]);
        }
    }
    // invariant 9 on the world's own layers, when it has them
    if (territory) {
        const WorldLayers layers = read_layers(r);
        const wi::TerritoryBase base = territory_base(layers);
        const std::vector<std::uint8_t> b1 = regions_composite(layers, 1);
        CHECK(wi::render_territory(base, {}, 1) == b1);
        const std::vector<wi::Rgb> colours = wi::civilization_colours(tb);
        const auto* elev = layers.elevation.get_if<std::int16_t>();
        const auto* reg = layers.region.get_if<std::uint16_t>();
        REQUIRE(elev != nullptr);
        REQUIRE(reg != nullptr);
        const std::uint32_t w = layers.elevation.width, hh = layers.elevation.height;
        for (const std::int64_t y : {E / 2, E}) {
            CAPTURE(y);
            std::map<std::int64_t, std::int64_t> holder;
            for (const wh::RegionHolding& x : t.year_state(y).regions) holder[x.region] = x.civ;
            const std::vector<std::uint8_t> img = wi::render_territory(base, wi::territory_fill(t, colours, y), 1);
            REQUIRE(img.size() == b1.size());
            std::int64_t bad = 0;
            for (std::uint32_t cy = 0; cy < hh; ++cy) {
                for (std::uint32_t cx = 0; cx < w; ++cx) {
                    const std::size_t i = std::size_t{cy} * w + cx;
                    const wi::Rgb got{img[3 * i], img[3 * i + 1], img[3 * i + 2]};
                    const wi::Rgb b{b1[3 * i], b1[3 * i + 1], b1[3 * i + 2]};
                    const auto it = holder.find((*reg)[i]);
                    const std::int64_t civ = it == holder.end() ? 0 : it->second;
                    if (got == b) continue;
                    if (civ == 0) {
                        ++bad;
                        continue;
                    }
                    const wi::Rgb c = wi::shade_colour(colours[static_cast<std::size_t>(civ - 1)],
                                                       wi::hillshade_factor(*elev, w, hh, cx, cy));
                    if (!(got == c)) ++bad;
                }
            }
            CHECK(bad == 0);
        }
    }
}

}  // namespace

TEST_SUITE("timeline") {

TEST_CASE("P4-01 13.5 item 8: the invariants of section 8 on every fixture") {
    for (const std::string& name : hrtest::fixture_names()) check_invariants(name, hrtest::fixture_world(name), false);
    check_invariants(T1, t1_world(), true);
}

TEST_CASE("P4-01 13.5 item 8: the invariants of section 8 on 512 worlds, seeds 0 to 19") {
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const WorldSpec c = property_world(seed, 512);
        check_invariants(c.name, world(c), true);
    }
}

}  // TEST_SUITE

TEST_SUITE("timeline-slow" * doctest::skip()) {

TEST_CASE("P4-01 13.5 item 8 (slow): the invariants of section 8 on 1024 worlds, seeds 0 to 9") {
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const WorldSpec c = property_world(seed, 1024);
        check_invariants(c.name, world(c), true);
    }
}

}  // TEST_SUITE
