// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 5: the people ledger of 6.4 and 7.2. A
// recording PeopleLedger is set on peoples in the configuration without RelationsSubsystem (so it sees exactly
// the calls of peoples and conflict): the ruin procedure (abandonment kind 31 and 16, sack, overrun), the refugees
// of a capture and the flight of a last refuge each call as 7.2 says; the ledger only records (the history is the
// same with it, without it and with the subsystem's ledger absent). Hosts and arrivals are checked on the rows of
// relations-small (invariant 5 in p5_03_relations_checks.hpp: hops, war, kin standing, the displacing cause).
//
// Written from the spec before the implementation. Library names: PeopleLedger, PeoplesSubsystem::set_ledger
// (6.4.1) and the adapter's.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

using p503api::LedgerCall;
using p503api::RecordingLedger;

struct Recorded {
    p503check::World w;
    std::vector<LedgerCall> calls;
};

Recorded recorded(std::uint64_t seed, const wd::ContentSet& c) {
    RecordingLedger rec;
    p503api::RelConfig::Options o;
    o.relations = false;
    o.other_ledger = &rec;
    Recorded out{p503run::config_fixture(seed, c, o), {}};
    out.calls = rec.calls;
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 6.4.1, 7.2: the ledger only records: the history is the same with a recording ledger and with none") {
    for (const std::uint64_t seed : {0U, 1U, 2U}) {
        CAPTURE(seed);
        const Recorded a = recorded(seed, p503api::relations_small_content());
        p503api::RelConfig::Options o;
        o.relations = false;
        const p503check::World b = p503run::config_fixture(seed, p503api::relations_small_content(), o);
        CHECK(a.w.rows.t == b.rows.t);
        CHECK_FALSE(a.calls.empty());
    }
}

TEST_CASE("P5-03 7.2: the calls of the ruin procedure, the capture's refugees and the flight, against the rows") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const Recorded r = recorded(seed, p503api::relations_small_content());
        const p503check::Index x(r.w);
        static const std::map<std::string, bool> causes = {{"settlement_abandoned", true},
                                                           {"settlement_abandoned_to", true}, {"sacked", true},
                                                           {"settlement_overrun", true},
                                                           {"settlement_captured", true}};
        std::map<std::int64_t, std::int64_t> displaced_by;   // cause -> people
        for (const LedgerCall& c : r.calls) {
            CHECK(c.people >= 1);
            CHECK(x.stl.count(c.from) == 1U);
            if (c.moved) {
                CHECK(c.to != c.from);
                CHECK(x.stl.count(c.to) == 1U);
            } else {
                CHECK(c.race == 0);   // peoples and conflict displace every race of `from`
                CHECK(causes.count(x.kind_of(c.cause)) == 1U);
                if (x.ev.count(c.cause) != 0U) CHECK(p503check::at(*x.ev.at(c.cause), 6) == c.from);
                displaced_by[c.cause] += c.people;
            }
        }
        auto moved = [&](std::int64_t from, std::int64_t to, std::int64_t n) {
            for (const LedgerCall& c : r.calls) {
                if (c.moved && c.from == from && c.to == to && c.people == n) return true;
            }
            return false;
        };
        // kind 31: moved(s, t, settled) when settled >= 1; the displaced at most the lost (the dead of the road
        // are not displaced, 11.1)
        for (const std::int64_t e : x.events_of("settlement_abandoned_to")) {
            const std::int64_t s = x.part(e, 1), t = x.part(e, 3), settled = x.val(e, 0), lost = x.val(e, 1);
            CAPTURE(e);
            if (settled >= 1) CHECK(moved(s, t, settled));
            CHECK(displaced_by[e] <= lost);
        }
        // kind 16: no destination; its survivors displaced (11.1)
        for (const std::int64_t e : x.events_of("settlement_abandoned")) {
            CAPTURE(e);
            const std::int64_t survivors = x.val(e, 0);
            CHECK(displaced_by[e] == survivors);
        }
        // the flight of a last refuge (P3-06 20.1): moved(t, h, n) for the refuge founded with n people
        for (const std::int64_t e : x.events_of("refugees_settled")) {
            CAPTURE(e);
            CHECK(moved(x.part(e, 3), x.part(e, 1), x.val(e, 0)));
        }
        // a capture: the displaced at most those who fled
        for (const std::int64_t e : x.events_of("settlement_captured")) {
            CAPTURE(e);
            CHECK(displaced_by[e] <= x.val(e, 1));
        }
    }
}

TEST_CASE("P5-03 11.1: abandon_reach_hops 0 (relations-small): every destination lies in the abandoned place's region") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::relations_small_content());
        const p503check::Index x(w);
        for (const std::int64_t e : x.events_of("settlement_abandoned_to")) {
            const std::int64_t s = x.part(e, 1), t = x.part(e, 3);
            CHECK(p503check::at(*x.stl.at(s), 4) == p503check::at(*x.stl.at(t), 4));
        }
    }
}

TEST_CASE("P5-03 6.4.3: arrivals on relations-small: hosts within migrate_hops, not at war, kin standing, causes") {
    std::int64_t arrivals = 0;
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::relations_small_content());
        CHECK(p503check::violations(w) == std::vector<std::string>{});
        arrivals += static_cast<std::int64_t>(p503check::Index(w).events_of("refugees_arrived").size());
    }
    CHECK(arrivals > 0);
    // welcome_min 2001 (never): no arrival at all; migrate_hops 0: hosts in the origin's region only
    const wd::ContentSet closed =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"welcome_min", p503api::V(2001)}});
    const wd::ContentSet near =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"migrate_hops", p503api::V(0)}});
    for (const std::uint64_t seed : {0U, 1U}) {
        const p503check::World a = p503run::stage_fixture(seed, closed);
        CHECK(p503check::Index(a).events_of("refugees_arrived").empty());   // D(g, g) is a camp value < 2001 too
        const p503check::World b = p503run::stage_fixture(seed, near);
        const p503check::Index xb(b);
        for (const std::int64_t e : xb.events_of("refugees_arrived")) {
            CHECK(p503check::at(*xb.stl.at(xb.part(e, 1)), 4) == p503check::at(*xb.stl.at(xb.part(e, 3)), 4));
        }
    }
}

}  // TEST_SUITE
