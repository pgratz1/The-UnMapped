// The engineer's checks of P5-02 (the independent tests are the test
// writer's): invariant I0 (7.5) on a few 512 worlds, the growth identity of
// 7.1 at G = 1000, the yearly beast step, the possessive and {stem} rules of
// 21.2, chance_ppb's threshold and the settlement endings of 8.3.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p3_06_conflict_support.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/stages/history.hpp"

namespace {

namespace wh = worldgen::history;
namespace wd = worldgen::data;

// A definition's record of kind history with fields replaced.
wd::ContentSet with_fields(const wd::ContentSet& base, const std::string& id,
                           const std::map<std::string, wd::Value>& edits) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "history") {
            continue;
        }
        for (wd::Definition& d : k.definitions) {
            if (d.id != id) {
                continue;
            }
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const auto it = edits.find(d.record.name_at(i));
                r.add(d.record.name_at(i), it != edits.end() ? it->second : d.record.value_at(i));
            }
            d.record = std::move(r);
            return c;
        }
    }
    FAIL("no history." << id);
    return c;
}

// The fixture tuning `zero` of 7.5.
wd::ContentSet zero_content(const wd::ContentSet& base) {
    wd::Record pressure;
    pressure.add("low", wd::Value::integer(0));
    pressure.add("normal", wd::Value::integer(0));
    pressure.add("high", wd::Value::integer(0));
    wd::ContentSet c = with_fields(base, "monsters",
                                   {{"pressure", wd::Value::record(std::move(pressure))},
                                    {"drag_max_permille", wd::Value::integer(0)},
                                    {"feed_max_permille", wd::Value::integer(0)},
                                    {"bold_permille", wd::Value::integer(0)},
                                    {"outpost_permille", wd::Value::integer(0)}});
    c = with_fields(c, "peoples", {{"ghost_permille", wd::Value::integer(0)}});
    // P5-03 7.4: the relations neutral too (the zero tuning, no stances), so
    // that the stage without the monsters (and so without the relations)
    // is the identity's other side.
    c = with_fields(c, "relations",
                    {{"war_memory", wd::Value::integer(0)}, {"conquest_memory", wd::Value::integer(0)},
                     {"sack_memory", wd::Value::integer(0)}, {"capture_memory", wd::Value::integer(0)},
                     {"expel_memory", wd::Value::integer(0)}, {"shelter_memory", wd::Value::integer(0)},
                     {"meet_hops", wd::Value::integer(-1)}, {"welcome_min", wd::Value::integer(2001)},
                     {"subject_years", wd::Value::integer(100000)}, {"mixed_min_permille", wd::Value::integer(1001)},
                     {"expel_permille", wd::Value::integer(0)},
                     // P5-03a 10.1: the zero values of the new fields.
                     {"peace_memory_per_century", wd::Value::integer(0)}, {"foe_war_memory", wd::Value::integer(0)},
                     {"foe_monster_memory", wd::Value::integer(0)}, {"meet_sealed_hops", wd::Value::integer(-1)},
                     {"conquest_settlers_span_permille", wd::Value::integer(0)}});
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") {
            continue;
        }
        for (wd::Definition& d : k.definitions) {
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                r.add(d.record.name_at(i),
                      d.record.name_at(i) == "stances" ? wd::Value::array({}) : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

// The shipped configuration without MonstersSubsystem and without a wild view.
worldgen::stages::HistoryResult without_monsters(const wd::ContentSet& content, const wh::HistoryInputs& in) {
    const wh::EventCatalogue catalogue = wh::event_catalogue(content);
    const wh::RenamingTuning renaming_tuning = wh::renaming_tuning_from(content);
    wh::PeoplesSubsystem peoples;
    wh::PeoplesAwakening awakening(peoples);
    wh::ConflictSubsystem conflict(peoples, content);
    wh::RenamingSubsystem renaming(peoples, renaming_tuning, catalogue);
    wh::EmptyInterventionSource none;
    wh::EngineConfig config;
    config.awakening = &awakening;
    config.subsystems = {&peoples, &conflict, &renaming};
    config.tension = {&conflict};
    config.interventions = &none;
    config.catalogue = &catalogue;
    worldgen::stages::HistoryResult r = wh::run_engine(content, in, config);
    r.region_names = renaming.sorted_rows();
    return r;
}

wh::HistoryInputs inputs_of(std::uint64_t seed, std::int64_t map_size) {
    static dbtest::TempDir dir("p5-02-engineer");
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
    const worldgen::stages::HistoryResult b = without_monsters(zero, in);
    CHECK(a.event_kinds == b.event_kinds);
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
    CHECK(a.monsters.empty());
    CHECK(a.sites.empty());
    CHECK(a.beasts.size() == 5);
}

}  // namespace

TEST_CASE("p5-02 engineer: I0, zero tuning against the subsystem removed (512 seeds 0 and 1)") {
    check_i0(0, 512);
    check_i0(1, 512);
}

TEST_SUITE("p5-02-engineer-slow" * doctest::skip()) {
TEST_CASE("p5-02 engineer: I0 at 1024 (seeds 0 and 1)") {
    check_i0(0, 1024);
    check_i0(1, 1024);
}
}

TEST_CASE("p5-02 engineer: growth at G = 1000 is 9.1's") {
    for (std::int64_t p = 1; p < 400; p += 7) {
        for (std::int64_t g = 0; g <= 1000; g += 125) {
            const std::int64_t share = 400;
            const std::int64_t a = wh::grow_population(p, share, g, 450, 30, 1000);
            // 9.1 with G = 1000 written as P5-02 7.1's formula.
            std::int64_t d = (p * g * 1000 * (share - p) + 500 * 1000 * share) / (1000 * 1000 * share);
            if (g > 0) d = std::max<std::int64_t>(1, d);
            CHECK(a == std::min(p + d, std::int64_t{450}));
        }
    }
    CHECK(wh::grow_population(100, 400, 300, 450, 30, 800) < wh::grow_population(100, 400, 300, 450, 30, 1100));
}

TEST_CASE("p5-02 engineer: one year of beasts") {
    CHECK(wh::beast_year(0, 10, 200, 200) == 1);
    CHECK(wh::beast_year(0, 0, 200, 200) == 0);
    CHECK(wh::beast_year(5, 10, 200, 200) == 6);
    CHECK(wh::beast_year(9, 10, 1000, 200) == 10);
    CHECK(wh::beast_year(20, 10, 200, 200) == 18);
    CHECK(wh::beast_year(11, 10, 200, 200) == 10);
    CHECK(wh::beast_year(10, 10, 200, 200) == 10);
}

TEST_CASE("p5-02 engineer: chance_ppb thresholds") {
    CHECK_FALSE(wh::chance_ppb(0, 0));
    CHECK(wh::chance_ppb(0, 1));
    CHECK(wh::chance_ppb(~std::uint64_t{0}, 1000000000));
    CHECK_FALSE(wh::chance_ppb(std::uint64_t{1} << 63, 1));
}

TEST_CASE("p5-02 engineer: stems and the possessive") {
    const std::vector<std::string> goblin = {"{root}", "{root} Pit", "{root} Hole", "{root}gash"};
    CHECK(wh::settlement_stem("Nub Pit", goblin) == "Nub");
    CHECK(wh::settlement_stem("Krax Hole", goblin) == "Krax");
    CHECK(wh::settlement_stem("Bugg-Drux Pit", goblin) == "Bugg-Drux");
    CHECK(wh::settlement_stem("Pit", goblin) == "Pit");
    CHECK(wh::settlement_stem("Edadbury", {"{root}", "{root}bury"}) == "Edadbury");
    CHECK(wh::fill_rename_form("{stem}'s Fall", "Omos Pit", "Heath", "Omos").name == "Omos' Fall");
    CHECK(wh::fill_rename_form("{stem}'s Fall", "Nub Pit", "Heath", "Nub").name == "Nub's Fall");
    CHECK(wh::fill_rename_form("The Ashes of {name}", "Nub Pit", "Heath", "Nub").name == "The Ashes of Nub Pit");
    const std::array<wh::SlotValue, 1> slots = {wh::SlotValue{"{root}", "Iersaes"}};
    const wh::RenamedName lair = wh::fill_slots("{root}'s Deep", slots);
    CHECK(lair.name == "Iersaes' Deep");
    CHECK(lair.joins == std::vector<std::size_t>{7});
}
