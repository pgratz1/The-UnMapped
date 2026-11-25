// The engineer's checks of P5-03 (the independent tests are the test
// writer's): proof obligation I0 (7.4) on a few 512 worlds, the
// largest-remainder split of 6.3.2, and the shares and rows of a generated
// world (invariants 3 and 4 in part, on the stage's rows).

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p3_06_conflict_support.hpp"
#include "p5_03_relations_api.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/relations.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/stages/history.hpp"

namespace {

namespace wh = worldgen::history;
namespace wd = worldgen::data;

// A definition of kind `kind` with fields replaced.
wd::ContentSet with_fields(const wd::ContentSet& base, const std::string& kind, const std::string& id,
                           const std::map<std::string, wd::Value>& edits) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != kind) {
            continue;
        }
        for (wd::Definition& d : k.definitions) {
            if (d.id != id && !id.empty()) {
                continue;
            }
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const auto it = edits.find(d.record.name_at(i));
                r.add(d.record.name_at(i), it != edits.end() ? it->second : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

// The fixture tuning `zero` of 7.4, every race's stances empty.
wd::ContentSet zero_content(const wd::ContentSet& base) {
    const wd::ContentSet c = with_fields(base, "history", "relations",
                                         {{"war_memory", wd::Value::integer(0)},
                                          {"conquest_memory", wd::Value::integer(0)},
                                          {"sack_memory", wd::Value::integer(0)},
                                          {"capture_memory", wd::Value::integer(0)},
                                          {"expel_memory", wd::Value::integer(0)},
                                          {"shelter_memory", wd::Value::integer(0)},
                                          {"meet_hops", wd::Value::integer(-1)},
                                          {"welcome_min", wd::Value::integer(2001)},
                                          {"subject_years", wd::Value::integer(100000)},
                                          {"mixed_min_permille", wd::Value::integer(1001)},
                                          {"expel_permille", wd::Value::integer(0)},
                                          // P5-03a 10.1: the zero values of the new fields.
                                          {"peace_memory_per_century", wd::Value::integer(0)},
                                          {"foe_war_memory", wd::Value::integer(0)},
                                          {"foe_monster_memory", wd::Value::integer(0)},
                                          {"meet_sealed_hops", wd::Value::integer(-1)},
                                          {"conquest_settlers_span_permille", wd::Value::integer(0)}});
    // P5-04 7.14 (0.26.0): the alliances at their zero too (the stage without relations builds none)
    const wd::ContentSet a = with_fields(c, "history", "alliances",
                                         {{"ally_hops", wd::Value::integer(-1)}, {"trade_hops", wd::Value::integer(-1)},
                                          {"mine_permille", wd::Value::integer(0)}});
    // P5-06 7.11: and the figures at their zero
    return p503api::figures_zero_of(
        with_fields(a, "race", "", {{"stances", wd::Value::array({})}, {"origin_allies", wd::Value::array({})}}));
}

// The shipped configuration without RelationsSubsystem, no ledger and no
// disposition view.
worldgen::stages::HistoryResult without_relations(const wd::ContentSet& content, const wh::HistoryInputs& in) {
    const wh::EventCatalogue catalogue = wh::event_catalogue(content);
    const wh::RenamingTuning renaming_tuning = wh::renaming_tuning_from(content);
    wh::PeoplesSubsystem peoples;
    wh::PeoplesAwakening awakening(peoples);
    wh::ConflictSubsystem conflict(peoples, content);
    wh::MonstersSubsystem monsters(peoples, content);
    wh::RenamingSubsystem renaming(peoples, renaming_tuning, catalogue);
    peoples.set_wild(&monsters);
    renaming.set_monsters(&monsters);
    monsters.set_region_names(&renaming);
    wh::EmptyInterventionSource none;
    wh::EngineConfig config;
    config.awakening = &awakening;
    config.subsystems = {&peoples, &conflict, &renaming, &monsters};
    config.tension = {&conflict, &monsters};
    config.interventions = &none;
    config.catalogue = &catalogue;
    worldgen::stages::HistoryResult r = wh::run_engine(content, in, config);
    r.region_names = renaming.sorted_rows();
    monsters.finish(in.params.span);
    r.monsters = monsters.monster_rows();
    r.sites = monsters.site_rows();
    r.beast_populations = monsters.beast_population_rows();
    return r;
}

wh::HistoryInputs inputs_of(std::uint64_t seed, std::int64_t map_size) {
    static dbtest::TempDir dir("p5-03-engineer");
    const std::vector<worldgen::stages::StageInfo> stages = worldgen::stages::stages_until(std::string("chronology"));
    const chrono_api::Params hp;
    const worldgen::WorldParams wp = chrono_test::world_params(seed, map_size, hp, chrono_test::shipped());
    const std::string path = dir.file("s" + std::to_string(seed) + "-" + std::to_string(map_size) + ".db");
    REQUIRE(worldgen::stages::testing::generate_world_with_stages(wp, chrono_test::shipped(), stages, path).ok());
    return wh::history_inputs_from_world(chrono_test::open_world(path), chrono_test::shipped());
}

void check_i0(std::uint64_t seed, std::int64_t map_size) {
    const wh::HistoryInputs in = inputs_of(seed, map_size);
    const wd::ContentSet zero = zero_content(chrono_test::shipped());
    const worldgen::stages::HistoryResult a = worldgen::stages::history_stage(zero, in);
    const worldgen::stages::HistoryResult b = without_relations(zero, in);
    CHECK(a.phases == b.phases);
    CHECK(a.events.size() == b.events.size());
    CHECK(a.events == b.events);
    CHECK(a.event_participants == b.event_participants);
    CHECK(a.event_causes == b.event_causes);
    CHECK(a.event_values == b.event_values);
    CHECK(a.civilizations == b.civilizations);
    CHECK(a.settlements == b.settlements);
    CHECK(a.settlement_census == b.settlement_census);
    CHECK(a.region_control == b.region_control);
    CHECK(a.relations == b.relations);
    CHECK(a.region_names == b.region_names);
    CHECK(a.monsters == b.monsters);
    CHECK(a.sites == b.sites);
    CHECK(a.beast_populations == b.beast_populations);
    CHECK(a.race_relations.empty());
}

// Invariant 3 in part: per settlement, at the year of each of its rows, the
// shares in force sum to 1000 with one ruling race.
void check_shares(const worldgen::stages::HistoryResult& r) {
    std::map<std::int64_t, std::vector<worldgen::db::SettlementRaceRow>> by;
    for (const worldgen::db::SettlementRaceRow& x : r.settlement_races) {
        by[x.settlement].push_back(x);
    }
    for (const auto& [s, rows] : by) {
        for (const worldgen::db::SettlementRaceRow& at : rows) {
            std::map<std::int64_t, worldgen::db::SettlementRaceRow> in_force;
            for (const worldgen::db::SettlementRaceRow& x : rows) {
                if (x.year <= at.year) {
                    in_force[x.race] = x;
                }
            }
            std::int64_t sum = 0;
            std::int64_t ruling = 0;
            for (const auto& [race, x] : in_force) {
                sum += x.share;
                ruling += x.standing == 3 ? 1 : 0;
            }
            CHECK_MESSAGE(sum == 1000, "settlement " << s << " year " << at.year);
            CHECK_MESSAGE(ruling == 1, "settlement " << s << " year " << at.year);
        }
    }
}

}  // namespace

TEST_CASE("p5-03 engineer: I0, zero tuning against the subsystem removed (512 seeds 0 and 1)") {
    check_i0(0, 512);
    check_i0(1, 512);
}

TEST_CASE("p5-03 engineer: the largest-remainder split of 6.3.2") {
    const std::vector<std::int64_t> w = {500, 300, 200};
    CHECK(wh::largest_remainder(w, 1000) == std::vector<std::int64_t>{500, 300, 200});
    const std::vector<std::int64_t> thirds = {1, 1, 1};
    CHECK(wh::largest_remainder(thirds, 1000) == std::vector<std::int64_t>{334, 333, 333});  // ties by index
    const std::vector<std::int64_t> mixed = {1000 * 300, 240 * 1000};
    CHECK(wh::largest_remainder(mixed, 1000) == std::vector<std::int64_t>{556, 444});
    const std::vector<std::int64_t> one_zero = {0, 7};
    CHECK(wh::largest_remainder(one_zero, 900) == std::vector<std::int64_t>{0, 900});
}

TEST_CASE("p5-03 engineer: the shares of a 512 world sum to 1000 with one ruling race") {
    const wh::HistoryInputs in = inputs_of(2, 512);
    const worldgen::stages::HistoryResult r = worldgen::stages::history_stage(chrono_test::shipped(), in);
    CHECK_FALSE(r.settlement_races.empty());
    check_shares(r);
}
