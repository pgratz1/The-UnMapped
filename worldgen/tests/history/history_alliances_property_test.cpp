// Spec P5-04 (docs/specs/p5-04-alliances-trade.md r2) 18.2 and 18.3: the invariants of 13 (p5_04_alliances_checks.hpp)
// on the fixture alliances-small (18.3: the conflict stress data with the alliance edits, on the stress geographies),
// on the shipped data's generated worlds (seeds 0 to 9 at 512; seeds 0 to 19 at 1024 in the slow suite), AL9's hard
// bound on every one of them, I0 (7.14) on the stress fixtures, the elf-faerie origin rule (AL4) and determinism (a
// run twice, a world at one and four threads). Every kind of 63 to 78 must happen on alliances-small (18.3).
//
// The rules' internals (the cache, the reasons, the draws) are private: 18.1 items 2 to 13 are checked here through
// their visible effects on rows, and by hand on the formulas in the reference (p5_04_alliances_reference.py units).
// Written from the spec before the implementation.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_04_alliances_checks.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;
using p503api::V;

// 18.3: alliances-small (the conflict stress data with these edits).
const wd::ContentSet& alliances_small() {
    static const wd::ContentSet c = p503api::with_history(
        p503api::with_history(conflict_test::stress_content(), "alliances",
                              {{"form_foe_permille", V(1000)}, {"form_fear_permille", V(1000)},
                               {"form_trade_permille", V(1000)}, {"form_friend_permille", V(1000)},
                               {"form_max_permille", V(1000)}, {"defend_permille", V(1000)},
                               {"refusals_to_break", V(1)}, {"term_years", V(10)}, {"trade_permille", V(1000)},
                               {"spoils_grant_permille", V(500)}, {"spoils_grant_min", V(300)},
                               {"spoils_grant_max", V(700)}, {"withheld_to_break", V(1)}, {"mine_permille", V(1000)},
                               {"mine_settlement_min", V(0)}}),
        "conflict", {{"allied_strain_permille", V(1000)}});
    return c;
}

// 4.2: the zero tuning of I0 (every race's origin_allies empty).
wd::ContentSet without_origin(const wd::ContentSet& base) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (wd::Definition& d : k.definitions) {
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                r.add(d.record.name_at(i), d.record.name_at(i) == "origin_allies" ? wd::Value::array({})
                                                                                    : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}
const wd::ContentSet& zero() {
    // P5-06 (integration): with P5-06's zero tuning, since the configuration without alliances builds no figures
    static const wd::ContentSet c = p503api::figures_zero_of(without_origin(p503api::with_history(
        conflict_test::shipped(), "alliances", {{"ally_hops", V(-1)}, {"trade_hops", V(-1)}, {"mine_permille", V(0)}})));
    return c;
}

// 18.3 with the reference's reading A11: the stress cases with every race present (so that dwarves mine and the
// elves and faeries ally by origin), on stress seeds of the conflict golden file (together every kind of 63 to
// 78 at the calibrated values; seed 5 holds the spurnings).
const std::vector<std::uint64_t>& fixture_seeds() {
    static const std::vector<std::uint64_t> v = {3U, 5U, 6U, 7U};
    return v;
}

eng::HistoryInputs small_inputs(std::uint64_t seed, const wd::ContentSet& content) {
    const conflict_test::CCase& c = conflict_test::conflict_case(("stress-" + std::to_string(seed)).c_str());
    auto st = conflict_test::settings_of(c);
    st.races = chrono_test::races_of_text("all");
    return conflict_test::inputs_of(conflict_test::make_synthetic(seed), seed, st, content);
}

std::string line_of(const std::map<std::string, std::int64_t>& m) {
    std::string out;
    for (const auto& [k, v] : m) out += (out.empty() ? "" : ", ") + k + " " + std::to_string(v);
    return out;
}

void check_world_file(std::uint64_t seed, std::int64_t size) {
    CAPTURE(seed);
    CAPTURE(size);
    const std::string path = p503run::world_path(seed, size);
    const db::WorldReader r = chrono_test::open_world(path);
    const p503check::World w = p503check::world_of_file(r, conflict_test::shipped());
    const p504check::Geo g = p504check::geo_of_file(r);
    CHECK(p504check::violations(w, g, p504check::data_of(conflict_test::shipped())) == std::vector<std::string>{});
    CHECK(p504check::cause_violations(w) == std::vector<std::string>{});
    MESSAGE("seed " << seed << " at " << size << ": " << line_of(p504check::kind_counts(w)));
}

// AL4: every world with an elven and a faerie realm alive together at some year has an origin alliance (by rule;
// 6.4's allies_max and bloc test may refuse one, which this reports instead of failing).
void check_origin(const p503check::World& w) {
    const p503check::Index x(w);
    const p504check::Rows rows(w);
    std::vector<std::pair<std::int64_t, std::int64_t>> both;
    for (const auto& [a, ra] : x.civ) {
        for (const auto& [b, rb] : x.civ) {
            if (a >= b) continue;
            const std::string ka = x.race_key.at(p503check::at(*ra, 2)), kb = x.race_key.at(p503check::at(*rb, 2));
            if (!((ka == "elf" && kb == "faerie") || (ka == "faerie" && kb == "elf"))) continue;
            const std::int64_t from = std::max(p503check::at(*ra, 6), p503check::at(*rb, 6));
            const std::int64_t to = std::min(p504check::is_null(*ra, 8) ? w.end_year : p503check::at(*ra, 8),
                                             p504check::is_null(*rb, 8) ? w.end_year : p503check::at(*rb, 8));
            if (from < to) both.emplace_back(a, b);
        }
    }
    if (both.empty()) return;
    bool any = false;
    for (const std::int64_t e : rows.of("alliance_formed")) any = any || rows.value(e, "basis") == 0;
    CHECK_MESSAGE(any, "elves and faeries alive together and no origin alliance");
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04 18.3: alliances-small: the invariants of 13 hold and every kind of 63 to 78 happens") {
    std::set<std::string> seen;
    for (const std::uint64_t seed : fixture_seeds()) {
        CAPTURE(seed);
        const eng::HistoryInputs in = small_inputs(seed, alliances_small());
        const p503check::World w = p503check::world_of_run(p503api::history_stage(alliances_small(), in), in,
                                                           alliances_small());
        CHECK(p504check::violations(w, p504check::geo_of_inputs(in), p504check::data_of(alliances_small())) ==
              std::vector<std::string>{});
        CHECK(p504check::cause_violations(w) == std::vector<std::string>{});
        const auto counts = p504check::kind_counts(w);
        for (const auto& [k, n] : counts) {
            if (n > 0) seen.insert(k);
        }
        MESSAGE("alliances-small " << seed << ": " << line_of(counts));
    }
    CHECK(seen == std::set<std::string>{"alliance_formed", "alliance_renewed", "alliance_lapsed", "alliance_broken",
                                        "alliance_betrayed", "ally_joined_war", "ally_refused", "ally_withdrew",
                                        "allied_battle", "trade_opened", "trade_broken", "spoils_shared",
                                        "spoils_withheld", "alliance_spurned", "mine_opened", "arms_sold"});
}

TEST_CASE("P5-04 7.14 (I0): with the zero tuning the history equals that of the configuration without alliances") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const eng::HistoryInputs in = p503run::stress_inputs(seed, zero());
        const p503api::RTables a = p503api::history_stage(zero(), in);
        // P5-03's configuration (peoples, conflict, renaming, monsters, relations): no AlliancesSubsystem, no
        // AllianceView set, no site seam
        const p503api::RelConfig cfg(zero(), {});
        const p503api::RTables b = cfg.run(zero(), in);
        for (const char* t : {"event_kind", "event_kind_role", "event_kind_value", "phase", "event", "event_participant",
                              "event_cause", "event_value", "civilization", "settlement", "settlement_census",
                              "region_control", "relation", "region_name", "beast", "monster_kind", "beast_population",
                              "monster", "site"}) {
            CAPTURE(std::string(t));
            CHECK(a.t.of(t) == b.t.of(t));
        }
        CHECK(a.race_relation == b.race_relation);
        CHECK(a.settlement_race == b.settlement_race);
        const p503check::World w = p503check::world_of_run(a, in, zero());
        for (const auto& [k, n] : p504check::kind_counts(w)) {
            CAPTURE(k);
            CHECK(n == 0);
        }
        for (const eng::Row& r : a.t.relation) CHECK((r.at(4).bytes != "alliance" && r.at(4).bytes != "trade"));
        for (const eng::Row& s : a.t.site) CHECK(p503check::at(s, 2) != 3);
    }
}

TEST_CASE("P5-04 18.2: the invariants of 13 and AL9 on the shipped data's worlds, seeds 0 to 9 at 512") {
    for (std::uint64_t seed = 0; seed < 10; ++seed) check_world_file(seed, 512);
}

TEST_CASE("P5-04 18.2, AL4: elves and faeries alive together have an origin alliance (512, seeds 0 to 9)") {
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        CAPTURE(seed);
        const db::WorldReader r = chrono_test::open_world(p503run::world_path(seed, 512));
        check_origin(p503check::world_of_file(r, conflict_test::shipped()));
    }
}

TEST_CASE("P5-04 14: determinism: alliances-small twice; a world at one and at four threads") {
    const eng::HistoryInputs in = p503run::stress_inputs(1, alliances_small());
    const p503api::RTables a = p503api::history_stage(alliances_small(), in);
    const p503api::RTables b = p503api::history_stage(alliances_small(), in);
    CHECK(a.t.relation == b.t.relation);
    CHECK(a.t.event == b.t.event);
    CHECK(a.t.event_participant == b.t.event_participant);
    CHECK(a.t.site == b.t.site);
    const std::string one = p503run::world_path(42, 512, "normal", 6000, "all", "", 1);
    const std::string four = p503run::world_path(42, 512, "normal", 6000, "all", "", 4);
    CHECK(dbtest::read_file(one) == dbtest::read_file(four));
}

}  // TEST_SUITE

TEST_SUITE("alliances-slow" * doctest::skip()) {

TEST_CASE("P5-04 18.2: the invariants of 13 and AL9 at 1024, seeds 0 to 19") {
    for (std::uint64_t seed = 0; seed < 20; ++seed) check_world_file(seed, 1024);
}

// Validation finding 1 (0.26.0): seed 6 at 1024 with four civilizations per race, where a third realm's conquest
// (e1954, c8 conquers c5) ended the war of the principal c17, who then settled spoils on it (e1955). Spec 13 item 11
// and 6.11: a share is owed only on an end whose own winner is the principal; no spoils event may cite another
// realm's conquest.
TEST_CASE("P5-04 13 item 11: no spoils on a war a third realm ended by conquest (civs 4, seed 6 at 1024)") {
    chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("all"));
    p.civs = 4;
    const std::string path = chrono_test::world_dir().file("civs4-seed-6-1024.db");
    const worldgen::WorldParams wp = chrono_test::world_params(6, 1024, p, conflict_test::shipped());
    REQUIRE(worldgen::generate_world(wp, conflict_test::shipped(), path, db::Overwrite::refuse).ok());
    const db::WorldReader r = chrono_test::open_world(path);
    const p503check::World w = p503check::world_of_file(r, conflict_test::shipped());
    const p504check::Rows rows(w);
    const p503check::Index x(w);
    std::int64_t conquest_cited = 0;
    for (const char* k : {"spoils_shared", "spoils_withheld"}) {
        for (const std::int64_t e : rows.of(k)) {
            for (const std::int64_t c : x.cause_list(e)) {
                if (rows.kind(c) != "civ_conquered") continue;
                ++conquest_cited;
                CAPTURE(e);
                CAPTURE(c);
                CHECK(rows.one(c, "conqueror") == rows.one(e, "principal"));
                CHECK(rows.one(c, "civilization") == rows.one(e, "enemy"));
            }
        }
    }
    MESSAGE("spoils events citing a conquest: " << conquest_cited);
    const p504check::Geo g = p504check::geo_of_file(r);
    CHECK(p504check::violations(w, g, p504check::data_of(conflict_test::shipped())) == std::vector<std::string>{});
}

}  // TEST_SUITE
