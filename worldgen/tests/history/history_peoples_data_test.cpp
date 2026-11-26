// Spec P3-05 12 and 16.1 items 15 and 16: the tuning history.peoples (every
// field's range, each named check's message once, required fields), the
// race field density_permille (required, int(1, 1000), after
// growth_permille), the 0.15.0 race values of 12.2, and the 0.16.0 items
// that must be absent at 0.15.0 (P3-01 r7: prowess_permille, goblin
// aggression 600, id `conflict`).
//
// Written from the spec only, on the P3-01 test transcription
// (p3_01_history_data.hpp, updated to 0.15.0 by these tests) loaded
// through the production registry. These cases need no P3-05 code, only
// its data and kind registration: they fail until the 0.15.0 data lands.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "../p3_01_history_data.hpp"
#include "../p3_01_history_support.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/value.hpp"

namespace {

namespace wd = worldgen::data;
using history_data::Files;
using namespace history_test;

Files with_peoples_edit(std::string_view from, std::string_view to) {
    Files f = history_data::shipped_files();
    std::string& t = f["history/peoples.toml"];
    const std::size_t at = t.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in peoples.toml");
    t.replace(at, from.size(), to);
    return f;
}

wd::LoadError one_error(const Load& l) {
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

const wd::Record& peoples_record(const wd::ContentSet& c) {
    const wd::KindContent* h = c.find_kind("history");
    REQUIRE(h != nullptr);
    for (const wd::Definition& d : h->definitions) {
        if (d.id == "peoples") {
            return d.record;
        }
    }
    FAIL("no history.peoples");
    return h->definitions.front().record;
}

// 12.1: (field, low, high) in table order; records list each of their fields.
struct Range {
    const char* field;
    std::int64_t lo, hi;
};
const std::vector<Range>& ranges() {
    static const std::vector<Range> v = {
        {"score_site", 0, 2000}, {"score_biome", 0, 2000}, {"score_upland", 0, 2000}, {"score_coast", 0, 2000},
        {"ore_reach_full", 1, 100000}, {"coast_full", 1, 100000}, {"rough_base", 0, 1000}, {"rough_upland", 0, 1000},
        {"people_per_kcell", 1, 100000}, {"capacity_floor", 1, 100000}, {"capital_weight", 1000, 10000},
        {"founding_population", 1, 10000}, {"village_min", 1, 1000000}, {"town_min", 1, 1000000},
        {"city_min", 1, 1000000}, {"city_max", 1, 1000000}, {"fortress_max", 1, 1000000},
        {"kind_shrink_permille", 0, 1000}, {"site_spacing", 1, 1000}, {"region_slots_max", 1, 64},
        {"mouth_class_min", 1, 15}, {"port_radius", 0, 16}, {"port_mouth_bonus", 0, 255}, {"port_coast_bonus", 0, 255},
        {"decline_permille", 1, 1000}, {"recover_permille", 0, 1000}, {"colonist_permille", 1, 1000},
        {"colonists_min", 1, 10000}, {"abandon_min", 1, 10000}, {"abandon_frail_max", 1, 1000000},
        {"abandon_permille", 0, 1000}, {"found_pressure_permille", 1, 2000}, {"found_rate_permille", 0, 1000},
        {"quiet_found_permille", 0, 1000}, {"reach_base", 0, 64}, {"reach_per_expansion", 0, 64},
        {"expand_min_score", -3000, 4000}, {"expand_relative_permille", 0, 1000}, {"expand_min_capacity", 0, 1000000},
        {"cost_penalty", 0, 10000}, {"pass_cost", 1, 100}, {"river_cost", 0, 100},
        {"river_border_share_permille", 0, 1000}, {"civs_two_permille", 0, 1000}, {"regions_per_civ", 0, 100},
        // r8 21 item 5 (0.18.0, P3-09): after regions_per_civ
        {"regions_per_extra_civ", 1, 100000}, {"civs_extra_max", 0, 3},
        // r12 22.2, 22.5 (0.19.0, P3-10): after civs_extra_max
        {"cause_memory_years", 0, 100000}, {"inland_cap", 1, 256},
        {"home_min_hops", 1, 64}, {"home_min_capacity", 0, 1000000}, {"intensity", 0, 10000},
        {"famine_crowding_permille", 0, 2000}, {"famine_ppm", 0, 1000000}, {"famine_toll_min", 0, 1000},
        {"famine_toll_max", 0, 1000}, {"plague_ppm", 0, 1000000}, {"plague_toll_min", 0, 1000},
        {"plague_toll_max", 0, 1000}, {"plague_spread_permille", 0, 1000}, {"plague_spread_toll_permille", 0, 1000},
        {"crisis_per_generation_permille", 0, 1000}, {"crisis_loss_permille", 0, 1000},
        {"split_min_settlements", 2, 1000}, {"split_min_regions", 2, 1000}, {"split_permille", 0, 1000},
        {"colony_new_region_salience", 0, 100}, {"plague_small_salience", 0, 100},
        // r6 (0.17.0, P3-08): after plague_small_salience
        {"plague_major_dead", 0, 1000000}, {"crisis_small_salience", 0, 100},
        // P5-02 4.5 (0.23.0): after crisis_small_salience
        {"ghost_permille", 0, 1000}, {"ghost_max_population", 0, 1000000},
        // P5-03 11.1, 11.2 (0.24.0): after ghost_max_population
        {"abandon_reach_hops", 0, 16}, {"abandon_loss_min_permille", 0, 1000}, {"abandon_loss_max_permille", 0, 1000},
    };
    return v;
}

// The first line of the listing that sets `field` ("field = ..."), its value text.
std::string listing_line(std::string_view field) {
    const std::string t(history_data::PEOPLES_TOML);
    const std::size_t at = t.find("\n" + std::string(field) + " = ");
    REQUIRE(at != std::string::npos);
    const std::size_t end = t.find('\n', at + 1);
    return t.substr(at + 1, end - at - 1);
}

}  // namespace

TEST_CASE("P3-05 12.1 (r6, r8): history.peoples loads with the listing's 73 fields in table order and values (r12: 0.19.0)") {
    const Load l = load(history_data::shipped_files());
    INFO(dump(l));
    REQUIRE(l.ok());
    const wd::Record& r = peoples_record(*l.content);
    REQUIRE(r.size() == ranges().size());
    for (std::size_t i = 0; i < r.size(); ++i) {
        CHECK(r.name_at(i) == ranges()[i].field);
    }
    CHECK(r.get_int("people_per_kcell") == 2500);
    CHECK(r.get_int("capital_weight") == 3000);
    CHECK(r.get_int("site_spacing") == 32);
    CHECK(r.get_int("home_min_capacity") == 1500);
    CHECK(r.get_int("expand_min_score") == 250);
    CHECK(r.get_record("abandon_permille").get_int("hamlet") == 300);
    CHECK(r.get_record("abandon_permille").get_int("city") == 0);
    CHECK(r.get_record("plague_ppm").get_int("city") == 3000);
    CHECK(r.get_record("intensity").get_int("turbulent") == 2000);
    CHECK(r.get_int("plague_small_salience") == 25);
    // r6 (0.17.0): the halved rates and the two new fields
    CHECK(r.get_int("famine_ppm") == 1250);
    CHECK(r.get_int("plague_spread_permille") == 100);
    CHECK(r.get_int("plague_major_dead") == 1000);
    CHECK(r.get_int("crisis_small_salience") == 25);
    // r8 21 item 5 (0.18.0): more realms on a larger map
    CHECK(r.get_int("regions_per_extra_civ") == 90);
    CHECK(r.get_int("civs_extra_max") == 2);
    // r12 22.2, 22.5 (0.19.0)
    CHECK(r.get_int("cause_memory_years") == 50);
    CHECK(r.get_int("inland_cap") == 20);
    CHECK(r.size() == 75U);   // P5-06 25.1: ruin_fade_years and ghost_fade_years removed; P5-03 11.1, 11.2: four more
    // the shipped content (game/data) holds the same record (12.1: identical copies)
    CHECK(peoples_record(shipped_content()) == r);
}

TEST_CASE("P3-05 16.1 item 15: every history.peoples field one step outside its range is a range error") {
    for (const Range& rg : ranges()) {
        CAPTURE(std::string_view(rg.field));
        const std::string line = listing_line(rg.field);
        const bool record = line.find('{') != std::string::npos;
        auto with = [&](std::int64_t v) {
            if (!record) {
                return with_peoples_edit(line, std::string(rg.field) + " = " + std::to_string(v));
            }
            // the first field of the record
            const std::size_t eq = line.find(" = ", line.find('{'));
            const std::size_t comma = line.find(',', eq);
            return with_peoples_edit(line, line.substr(0, eq + 3) + std::to_string(v) + line.substr(comma));
        };
        // in range at both bounds (named checks aside: only fields they do not involve are moved to a bound)
        const Load hi = load(with(rg.hi + 1));
        const wd::LoadError e = one_error(hi);
        CHECK(e.path == "history/peoples.toml");
        CHECK(contains(e.message, "history.peoples." + std::string(rg.field)));
        CHECK(contains(e.message, "expected"));
        if (rg.lo > INT64_MIN) {
            const wd::LoadError e2 = one_error(load(with(rg.lo - 1)));
            CHECK(contains(e2.message, "history.peoples." + std::string(rg.field)));
        }
    }
}

TEST_CASE("P3-05 12.1: each named check of history.peoples once, with its message") {
    struct C {
        const char* from;
        const char* to;
        const char* check;
        const char* message;
    };
    for (const C& c : std::vector<C>{
             {"town_min = 1500", "town_min = 250", "peoples_ladder",
              "village_min, town_min, city_min must increase and city_max must not be below city_min"},
             {"city_max = 25000", "city_max = 7999", "peoples_ladder",
              "village_min, town_min, city_min must increase and city_max must not be below city_min"},
             {"famine_toll_max = 200", "famine_toll_max = 49", "peoples_tolls", "a toll minimum must not be above its maximum"},
             {"plague_toll_min = 100", "plague_toll_min = 301", "peoples_tolls", "a toll minimum must not be above its maximum"},
             {"capacity_floor = 400", "capacity_floor = 299", "peoples_floor", "capacity_floor must not be below founding_population"},
             {"abandon_min = 40", "abandon_min = 60", "peoples_colonists", "colonists_min must be above abandon_min"},
         }) {
        CAPTURE(std::string_view(c.to));
        const wd::LoadError e = one_error(load(with_peoples_edit(c.from, c.to)));
        CHECK(e.path == "history/peoples.toml");
        CHECK(e.line == 1U);
        CHECK(e.message == "check '" + std::string(c.check) + "' failed for 'history.peoples': " + c.message);
    }
    // the edges that pass: city_max = city_min, toll min = max, capacity_floor = founding_population
    CHECK(load(with_peoples_edit("city_max = 25000", "city_max = 8000")).ok());
    CHECK(load(with_peoples_edit("famine_toll_max = 200", "famine_toll_max = 50")).ok());
    CHECK(load(with_peoples_edit("capacity_floor = 400", "capacity_floor = 300")).ok());
    CHECK(load(with_peoples_edit("abandon_min = 40", "abandon_min = 59")).ok());
}

TEST_CASE("P3-05 12.1: every history.peoples field is required and no other key is accepted") {
    for (const Range& rg : ranges()) {
        CAPTURE(std::string_view(rg.field));
        const std::string line = listing_line(rg.field);
        const wd::LoadError e = one_error(load(with_peoples_edit(line + "\n", "")));
        CHECK(e.message == "missing required field 'history.peoples." + std::string(rg.field) + "'");
    }
    CHECK_FALSE(load(with_peoples_edit("score_site = 1000", "score_site = 1000\nscore_magic = 1")).ok());
}

TEST_CASE("P3-05 16.1 item 16, 12.2 item 1: density_permille is required, int(1, 1000), after growth_permille") {
    const Load l = load(history_data::shipped_files());
    REQUIRE(l.ok());
    const wd::KindContent* race = l.content->find_kind("race");
    REQUIRE(race != nullptr);
    const std::map<std::string, std::int64_t> want = {{"dwarf", 700}, {"elf", 300}, {"faerie", 250},
                                                       {"goblin", 900}, {"human", 1000}, {"orc", 800}};
    for (const wd::Definition& d : race->definitions) {
        CAPTURE(d.id);
        CHECK(d.record.get_int("density_permille") == want.at(d.id));
        std::size_t g = 0, k = 0;
        for (std::size_t i = 0; i < d.record.size(); ++i) {
            if (d.record.name_at(i) == "growth_permille") g = i;
            if (d.record.name_at(i) == "density_permille") k = i;
        }
        CHECK(k == g + 1);
    }
    Files f = history_data::shipped_files();
    std::string& orc = f["races/orc.toml"];
    orc.replace(orc.find("density_permille = 800\n"), 23, "");
    CHECK(one_error(load(f)).message == "missing required field 'race.orc.density_permille'");
}

TEST_CASE("P3-05 12.2 item 2, 12.3 (r6): the goblin biome weights of 0.15.0 and the faerie weights of 0.17.0") {
    const Load l = load(history_data::shipped_files());
    REQUIRE(l.ok());
    const wd::KindContent* race = l.content->find_kind("race");
    auto weights = [&](std::string_view id) {
        std::map<std::string, std::int64_t> m;
        for (const wd::Definition& d : race->definitions) {
            if (d.id == id) {
                for (const wd::Value& b : d.record.get_array("biomes")) {
                    m[b.as_record().get_string("biome")] = b.as_record().get_int("weight");
                }
            }
        }
        return m;
    };
    const auto gob = weights("goblin");
    CHECK(gob.at("temperate_forest") == -200);
    CHECK(gob.at("wet_woodland") == -300);
    CHECK(gob.size() == 8U);
    const auto fae = weights("faerie");
    // 12.3 candidate A (shipped from 0.17.0, P3-08 item 4): nine rows, heath kept with weight 0
    const std::map<std::string, std::int64_t> want_fae = {
        {"temperate_forest", 800}, {"wet_woodland", 800}, {"fen", 300},       {"marsh", 200},  {"highland_moor", 100},
        {"heath", 0},              {"grassland", -300},   {"alpine", -400},   {"steppe", -400}};
    CHECK(fae == want_fae);
    for (const wd::Definition& d : race->definitions) {
        if (d.id == "faerie") CHECK(d.record.get_int("upland") == 200);
    }
}

TEST_CASE("P3-05 r6 6.2, P3-01 r8 (1): cell_weights is required, after site_weights; dwarves 400, 600, the rest 0") {
    const Load l = load(history_data::shipped_files());
    INFO(dump(l));
    REQUIRE(l.ok());
    const wd::KindContent* race = l.content->find_kind("race");
    REQUIRE(race != nullptr);
    for (const wd::Definition& d : race->definitions) {
        CAPTURE(d.id);
        const wd::Record& cw = d.record.get_record("cell_weights");
        CHECK(cw.size() == 5U);  // P3-01 r11 3.4 (0.19.0): inland, port, habitability after height
        CHECK(cw.name_at(0) == "ore");
        CHECK(cw.name_at(1) == "height");
        CHECK(cw.name_at(2) == "inland");
        CHECK(cw.name_at(3) == "port");
        CHECK(cw.name_at(4) == "habitability");
        CHECK(cw.get_int("ore") == (d.id == "dwarf" ? 400 : 0));
        CHECK(cw.get_int("height") == (d.id == "dwarf" ? 600 : 0));
        std::size_t sw = 0, c = 0;
        for (std::size_t i = 0; i < d.record.size(); ++i) {
            if (d.record.name_at(i) == "site_weights") sw = i;
            if (d.record.name_at(i) == "cell_weights") c = i;
        }
        CHECK(c == sw + 1);
    }
    // the shipped content holds the same records
    CHECK(shipped_content().find_kind("race")->definitions.size() == race->definitions.size());
    // required, and each field int(0, 1000). On orc (order 6): a definition failing type validation is
    // left out of the kind checks (P0-03 amendment 16), so editing dwarf (order 3) would add a
    // race_order_dense error (integration fix).
    Files f = history_data::shipped_files();
    std::string& dw = f["races/orc.toml"];
    const std::size_t at = dw.find("cell_weights = ");
    REQUIRE(at != std::string::npos);
    const std::size_t end = dw.find('\n', at);
    Files missing = f;
    missing["races/orc.toml"].erase(at, end - at + 1);
    CHECK(contains(one_error(load(missing)).message, "cell_weights"));
    Files high = f;
    // (P3-01 r11 3.4, 0.19.0: the record holds inland, port and habitability too)
    high["races/orc.toml"].replace(at, end - at, "cell_weights = { ore = 1001, height = 0, inland = 0, port = 1000, habitability = 1000 }");
    CHECK(contains(one_error(load(high)).message, "race.orc.cell_weights.ore"));
    Files low = f;
    low["races/orc.toml"].replace(at, end - at, "cell_weights = { ore = 0, height = -1, inland = 0, port = 1000, habitability = 1000 }");
    CHECK(contains(one_error(load(low)).message, "race.orc.cell_weights.height"));
}

TEST_CASE("P3-06 12.2, 16.1 item 13 / P3-01 r7: prowess_permille and goblin aggression 600 at 0.16.0") {
    const wd::ContentSet& c = shipped_content();
    const wd::KindContent* race = c.find_kind("race");
    REQUIRE(race != nullptr);
    const std::map<std::string, std::int64_t> prowess = {{"dwarf", 1400}, {"elf", 1600}, {"faerie", 1200},
                                                         {"goblin", 800}, {"human", 1000}, {"orc", 1300}};
    for (const wd::Definition& d : race->definitions) {
        CAPTURE(d.id);
        CHECK(d.record.get_int("prowess_permille") == prowess.at(d.id));
        if (d.id == "goblin") {
            CHECK(d.record.get_int("aggression") == 600);
        }
    }
    // prowess_permille is required (no default): removing it from orc (order 6) is one error
    Files f = history_data::shipped_files();
    std::string& orc = f["races/orc.toml"];
    orc.replace(orc.find("prowess_permille = 1300\n"), 24, "");
    CHECK(contains(one_error(load(f)).message, "prowess_permille"));
    // history id conflict is registered (the P3-01 unknown-id test names the list)
    const wd::KindContent* hist = c.find_kind("history");
    REQUIRE(hist != nullptr);
    std::vector<std::string> ids;
    for (const wd::Definition& d : hist->definitions) {
        ids.push_back(d.id);
    }
    CHECK(ids == std::vector<std::string>{"alliances", "chronology", "conflict", "engine", "figures", "monsters", "peoples", "relations", "renaming", "report"});   // P5-03 4.2; P5-06
}
