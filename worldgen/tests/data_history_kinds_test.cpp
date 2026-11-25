// The data kinds `race`, `culture` and `history` (spec P3-01 sections 3 to
// 5 and 8.1; the `culture_naming` record of P3-02 3; P0-03 7.5, 8, 9 and
// amendments 8, 10, 15 to 17).
//
// Written from the specs only. Every case loads a data root through the
// production registry: biomes/ and worldgen/ as the shipped game/data holds
// them, and races/, cultures/, history/ from the spec listings transcribed
// in p3_01_history_data.hpp (each definition header at line 1, column 1),
// edited per case. The shipped files on disk are compared with the same
// transcription value for value.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "p3_01_history_data.hpp"
#include "p3_01_history_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/data/value.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

namespace wd = worldgen::data;
using history_data::Files;
using namespace history_test;

Files edited(Files files, const std::string& path, std::string_view from, std::string_view to) {
    const auto it = files.find(path);
    REQUIRE_MESSAGE(it != files.end(), "no file " << path);
    const std::size_t at = it->second.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in " << path);
    it->second.replace(at, from.size(), to);
    return files;
}

Files shipped_edited(const std::string& path, std::string_view from, std::string_view to) {
    return edited(history_data::shipped_files(), path, from, to);
}

wd::LoadError only_error(const Load& l) {
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

// A check failure at a definition's header (P0-03 amendment 8 and 16; P3-01 3).
void at_definition(const wd::LoadError& e, std::string_view path, std::string_view check, std::string_view qualified,
                   std::string_view message) {
    CAPTURE(e.to_string());
    CHECK(e.path == path);
    CHECK(e.line == 1U);
    CHECK(e.column == 1U);
    CHECK(e.message == "check '" + std::string(check) + "' failed for '" + std::string(qualified) +
                           "': " + std::string(message));
}

// A kind-level failure against the directory (P0-03 amendment 16).
void at_directory(const wd::LoadError& e, std::string_view directory, std::string_view check, std::string_view kind,
                  std::string_view message) {
    CAPTURE(e.to_string());
    CHECK(e.path == directory);
    CHECK(e.line == 0U);
    CHECK(e.column == 0U);
    CHECK(e.to_string() == std::string(directory) + ": error: check '" + std::string(check) + "' failed for kind '" +
                               std::string(kind) + "': " + std::string(message));
}

// The errors of `l` whose message names check `name`.
std::vector<wd::LoadError> errors_of_check(const Load& l, std::string_view name) {
    std::vector<wd::LoadError> out;
    for (const wd::LoadError& e : l.errors) {
        if (contains(e.message, "check '" + std::string(name) + "'")) {
            out.push_back(e);
        }
    }
    return out;
}

// The one error of a failing load whose message names `key`, at
// <path>:<line>:<column> with exactly `message`.
void value_error(const Load& l, std::string_view path, std::uint32_t line, std::uint32_t column,
                 std::string_view message) {
    const wd::LoadError e = only_error(l);
    CHECK(e.path == path);
    CHECK(e.line == line);
    CHECK(e.column == column);
    CHECK(e.message == message);
}

void check_race(const wd::Definition& d, const history_data::Race& want) {
    CAPTURE(std::string(want.id));
    const wd::Record& r = d.record;
    REQUIRE(r.size() == history_data::race_fields().size());
    for (std::size_t i = 0; i < r.size(); ++i) {
        CHECK(r.name_at(i) == history_data::race_fields()[i]);
    }
    CHECK(r.get_int("order") == want.order);
    CHECK(r.get_string("name") == want.name);
    CHECK(r.get_string("plural") == want.plural);
    CHECK(r.get_string("adjective") == want.adjective);
    CHECK(r.get_string("camp") == want.camp);
    CHECK(r.get_fx("awakening_share").raw() == want.awakening_share_raw);
    CHECK(r.get_int("lifespan_years") == want.lifespan_years);
    CHECK(r.get_int("generation_years") == want.generation_years);
    CHECK(r.get_int("growth_permille") == want.growth_permille);
    CHECK(r.get_int("density_permille") == want.density_permille);
    const wd::Record& sw = r.get_record("site_weights");
    REQUIRE(sw.size() == 5U);
    for (std::size_t i = 0; i < sw.size(); ++i) {
        CHECK(sw.name_at(i) == history_data::site_weight_fields()[i]);
    }
    CHECK(sw.get_int("fertility") == want.fertility);
    CHECK(sw.get_int("forage") == want.forage);
    CHECK(sw.get_int("timber") == want.timber);
    CHECK(sw.get_int("ore") == want.ore);
    CHECK(sw.get_int("fresh_water") == want.fresh_water);
    // P3-01 r8 (1) (generator 0.17.0): record race_cell_weights {ore, height}; r11 3.4 (0.19.0) adds inland,
    // port and habitability.
    const wd::Record& cw = r.get_record("cell_weights");
    REQUIRE(cw.size() == 5U);
    for (std::size_t i = 0; i < cw.size(); ++i) {
        CHECK(cw.name_at(i) == history_data::cell_weight_fields()[i]);
    }
    CHECK(cw.get_int("ore") == want.cell_ore);
    CHECK(cw.get_int("height") == want.cell_height);
    CHECK(cw.get_int("inland") == want.cell_inland);
    CHECK(cw.get_int("port") == want.cell_port);
    CHECK(cw.get_int("habitability") == want.cell_habitability);
    const auto& biomes = r.get_array("biomes");
    REQUIRE(biomes.size() == want.biomes.size());
    for (std::size_t i = 0; i < biomes.size(); ++i) {
        const wd::Record& b = biomes[i].as_record();
        REQUIRE(b.size() == 2U);
        CHECK(b.name_at(0) == "biome");
        CHECK(b.name_at(1) == "weight");
        CHECK(b.get_string("biome") == want.biomes[i].biome);
        CHECK(b.get_int("weight") == want.biomes[i].weight);
    }
    CHECK(r.get_int("upland") == want.upland);
    CHECK(r.get_int("coast") == want.coast);
    CHECK(r.get_int("expansion") == want.expansion);
    CHECK(r.get_int("aggression") == want.aggression);
    CHECK(r.get_int("prowess_permille") == want.prowess_permille);  // P3-06 12.2 (P3-01 r7)
    // P3-01 r9 3.2 / P3-06 r9 20.12 (generator 0.18.0, P3-09)
    CHECK(r.get_int("muster_permille") == want.muster_permille);
    CHECK(r.get_int("conceal_min") == want.conceal_min);
    CHECK(r.get_int("home_defense_permille") == want.home_defense_permille);
    // P3-01 r11 3.5 (generator 0.19.0, P3-10)
    CHECK(r.get_int("odds_min_permille") == want.odds_min_permille);
    CHECK(r.get_int("awakening_quarrel") == want.awakening_quarrel);
    CHECK(r.get_int("hidden_years") == want.hidden_years);  // P3-01 r13 3.6 (P3-10 second round)
    CHECK(r.get_int("awakening_population") == want.awakening_population);  // P3-01 r14 3.7 (P3-11)
    const auto& kinds = r.get_array("settlement_kinds");
    REQUIRE(kinds.size() == want.settlement_kinds.size());
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        CHECK(kinds[i].as_string() == want.settlement_kinds[i]);
    }
    CHECK(r.get_string("culture") == want.culture);
}

// Every value of P3-01 3.1, 4 and 5.1 in a loaded content set.
void check_shipped_values(const wd::ContentSet& c) {
    const wd::KindContent* race = c.find_kind("race");
    REQUIRE(race != nullptr);
    REQUIRE(race->definitions.size() == history_data::races().size());
    for (std::size_t i = 0; i < race->definitions.size(); ++i) {
        CHECK(race->definitions[i].id == history_data::races()[i].id);  // canonical id order
        check_race(race->definitions[i], history_data::races()[i]);
    }
    const wd::KindContent* culture = c.find_kind("culture");
    REQUIRE(culture != nullptr);
    REQUIRE(culture->definitions.size() == history_data::culture_listings().size());
    for (std::size_t i = 0; i < culture->definitions.size(); ++i) {
        const history_data::CultureListing& want = history_data::culture_listings()[i];
        const wd::Definition& d = culture->definitions[i];
        CAPTURE(std::string(want.id));
        CHECK(d.id == want.id);
        REQUIRE(d.record.size() == 3U);
        for (std::size_t f = 0; f < 3; ++f) {
            CHECK(d.record.name_at(f) == history_data::culture_fields()[f]);
        }
        CHECK(d.record.get_string("race") == want.race);
        CHECK(d.record.get_string("name") == want.name);
        const wd::Record& n = d.record.get_record("naming");
        REQUIRE(n.size() == history_data::naming_fields().size());
        for (std::size_t f = 0; f < n.size(); ++f) {
            CHECK(n.name_at(f) == history_data::naming_fields()[f]);
        }
    }
    const wd::KindContent* history = c.find_kind("history");
    REQUIRE(history != nullptr);
    REQUIRE(history->definitions.size() == 10U);   // figures (P5-06);   // alliances (P5-04 4.2);  // relations (P5-03 4.2); chronology, conflict (P3-06 12.1), engine, monsters (P5-02 4.3), peoples (P3-05 12.1), renaming (P5-12 4), report (P3-04 4)
    CHECK(history->definitions[0].id == "alliances");   // P5-04 4.2: ids in byte order
    const wd::Definition& chrono = history->definitions[1];
    CHECK(chrono.id == "chronology");
    REQUIRE(chrono.record.size() == 3U);
    for (std::size_t f = 0; f < 3; ++f) {
        CHECK(chrono.record.name_at(f) == history_data::chronology_fields()[f]);
    }
    CHECK(chrono.record.get_fx("awakening_jitter_frac").raw() == history_data::CHRONOLOGY_JITTER_RAW);
    CHECK(chrono.record.get_int("awakening_min_gap_years") == history_data::CHRONOLOGY_MIN_GAP_YEARS);
    CHECK(chrono.record.get_string("age_name_form") == history_data::CHRONOLOGY_AGE_NAME_FORM);
}

}  // namespace

// --- Registration (P3-01 3, 4, 5) --------------------------------------------------------

TEST_CASE("P3-01 3-5: the production registry holds race, culture and history beside biome and worldgen") {
    const wd::KindRegistry r = wd::builtin_kinds();
    const auto& kinds = r.kinds();
    REQUIRE(kinds.size() == 9U);
    // Ascending name (P0-03 7.1): the encoding order of P3-01 7; from
    // generator 0.17.0 name_filter (P3-02 r5 11.2) and placename (P1-06 r3
    // 4.1) between history and race (P3-01 r8 (4)); from 0.23.0 beast (first)
    // and monster (after history), P5-02 4.1 and 4.2.
    CHECK(kinds[0].name == "beast");
    CHECK(kinds[1].name == "biome");
    CHECK(kinds[2].name == "culture");
    CHECK(kinds[3].name == "history");
    CHECK(kinds[4].name == "monster");
    CHECK(kinds[5].name == "name_filter");
    CHECK(kinds[6].name == "placename");
    CHECK(kinds[7].name == "race");
    CHECK(kinds[8].name == "worldgen");
    CHECK(kinds[0].directory == "beasts");
    CHECK(kinds[0].required);
    CHECK_FALSE(kinds[0].has_fixed_ids());
    CHECK(kinds[4].directory == "monsters");
    CHECK(kinds[4].required);
    CHECK_FALSE(kinds[4].has_fixed_ids());
    CHECK(kinds[5].directory == "name_filters");
    CHECK(kinds[5].required);
    CHECK_FALSE(kinds[5].has_fixed_ids());
    CHECK(kinds[6].directory == "placenames");
    CHECK(kinds[6].required);
    CHECK_FALSE(kinds[6].has_fixed_ids());

    const wd::Kind& culture = kinds[2];
    CHECK(culture.directory == "cultures");
    CHECK(culture.required);
    CHECK_FALSE(culture.has_fixed_ids());

    const wd::Kind& history = kinds[3];
    CHECK(history.directory == "history");
    CHECK(history.required);
    REQUIRE(history.has_fixed_ids());
    REQUIRE(history.id_schemas.size() == 10U);   // P5-06: figures;   // P5-04 4.2: alliances  // P3-01 5: chronology; engine and report from P3-04; peoples P3-05; conflict P3-06;
                                               // renaming P5-12 (r16 5.4); monsters P5-02 4.3; relations P5-03 4.2
    CHECK(history.id_schemas[0].id == "alliances");   // P5-04 4.2
    CHECK(history.id_schemas[1].id == "chronology");
    CHECK(history.id_schemas[2].id == "conflict");
    CHECK(history.id_schemas[3].id == "engine");
    CHECK(history.id_schemas[4].id == "figures");   // P5-06
    CHECK(history.id_schemas[5].id == "monsters");
    CHECK(history.id_schemas[6].id == "peoples");
    CHECK(history.id_schemas[7].id == "relations");   // P5-03 4.2
    CHECK(history.id_schemas[8].id == "renaming");
    CHECK(history.id_schemas[9].id == "report");
    const wd::Schema* chrono = history.schema_for("chronology");
    REQUIRE(chrono != nullptr);
    REQUIRE(chrono->checks().size() == 1U);
    CHECK(chrono->checks()[0].name == "chronology_age_name_form");
    CHECK(history.schema_for("engine") != nullptr);
    CHECK(history.schema_for("peoples") != nullptr);  // P3-05 12.1
    REQUIRE(history.schema_for("conflict") != nullptr);  // P3-06 12.1, generator 0.16.0 (P3-01 r7)
    REQUIRE(history.schema_for("conflict")->checks().size() == 6U);  // r9 conflict_enmity_hops; r15, r17 quarrel; r22 toll
    CHECK(history.schema_for("conflict")->checks()[0].name == "conflict_declare");
    CHECK(history.schema_for("conflict")->checks()[1].name == "conflict_strain_max");
    CHECK(history.schema_for("conflict")->checks()[2].name == "conflict_enmity_hops");
    CHECK(history.schema_for("conflict")->checks()[3].name == "conflict_quarrel_strain");  // P3-06 r15 21.12
    CHECK(history.schema_for("conflict")->checks()[4].name == "conflict_quarrel_strain_max");  // P3-06 r17 22.7
    CHECK(history.schema_for("conflict")->checks()[5].name == "conflict_toll_spread");  // P3-06 r22 24.5, P3-01 r15 3.8
    // P3-04 4.1 with r15's engine_quiet_min and r19's engine_flare_min (after engine_quiet_min, P3-04 r19 22.3)
    REQUIRE(history.schema_for("engine")->checks().size() == 4U);
    CHECK(history.schema_for("engine")->checks()[0].name == "engine_simulated_bounds");
    CHECK(history.schema_for("engine")->checks()[1].name == "engine_quiet_min");
    CHECK(history.schema_for("engine")->checks()[2].name == "engine_flare_min");
    CHECK(history.schema_for("engine")->checks()[3].name == "engine_phase_budget");

    const wd::Kind& race = kinds[7];
    CHECK(race.directory == "races");
    CHECK(race.required);
    CHECK_FALSE(race.has_fixed_ids());
    REQUIRE(race.schema.fields().size() == history_data::race_fields().size());
    for (std::size_t i = 0; i < race.schema.fields().size(); ++i) {
        CHECK(race.schema.fields()[i].name == history_data::race_fields()[i]);
        CHECK(race.schema.fields()[i].is_required());  // "all required" (P3-01 3)
    }
    // Per-definition checks and kind-level checks, in the order of P3-01 3.
    REQUIRE(race.schema.checks().size() == 4U);
    CHECK(race.schema.checks()[0].name == "race_generation_below_lifespan");
    CHECK(race.schema.checks()[1].name == "race_site_weights_positive");
    CHECK(race.schema.checks()[2].name == "race_biomes_unique");
    CHECK(race.schema.checks()[3].name == "race_settlement_kinds_unique");
    REQUIRE(race.kind_checks.size() == 3U);
    CHECK(race.kind_checks[0].name == "race_order_unique");
    CHECK(race.kind_checks[1].name == "race_order_dense");
    CHECK(race.kind_checks[2].name == "race_awakening_order");

    REQUIRE(culture.schema.fields().size() == 3U);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(culture.schema.fields()[i].name == history_data::culture_fields()[i]);
        CHECK(culture.schema.fields()[i].is_required());
    }
}

// --- 8.1 test 1: the shipped data --------------------------------------------------------

TEST_CASE("P3-01 8.1 test 1: the spec listings load and every value equals the listing") {
    const Load l = load(history_data::shipped_files());
    INFO(dump(l));
    REQUIRE(l.ok());
    check_shipped_values(*l.content);
}

TEST_CASE("P3-01 8.1 test 1: game/data and the two fixture roots hold the listings, as identical copies") {
    const Load want = load(history_data::shipped_files());
    REQUIRE(want.ok());
    for (const char* base : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(std::string_view(base));
        const Load got = load_disk(source_root() / base);
        INFO(dump(got));
        REQUIRE(got.ok());
        check_shipped_values(*got.content);
        // Every validated value of the new kinds, the naming blocks of
        // P3-02 5 included, equals the transcription.
        for (const char* kind : {"race", "culture", "history"}) {
            CAPTURE(std::string_view(kind));
            const wd::KindContent* a = got.content->find_kind(kind);
            const wd::KindContent* b = want.content->find_kind(kind);
            REQUIRE(a != nullptr);
            REQUIRE(b != nullptr);
            CHECK(*a == *b);
        }
        // P3-01 2: "identical copies under worldgen/tests/data/geo/ and
        // worldgen/tests/data/geo-small/": byte for byte, file by file.
        // P3-08 (0.17.0): name_filters/ and placenames/ too (P3-02 r5 11.2, P1-06 r3 4.1).
        // P5-02 4.1, 4.2 (0.23.0): beasts/ and monsters/ too.
        for (const char* d : {"races", "cultures", "history", "name_filters", "placenames", "beasts", "monsters"}) {
            const wd::MemoryDirectory shipped = directory_from_disk(source_root() / "game" / "data", d);
            const wd::MemoryDirectory copy = directory_from_disk(source_root() / base, d);
            REQUIRE(copy.entries.size() == shipped.entries.size());
            for (std::size_t i = 0; i < copy.entries.size(); ++i) {
                CAPTURE(copy.entries[i].name);
                CHECK(copy.entries[i].name == shipped.entries[i].name);
                CHECK(copy.entries[i].bytes == shipped.entries[i].bytes);
            }
        }
    }
    // One file per definition, named after the id (P3-01 3.1, 4 and 5.1).
    const std::filesystem::path data = source_root() / "game" / "data";
    for (const char* f : {"races/elf.toml", "races/goblin.toml", "races/dwarf.toml", "races/faerie.toml",
                          "races/human.toml", "races/orc.toml", "cultures/dwarven.toml", "cultures/elven.toml",
                          "cultures/faerie.toml", "cultures/goblin.toml", "cultures/human.toml",
                          "cultures/orcish.toml", "history/chronology.toml", "history/conflict.toml",
                          "history/engine.toml", "history/peoples.toml", "history/report.toml",
                          // P3-01 r8 (4): the name_filter and placename files (P3-02 r5 11.2, P1-06 r3 4.1)
                          "name_filters/crude.toml", "name_filters/famous.toml", "name_filters/slurs.toml",
                          "name_filters/words.toml", "placenames/carr.toml", "placenames/common.toml",
                          "placenames/downs.toml", "placenames/fells.toml", "placenames/fen.toml", "placenames/heath.toml",
                          "placenames/isle.toml", "placenames/marsh.toml", "placenames/mere.toml", "placenames/moor.toml",
                          "placenames/peaks.toml", "placenames/steppe.toml", "placenames/traits.toml",
                          "placenames/waste.toml", "placenames/wood.toml",
                          // P5-02 4.1 to 4.3 (0.23.0)
                          "history/monsters.toml", "history/renaming.toml", "beasts/bear.toml", "beasts/boar.toml",
                          "beasts/deer.toml", "beasts/giant_spider.toml", "beasts/wolf.toml", "monsters/dragon.toml",
                          "monsters/giant.toml", "monsters/ogre.toml", "monsters/troll.toml", "monsters/undead.toml",
                          "monsters/wyvern.toml"}) {
        CAPTURE(std::string_view(f));
        CHECK(std::filesystem::is_regular_file(data / f));
    }
    // P3-01 5: "a file for a future id must not be committed early".
    CHECK(directory_from_disk(data, "history").entries.size() == 10U);  // figures.toml from 0.27.0 (P5-06); alliances.toml from 0.26.0 (P5-04 4.2),
                                                                         // relations.toml from 0.24.0 (P5-03 4.2),
                                                                         // conflict.toml from 0.16.0 (P3-06 12.1),
                                                                         // renaming.toml from 0.22.0 (P5-12 4),
                                                                         // monsters.toml from 0.23.0 (P5-02 4.3)
}

TEST_CASE("P3-01 3.1: the races in race order are elves, goblins, dwarves, faeries, humans, orcs (design 5)") {
    const Load l = load(history_data::shipped_files());
    REQUIRE(l.ok());
    std::vector<std::pair<std::int64_t, std::string>> by_order;
    const wd::KindContent* race = l.content->find_kind("race");
    REQUIRE(race != nullptr);
    for (const wd::Definition& d : race->definitions) {
        by_order.emplace_back(d.record.get_int("order"), d.id);
    }
    std::sort(by_order.begin(), by_order.end());
    REQUIRE(by_order.size() == 6U);
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(by_order[i].first == static_cast<std::int64_t>(i + 1));
        CHECK(by_order[i].second == history_data::race_order()[i]);
    }
}

// --- 8.1 test 2: per-definition checks (P3-01 3) -----------------------------------------

TEST_CASE("P3-01 3 race_generation_below_lifespan") {
    at_definition(only_error(load(shipped_edited("races/elf.toml", "generation_years = 150", "generation_years = 700"))),
                  "races/elf.toml", "race_generation_below_lifespan", "race.elf",
                  "generation_years must be below lifespan_years");
    at_definition(only_error(load(shipped_edited("races/orc.toml", "generation_years = 18", "generation_years = 51"))),
                  "races/orc.toml", "race_generation_below_lifespan", "race.orc",
                  "generation_years must be below lifespan_years");
    CHECK(load(shipped_edited("races/elf.toml", "generation_years = 150", "generation_years = 699")).ok());
}

TEST_CASE("P3-01 3 race_site_weights_positive") {
    at_definition(only_error(load(shipped_edited(
                      "races/elf.toml", "{ fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }",
                      "{ fertility = 0, forage = 0, timber = 0, ore = 0, fresh_water = 0 }"))),
                  "races/elf.toml", "race_site_weights_positive", "race.elf", "the site_weights must not all be 0");
    // A sum of 1 is enough.
    CHECK(load(shipped_edited("races/elf.toml",
                              "{ fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }",
                              "{ fertility = 0, forage = 0, timber = 0, ore = 0, fresh_water = 1 }"))
              .ok());
}

TEST_CASE("P3-01 3 race_biomes_unique: the later index, naming the first") {
    const std::string last = "  { biome = \"alpine\", weight = -600 },\n";
    at_definition(only_error(load(shipped_edited("races/elf.toml", last,
                                                 last + "  { biome = \"temperate_forest\", weight = 1 },\n"))),
                  "races/elf.toml", "race_biomes_unique", "race.elf",
                  "biomes[7]: biome 'temperate_forest' is already listed at biomes[0]");
    // A biome listed three times, and a second biome repeated: the first
    // repeat only (P3-01 3, revised: the smallest later index).
    at_definition(only_error(load(shipped_edited(
                      "races/elf.toml", last,
                      last + "  { biome = \"heath\", weight = 1 },\n  { biome = \"heath\", weight = 2 },\n"
                             "  { biome = \"steppe\", weight = 3 },\n"))),
                  "races/elf.toml", "race_biomes_unique", "race.elf",
                  "biomes[7]: biome 'heath' is already listed at biomes[3]");
    // The smallest later index decides, not the first biome listed.
    at_definition(only_error(load(shipped_edited("races/elf.toml", last,
                                                 last + "  { biome = \"steppe\", weight = 1 },\n  { biome = \"temperate_forest\", weight = 2 },\n"))),
                  "races/elf.toml", "race_biomes_unique", "race.elf",
                  "biomes[7]: biome 'steppe' is already listed at biomes[4]");
}

TEST_CASE("P3-01 3 race_settlement_kinds_unique") {
    at_definition(only_error(load(shipped_edited("races/elf.toml", "[\"hamlet\", \"village\", \"town\", \"city\"]",
                                                 "[\"hamlet\", \"village\", \"town\", \"city\", \"hamlet\"]"))),
                  "races/elf.toml", "race_settlement_kinds_unique", "race.elf",
                  "settlement_kinds[4]: 'hamlet' is already listed");
    at_definition(only_error(load(shipped_edited("races/faerie.toml", "[\"hamlet\", \"village\"]",
                                                 "[\"village\", \"village\"]"))),
                  "races/faerie.toml", "race_settlement_kinds_unique", "race.faerie",
                  "settlement_kinds[1]: 'village' is already listed");
    // Several repeats: the first only (P3-01 3, revised).
    at_definition(only_error(load(shipped_edited("races/elf.toml", "[\"hamlet\", \"village\", \"town\", \"city\"]",
                                                 "[\"town\", \"hamlet\", \"hamlet\", \"town\", \"town\"]"))),
                  "races/elf.toml", "race_settlement_kinds_unique", "race.elf",
                  "settlement_kinds[2]: 'hamlet' is already listed");
}

TEST_CASE("P3-01 3: the four per-definition checks failing together give four errors at the header") {
    Files f = shipped_edited("races/elf.toml", "generation_years = 150", "generation_years = 701");
    f = edited(f, "races/elf.toml", "{ fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }",
               "{ fertility = 0, forage = 0, timber = 0, ore = 0, fresh_water = 0 }");
    f = edited(f, "races/elf.toml", "{ biome = \"wet_woodland\", weight = 300 }", "{ biome = \"temperate_forest\", weight = 300 }");
    f = edited(f, "races/elf.toml", "[\"hamlet\", \"village\", \"town\", \"city\"]", "[\"town\", \"town\"]");
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE(l.errors.size() == 4U);
    // Sorted by message bytes at one position (P0-03 9).
    at_definition(l.errors[0], "races/elf.toml", "race_biomes_unique", "race.elf",
                  "biomes[1]: biome 'temperate_forest' is already listed at biomes[0]");
    at_definition(l.errors[1], "races/elf.toml", "race_generation_below_lifespan", "race.elf",
                  "generation_years must be below lifespan_years");
    at_definition(l.errors[2], "races/elf.toml", "race_settlement_kinds_unique", "race.elf",
                  "settlement_kinds[1]: 'town' is already listed");
    at_definition(l.errors[3], "races/elf.toml", "race_site_weights_positive", "race.elf",
                  "the site_weights must not all be 0");
}

// --- 8.1 test 2: kind-level checks (P3-01 3) ---------------------------------------------

TEST_CASE("P3-01 3 race_order_unique: at each later definition; race_order_dense is then not reported") {
    {  // orc takes order 5: the orc (later than human) only. Both order-5 races
       // are compared with faerie (order 4) and pass.
        at_definition(only_error(load(shipped_edited("races/orc.toml", "order = 6", "order = 5"))), "races/orc.toml",
                      "race_order_unique", "race.orc", "order 5 is also used by 'race.human'");
    }
    {  // three share order 5: one error per later race, each naming the
       // earliest by id (goblin). Order 2 is now missing, so dwarf (order 3)
       // is compared with nothing; goblin (order 5, share 0.35) is compared
       // with faerie (order 4, 0.70) and fails. race_order_dense: not reported.
        Files f = shipped_edited("races/orc.toml", "order = 6", "order = 5");
        f = edited(f, "races/goblin.toml", "order = 2", "order = 5");
        const Load l = load(f);
        INFO(dump(l));
        REQUIRE(l.errors.size() == 3U);
        at_definition(l.errors[0], "races/goblin.toml", "race_awakening_order", "race.goblin",
                      "awakening_share must be above that of 'race.faerie' (order 4)");
        at_definition(l.errors[1], "races/human.toml", "race_order_unique", "race.human",
                      "order 5 is also used by 'race.goblin'");
        at_definition(l.errors[2], "races/orc.toml", "race_order_unique", "race.orc", "order 5 is also used by 'race.goblin'");
    }
    {  // two races of order 1: each is checked against 0 (goblin fails); order
       // 2 is missing, so dwarf is compared with nothing.
        const Load l = load(shipped_edited("races/goblin.toml", "order = 2", "order = 1"));
        INFO(dump(l));
        REQUIRE(l.errors.size() == 2U);
        at_definition(l.errors[0], "races/goblin.toml", "race_awakening_order", "race.goblin",
                      "the race of order 1 must have awakening_share 0");
        at_definition(l.errors[1], "races/goblin.toml", "race_order_unique", "race.goblin",
                      "order 1 is also used by 'race.elf'");
    }
    {  // the earliest by id names the duplicate even when it is not the
       // earlier file or the smaller share: dwarf takes order 2 beside goblin.
        at_definition(only_error(load(shipped_edited("races/dwarf.toml", "order = 3", "order = 2"))), "races/goblin.toml",
                      "race_order_unique", "race.goblin", "order 2 is also used by 'race.dwarf'");
    }
}

TEST_CASE("P3-01 3 race_awakening_order: a race of order k is compared with the earliest (by id) race of order k - 1") {
    // dwarf and faerie share order 4 (dwarf 0.55, faerie 0.70); human (order
    // 5) is compared with dwarf only.
    Files f = shipped_edited("races/dwarf.toml", "order = 3", "order = 4");
    {  // human 0.60: above dwarf's 0.55 though below faerie's 0.70: no awakening error
        const Load l = load(edited(f, "races/human.toml", "awakening_share = \"0.83\"", "awakening_share = \"0.60\""));
        at_definition(only_error(l), "races/faerie.toml", "race_order_unique", "race.faerie",
                      "order 4 is also used by 'race.dwarf'");
    }
    {  // human 0.50: below dwarf's
        const Load l = load(edited(f, "races/human.toml", "awakening_share = \"0.83\"", "awakening_share = \"0.50\""));
        INFO(dump(l));
        REQUIRE(l.errors.size() == 2U);
        at_definition(l.errors[0], "races/faerie.toml", "race_order_unique", "race.faerie",
                      "order 4 is also used by 'race.dwarf'");
        at_definition(l.errors[1], "races/human.toml", "race_awakening_order", "race.human",
                      "awakening_share must be above that of 'race.dwarf' (order 4)");
    }
    {  // with no race of order k - 1 there is nothing to compare with: without
       // the dwarves (order 3), faerie at 0.30 (below goblin) gives only the gap.
        Files g = history_data::shipped_files();
        g.erase("races/dwarf.toml");
        g.erase("cultures/dwarven.toml");
        history_data::prune_monster_files(g);   // P5-02 4.2: the giants' naming culture
        g = edited(g, "races/faerie.toml", "awakening_share = \"0.70\"", "awakening_share = \"0.30\"");
        at_directory(only_error(load(g)), "races", "race_order_dense", "race",
                     "orders must be 1 to 5 without gaps; 3 is missing");
    }
}

TEST_CASE("P3-01 3 race_order_dense: against the directory, naming the smallest missing order") {
    {  // without the dwarves (order 3) and their culture
        Files f = history_data::shipped_files();
        f.erase("races/dwarf.toml");
        f.erase("cultures/dwarven.toml");
        history_data::prune_monster_files(f);   // P5-02 4.2: the giants' naming culture
        at_directory(only_error(load(f)), "races", "race_order_dense", "race",
                     "orders must be 1 to 5 without gaps; 3 is missing");
    }
    {  // the goblins alone: n = 1, order 2
        Files f;
        f["races/goblin.toml"] = std::string(history_data::GOBLIN_TOML);
        f["cultures/goblin.toml"] = history_data::culture_toml(history_data::culture_listings()[3]);
        f["history/chronology.toml"] = std::string(history_data::CHRONOLOGY_TOML);
        f["history/engine.toml"] = std::string(history_data::ENGINE_TOML);
        f["history/peoples.toml"] = std::string(history_data::PEOPLES_TOML);
        f["history/conflict.toml"] = std::string(history_data::CONFLICT_TOML);
        f["history/report.toml"] = std::string(history_data::REPORT_TOML);
        f["history/renaming.toml"] = std::string(history_data::RENAMING_TOML);   // P5-12 4
        f["history/relations.toml"] = std::string(history_data::RELATIONS_TOML);   // P5-03 4.2
        f["history/alliances.toml"] = std::string(history_data::ALLIANCES_TOML);   // P5-04 4.2: a required fixed id
        history_data::add_monster_files(f);   // P5-02 4.1 to 4.3
        at_directory(only_error(load(f)), "races", "race_order_dense", "race",
                     "orders must be 1 to 1 without gaps; 1 is missing");
    }
    {  // two gaps: the smallest is named
        Files f = history_data::shipped_files();
        for (const char* r : {"races/goblin.toml", "races/faerie.toml"}) {
            f.erase(r);
        }
        f.erase("cultures/goblin.toml");
        f.erase("cultures/faerie.toml");
        history_data::prune_monster_files(f);   // P5-02 4.2: the trolls' and wyverns' naming culture
        at_directory(only_error(load(f)), "races", "race_order_dense", "race",
                     "orders must be 1 to 4 without gaps; 2 is missing");
    }
    {  // the elves alone are a complete set (cross-kind conditions are the stage's, P3-01 3)
        Files f;
        f["races/elf.toml"] = std::string(history_data::ELF_TOML);
        f["cultures/elven.toml"] = history_data::culture_toml(history_data::culture_listings()[1]);
        f["history/chronology.toml"] = std::string(history_data::CHRONOLOGY_TOML);
        f["history/engine.toml"] = std::string(history_data::ENGINE_TOML);
        f["history/peoples.toml"] = std::string(history_data::PEOPLES_TOML);
        f["history/conflict.toml"] = std::string(history_data::CONFLICT_TOML);
        f["history/report.toml"] = std::string(history_data::REPORT_TOML);
        f["history/renaming.toml"] = std::string(history_data::RENAMING_TOML);   // P5-12 4
        f["history/relations.toml"] = std::string(history_data::RELATIONS_TOML);   // P5-03 4.2
        f["history/alliances.toml"] = std::string(history_data::ALLIANCES_TOML);   // P5-04 4.2: a required fixed id
        history_data::add_monster_files(f);   // P5-02 4.1 to 4.3
        const Load l = load(f);
        INFO(dump(l));
        CHECK(l.ok());
    }
}

TEST_CASE("P3-01 3 race_awakening_order: order 1 at 0, shares strictly increasing with order") {
    at_definition(only_error(load(shipped_edited("races/elf.toml", "awakening_share = 0", "awakening_share = \"0.01\""))),
                  "races/elf.toml", "race_awakening_order", "race.elf", "the race of order 1 must have awakening_share 0");
    // Equal to the previous order's share.
    at_definition(only_error(load(shipped_edited("races/dwarf.toml", "awakening_share = \"0.55\"", "awakening_share = \"0.35\""))),
                  "races/dwarf.toml", "race_awakening_order", "race.dwarf",
                  "awakening_share must be above that of 'race.goblin' (order 2)");
    // Below it.
    at_definition(only_error(load(shipped_edited("races/orc.toml", "awakening_share = \"0.93\"", "awakening_share = \"0.5\""))),
                  "races/orc.toml", "race_awakening_order", "race.orc",
                  "awakening_share must be above that of 'race.human' (order 5)");
    // The order, not the id, decides: faerie (order 4) below dwarf (order 3).
    at_definition(only_error(load(shipped_edited("races/faerie.toml", "awakening_share = \"0.70\"", "awakening_share = \"0.55\""))),
                  "races/faerie.toml", "race_awakening_order", "race.faerie",
                  "awakening_share must be above that of 'race.dwarf' (order 3)");
    // One raw step above is enough; the last may be 1.
    CHECK(load(shipped_edited("races/dwarf.toml", "awakening_share = \"0.55\"", "awakening_share = \"0.350000001\"")).ok());
    CHECK(load(shipped_edited("races/orc.toml", "awakening_share = \"0.93\"", "awakening_share = 1")).ok());
}

TEST_CASE("P3-01 3 (revised): a field error leaves a race out of the kind-level checks, so its order counts as missing") {
    // goblin (order 2) has an unknown key: race_order_dense reports 2 as
    // missing, and dwarf (order 3, share lowered below goblin's) is compared
    // with nothing.
    Files f = shipped_edited("races/goblin.toml", "upland = 100\n", "upland = 100\ntemperament = 3\n");
    f = edited(f, "races/dwarf.toml", "awakening_share = \"0.55\"", "awakening_share = \"0.30\"");
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE(l.errors.size() == 2U);
    at_directory(l.errors[0], "races", "race_order_dense", "race", "orders must be 1 to 5 without gaps; 2 is missing");
    CHECK(l.errors[1].path == "races/goblin.toml");
    CHECK(contains(l.errors[1].message, "race.goblin.temperament"));
}

TEST_CASE("P3-01 3 and P0-03 amendment 16: kind-level checks see only definitions that passed type validation") {
    // The orc definition fails type validation (unknown key): the remaining
    // five have orders 1 to 5, so no kind-level check fails.
    const Load l = load(shipped_edited("races/orc.toml", "upland = 100\n", "upland = 100\ntemperament = 3\n"));
    INFO(dump(l));
    REQUIRE(l.errors.size() == 1U);
    CHECK(l.errors[0].path == "races/orc.toml");
    CHECK(contains(l.errors[0].message, "race.orc.temperament"));  // P3-01 3: reserved for P5, not declared
}

// --- 8.1 test 3: the word grammar -------------------------------------------------------

TEST_CASE("P3-01 3, 4 and P0-03 amendment 17: name, plural, adjective and the culture name are words") {
    struct Case {
        const char* path;
        const char* from;
        const char* key;
        std::uint32_t line;
    };
    const std::vector<Case> cases = {
        // On orc, the race of the highest order: a definition that fails
        // type validation is left out of the kind-level checks (P0-03
        // amendment 16), and without the orcs the orders are still 1 to 5.
        {"races/orc.toml", "name = \"Orc\"", "race.orc.name", 3},
        {"races/orc.toml", "plural = \"Orcs\"", "race.orc.plural", 4},
        {"races/orc.toml", "adjective = \"Orcish\"", "race.orc.adjective", 5},
        {"cultures/elven.toml", "name = \"Elven\"", "culture.elven.name", 3},
    };
    for (const Case& c : cases) {
        const std::string field(std::string_view(c.from).substr(0, std::string_view(c.from).find(' ')));
        for (const char* bad : {"elf", "High Elf", "ELF", "Elf2", "Elf-kin", ""}) {
            CAPTURE(std::string_view(c.key));
            CAPTURE(std::string_view(bad));
            const Load l = load(shipped_edited(c.path, c.from, field + " = \"" + bad + "\""));
            value_error(l, c.path, c.line, static_cast<std::uint32_t>(field.size() + 4),
                        "'" + std::string(c.key) + "' is \"" + bad +
                            "\"; expected a string of at most 32 bytes, a word ([A-Z][a-z]*)");
        }
        // 32 bytes is a word; 33 bytes fail the length rule first.
        CHECK(load(shipped_edited(c.path, c.from, field + " = \"E" + std::string(31, 'l') + "\"")).ok());
        const Load l33 = load(shipped_edited(c.path, c.from, field + " = \"E" + std::string(32, 'l') + "\""));
        const wd::LoadError e = only_error(l33);
        CHECK(contains(e.message, "'" + std::string(c.key) + "' is 33 bytes long"));
    }
}

// --- 8.1 test 4: fixed ids and required directories --------------------------------------

TEST_CASE("P3-01 5: a history file for an unregistered id is an error at its header") {
    Files f = history_data::shipped_files();
    // P3-06 12.1 (P3-01 r7): conflict is registered from 0.16.0; P5-02 4.3 registers monsters (0.23.0); a future
    // id (deities) is not.
    f["history/deities.toml"] = "[history.deities]\nstep_years = 1\n";
    const wd::LoadError e = only_error(load(f));
    CHECK(e.path == "history/deities.toml");
    CHECK(e.line == 1U);
    CHECK(e.column == 1U);
    // P3-01 5 (revised): the registered ids, each in single quotes.
    CHECK(e.message ==
          "unknown id 'deities' for kind 'history'; expected one of 'alliances', 'chronology', 'conflict', 'engine', 'monsters', "
          "'peoples', 'relations', 'renaming', 'report'");   // relations: P5-03 4.2
}

TEST_CASE("P3-01 5: history/ without chronology.toml is a missing definition against the directory") {
    Files f = history_data::shipped_files();
    f.erase("history/chronology.toml");
    f["history/notes.md"] = "not data\n";  // the directory exists (P0-03 7.2: ignored)
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    bool found = false;
    for (const wd::LoadError& e : l.errors) {
        CHECK(e.path == "history");
        CHECK(e.line == 0U);
        found = found || e.to_string() == "history: error: missing definition 'history.chronology'";
    }
    CHECK(found);
}

TEST_CASE("P3-01 3-5: races/, cultures/ and history/ are required kind directories") {
    for (const char* d : {"races", "cultures", "history"}) {
        CAPTURE(std::string_view(d));
        Files f;
        for (const auto& [path, text] : history_data::shipped_files()) {
            if (path.substr(0, path.find('/')) != d) {
                f[path] = text;
            }
        }
        const Load l = load(f);
        INFO(dump(l));
        REQUIRE_FALSE(l.ok());
        // The missing directory is reported against it with no position
        // (P0-03 7.1, 7.5; the loader's wording "the directory of required
        // kind '<kind>' is missing"). Removing races/ also leaves the
        // cultures' race refs unresolved, which may be reported as well.
        bool found = false;
        for (const wd::LoadError& e : l.errors) {
            if (e.path == d && e.line == 0U) {
                found = found || contains(e.message, "required kind '" + std::string(history_data::kind_of_directory(d)) + "'");
            }
        }
        CHECK(found);
    }
    // An empty races/ directory: the required kind has no definitions, not
    // a kind-level check (P0-03 amendment 16: not run when no definition passed).
    Files f = history_data::shipped_files();
    for (const char* r : {"races/dwarf.toml", "races/elf.toml", "races/faerie.toml", "races/goblin.toml",
                          "races/human.toml", "races/orc.toml"}) {
        f.erase(r);
    }
    f["races/README.md"] = "x\n";
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    CHECK(errors_of_check(l, "race_order_dense").empty());
    CHECK(errors_of_check(l, "race_order_unique").empty());
}

TEST_CASE("P3-01 3, 4: references to biome, culture and race resolve or are errors") {
    {
        // On orc (order 6), in case an unresolved reference also keeps the
        // definition out of the kind-level checks.
        const Load l = load(shipped_edited("races/orc.toml", "culture = \"orcish\"", "culture = \"sylvan\""));
        const wd::LoadError e = only_error(l);
        CHECK(e.path == "races/orc.toml");
        CHECK(contains(e.message, "race.orc.culture"));
        CHECK(contains(e.message, "sylvan"));
    }
    {
        const Load l = load(shipped_edited("races/orc.toml", "{ biome = \"grassland\", weight = 200 }",
                                           "{ biome = \"meadow\", weight = 200 }"));
        const wd::LoadError e = only_error(l);
        CHECK(contains(e.message, "race.orc.biomes[2].biome"));
        CHECK(contains(e.message, "meadow"));
    }
    {
        const Load l = load(shipped_edited("cultures/elven.toml", "race = \"elf\"", "race = \"elves\""));
        const wd::LoadError e = only_error(l);
        CHECK(e.path == "cultures/elven.toml");
        CHECK(contains(e.message, "culture.elven.race"));
    }
    // A water biome or another race's culture is the stage's concern
    // (P3-01 3, cross-kind conditions), not the loader's.
    CHECK(load(shipped_edited("races/elf.toml", "{ biome = \"grassland\", weight = 0 }", "{ biome = \"ocean\", weight = 0 }")).ok());
    CHECK(load(shipped_edited("races/elf.toml", "culture = \"elven\"", "culture = \"orcish\"")).ok());
}

// --- 8.1 test 5: ranges and types ----------------------------------------------------------

TEST_CASE("P3-01 8.1 test 5: every bounded race field one step outside its bounds is a range error") {
    struct Edit {
        const char* path;
        const char* from;
        const char* ok;
        const char* bad;
        const char* key;
    };
    // Every edit is on orc (order 6): a definition that fails type
    // validation is left out of the kind-level checks (P0-03 amendment 16),
    // and the other five still have the orders 1 to 5, so the range error is
    // the only error.
    const std::vector<Edit> edits = {
        {"races/orc.toml", "order = 6", "order = 6", "order = 0", "race.orc.order"},
        {"races/orc.toml", "order = 6", "order = 6", "order = 7", "race.orc.order"},
        {"races/orc.toml", "lifespan_years = 50", "lifespan_years = 10000", "lifespan_years = 10001", "race.orc.lifespan_years"},
        {"races/orc.toml", "lifespan_years = 50", "lifespan_years = 19", "lifespan_years = 0", "race.orc.lifespan_years"},
        {"races/orc.toml", "generation_years = 18", "generation_years = 1", "generation_years = 0", "race.orc.generation_years"},
        {"races/orc.toml", "generation_years = 18", "generation_years = 49", "generation_years = 1001", "race.orc.generation_years"},
        {"races/orc.toml", "growth_permille = 18", "growth_permille = 0", "growth_permille = -1", "race.orc.growth_permille"},
        {"races/orc.toml", "growth_permille = 18", "growth_permille = 1000", "growth_permille = 1001", "race.orc.growth_permille"},
        // P3-05 12.2 item 1: density_permille int(1, 1000), required
        {"races/orc.toml", "density_permille = 800", "density_permille = 1", "density_permille = 0", "race.orc.density_permille"},
        {"races/orc.toml", "density_permille = 800", "density_permille = 1000", "density_permille = 1001", "race.orc.density_permille"},
        {"races/orc.toml", "fertility = 35", "fertility = 1000", "fertility = 1001", "race.orc.site_weights.fertility"},
        {"races/orc.toml", "fertility = 35", "fertility = 0", "fertility = -1", "race.orc.site_weights.fertility"},
        {"races/orc.toml", "forage = 25", "forage = 1000", "forage = 1001", "race.orc.site_weights.forage"},
        {"races/orc.toml", "timber = 10", "timber = 0", "timber = -1", "race.orc.site_weights.timber"},
        {"races/orc.toml", "ore = 30", "ore = 1000", "ore = 1001", "race.orc.site_weights.ore"},
        {"races/orc.toml", "fresh_water = 0", "fresh_water = 1000", "fresh_water = -1", "race.orc.site_weights.fresh_water"},
        {"races/orc.toml", "weight = 400", "weight = 1000", "weight = 1001", "race.orc.biomes[0].weight"},
        {"races/orc.toml", "weight = -300", "weight = -1000", "weight = -1001", "race.orc.biomes[8].weight"},
        {"races/orc.toml", "upland = 100", "upland = -1000", "upland = -1001", "race.orc.upland"},
        {"races/orc.toml", "upland = 100", "upland = 1000", "upland = 1001", "race.orc.upland"},
        {"races/orc.toml", "coast = -100", "coast = -1000", "coast = -1001", "race.orc.coast"},
        {"races/orc.toml", "coast = -100", "coast = 1000", "coast = 1001", "race.orc.coast"},
        {"races/orc.toml", "expansion = 600", "expansion = 0", "expansion = -1", "race.orc.expansion"},
        {"races/orc.toml", "expansion = 600", "expansion = 1000", "expansion = 1001", "race.orc.expansion"},
        {"races/orc.toml", "aggression = 900", "aggression = 0", "aggression = -1", "race.orc.aggression"},
        {"races/orc.toml", "aggression = 900", "aggression = 1000", "aggression = 1001", "race.orc.aggression"},
        // P3-01 r8 (1) (generator 0.17.0): cell_weights {ore, height}, each int(0, 1000); r11 3.4 (0.19.0):
        // inland, port, habitability, each int(0, 1000)
        {"races/orc.toml", "ore = 0, height = 0,", "ore = 1000, height = 0,", "ore = 1001, height = 0,",
         "race.orc.cell_weights.ore"},
        {"races/orc.toml", "ore = 0, height = 0,", "ore = 0, height = 1000,", "ore = 0, height = -1,",
         "race.orc.cell_weights.height"},
        {"races/orc.toml", "inland = 0,", "inland = 1000,", "inland = 1001,", "race.orc.cell_weights.inland"},
        {"races/orc.toml", "inland = 0,", "inland = 0,", "inland = -1,", "race.orc.cell_weights.inland"},
        {"races/orc.toml", "port = 1000,", "port = 0,", "port = -1,", "race.orc.cell_weights.port"},
        {"races/orc.toml", "port = 1000,", "port = 1000,", "port = 1001,", "race.orc.cell_weights.port"},
        {"races/orc.toml", "habitability = 1000 }", "habitability = 0 }", "habitability = -1 }",
         "race.orc.cell_weights.habitability"},
        {"races/orc.toml", "habitability = 1000 }", "habitability = 1000 }", "habitability = 1001 }",
         "race.orc.cell_weights.habitability"},
        // P3-01 r11 3.5 (0.19.0): odds_min_permille int(0, 1000), awakening_quarrel int(0, 1)
        {"races/orc.toml", "odds_min_permille = 500", "odds_min_permille = 0", "odds_min_permille = -1",
         "race.orc.odds_min_permille"},
        {"races/orc.toml", "odds_min_permille = 500", "odds_min_permille = 1000", "odds_min_permille = 1001",
         "race.orc.odds_min_permille"},
        {"races/orc.toml", "awakening_quarrel = 1", "awakening_quarrel = 0", "awakening_quarrel = -1",
         "race.orc.awakening_quarrel"},
        {"races/orc.toml", "awakening_quarrel = 1", "awakening_quarrel = 1", "awakening_quarrel = 2",
         "race.orc.awakening_quarrel"},
        // P3-01 r13 3.6 (P3-10 second round): hidden_years int(0, 10000)
        {"races/orc.toml", "hidden_years = 100", "hidden_years = 0", "hidden_years = -1", "race.orc.hidden_years"},
        {"races/orc.toml", "hidden_years = 100", "hidden_years = 10000", "hidden_years = 10001",
         "race.orc.hidden_years"},
        // P3-01 r14 3.7 (P3-11, generator 0.20.0): awakening_population int(0, 100000)
        {"races/orc.toml", "awakening_population = 1200", "awakening_population = 0", "awakening_population = -1",
         "race.orc.awakening_population"},
        {"races/orc.toml", "awakening_population = 1200", "awakening_population = 100000",
         "awakening_population = 100001", "race.orc.awakening_population"},
        // P3-06 12.2 item 1 (P3-01 r7): prowess_permille int(1, 4000)
        {"races/orc.toml", "prowess_permille = 1300", "prowess_permille = 1", "prowess_permille = 0", "race.orc.prowess_permille"},
        {"races/orc.toml", "prowess_permille = 1300", "prowess_permille = 4000", "prowess_permille = 4001", "race.orc.prowess_permille"},
        // P3-01 r9 3.2 (generator 0.18.0): muster_permille int(1, 10000), conceal_min int(0, 1001),
        // home_defense_permille int(0, 10000)
        {"races/orc.toml", "muster_permille = 4000", "muster_permille = 1", "muster_permille = 0", "race.orc.muster_permille"},
        {"races/orc.toml", "muster_permille = 4000", "muster_permille = 10000", "muster_permille = 10001",
         "race.orc.muster_permille"},
        {"races/orc.toml", "conceal_min = 1001", "conceal_min = 0", "conceal_min = -1", "race.orc.conceal_min"},
        {"races/orc.toml", "conceal_min = 1001", "conceal_min = 1001", "conceal_min = 1002", "race.orc.conceal_min"},
        {"races/orc.toml", "home_defense_permille = 0", "home_defense_permille = 0", "home_defense_permille = -1",
         "race.orc.home_defense_permille"},
        {"races/orc.toml", "home_defense_permille = 0", "home_defense_permille = 10000", "home_defense_permille = 10001",
         "race.orc.home_defense_permille"},
        // fx(0, 1): one raw step is below the decimal resolution, so the
        // nearest decimals outside are used (raw ONE + 4 and -4). Below 0
        // the awakening check would fail too, but the definition is not
        // type-valid, so only the range error is reported.
        {"races/orc.toml", "awakening_share = \"0.93\"", "awakening_share = 1", "awakening_share = \"1.000000001\"",
         "race.orc.awakening_share"},
        {"races/orc.toml", "awakening_share = \"0.93\"", "awakening_share = \"0.93\"", "awakening_share = \"-0.000000001\"",
         "race.orc.awakening_share"},
        {"races/orc.toml", "camp = \"evil\"", "camp = \"neutral\"", "camp = \"chaotic\"", "race.orc.camp"},
        {"races/orc.toml", "\"fortress\"]", "\"city\", \"fortress\"]", "\"citadel\"]", "race.orc.settlement_kinds[3]"},
        {"races/orc.toml", "name = \"Orc\"", "name = \"Orc\"", "name = 3", "race.orc.name"},
    };
    for (const Edit& e : edits) {
        CAPTURE(std::string_view(e.bad));
        const Load good = load(shipped_edited(e.path, e.from, e.ok));
        CHECK_MESSAGE(good.ok(), dump(good));
        const Load bad = load(shipped_edited(e.path, e.from, e.bad));
        const wd::LoadError err = only_error(bad);
        CHECK(err.path == e.path);
        CHECK(contains(err.message, "'" + std::string(e.key) + "'"));
        CHECK(contains(err.message, "expected"));
    }
    // Integer fields take integers only; fx fields reject TOML floats (P0-03 4).
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"lifespan_years = 50", "lifespan_years = \"50\""},
             {"lifespan_years = 50", "lifespan_years = 50.0"},
             {"awakening_share = \"0.93\"", "awakening_share = 0.93"},
             {"camp = \"evil\"", "camp = 1"}}) {
        CAPTURE(to);
        const wd::LoadError e = only_error(load(shipped_edited("races/orc.toml", from, to)));
        CHECK(e.path == "races/orc.toml");
    }
    CHECK(contains(dump(load(shipped_edited("races/orc.toml", "awakening_share = \"0.93\"", "awakening_share = 0.93"))),
                   "floating-point"));
}

TEST_CASE("P3-01 3: array lengths of biomes (0 to 16) and settlement_kinds (1 to 5)") {
    // biomes: 0 and 16 load, 17 do not. On orc (order 6), so that a type
    // error leaves orders 1 to 5 for the kind-level checks.
    const std::string orc(history_data::ORC_TOML);
    const std::size_t b0 = orc.find("biomes = [");
    const std::size_t b1 = orc.find("]\n", b0) + 2;
    const auto with_biomes = [&](const std::string& list) {
        Files f = history_data::shipped_files();
        f["races/orc.toml"] = orc.substr(0, b0) + "biomes = [" + list + "]\n" + orc.substr(b1);
        return f;
    };
    CHECK(load(with_biomes("")).ok());
    std::vector<std::string> ids = {"alpine", "blighted_waste", "fen", "grassland", "heath", "highland_moor",
                                    "lake", "marsh", "ocean", "steppe", "temperate_forest", "wet_woodland"};
    // 16 distinct entries are impossible with 12 biomes, so 16 and 17 use
    // repeats: 16 is a race_biomes_unique failure (types valid), 17 a length error.
    std::string sixteen, seventeen;
    for (int i = 0; i < 17; ++i) {
        const std::string item = "{ biome = \"" + ids[static_cast<std::size_t>(i) % ids.size()] + "\", weight = 1 }, ";
        if (i < 16) {
            sixteen += item;
        }
        seventeen += item;
    }
    const Load l16 = load(with_biomes(sixteen));
    INFO(dump(l16));
    REQUIRE_FALSE(l16.ok());
    for (const wd::LoadError& e : l16.errors) {
        CHECK(contains(e.message, "check 'race_biomes_unique'"));
    }
    const wd::LoadError e17 = only_error(load(with_biomes(seventeen)));
    CHECK(contains(e17.message, "'race.orc.biomes'"));
    CHECK(contains(e17.message, "17"));
    // settlement_kinds: [] fails, five load, six (necessarily a repeat) fail on the length.
    const std::string kinds = "[\"hamlet\", \"village\", \"town\", \"fortress\"]";
    CHECK(contains(only_error(load(shipped_edited("races/orc.toml", kinds, "[]"))).message, "'race.orc.settlement_kinds'"));
    CHECK(load(shipped_edited("races/orc.toml", kinds, "[\"hamlet\", \"village\", \"town\", \"city\", \"fortress\"]")).ok());
    const wd::LoadError e6 =
        only_error(load(shipped_edited("races/orc.toml", kinds, "[\"hamlet\", \"village\", \"town\", \"city\", \"fortress\", \"town\"]")));
    CHECK(contains(e6.message, "'race.orc.settlement_kinds'"));
    CHECK_FALSE(contains(e6.message, "check '"));
}

TEST_CASE("P3-01 3: every race field is required and no other key is accepted") {
    for (const std::string_view field : history_data::race_fields()) {
        CAPTURE(std::string(field));
        // Remove the line (or the block) that starts with "<field> = ".
        std::string text(history_data::ORC_TOML);  // order 6: see the range test
        const std::size_t at = text.find("\n" + std::string(field) + " = ") + 1;
        REQUIRE(at != 0U);
        std::size_t end = text.find('\n', at) + 1;
        if (field == "biomes") {
            end = text.find("]\n", at) + 2;
        }
        text.erase(at, end - at);
        Files f = history_data::shipped_files();
        f["races/orc.toml"] = text;
        const wd::LoadError e = only_error(load(f));
        CHECK(e.path == "races/orc.toml");
        CHECK(e.line == 1U);  // at the definition's header (P0-03 7.5)
        CHECK(e.message == "missing required field 'race.orc." + std::string(field) + "'");
    }
    // A missing site weight or biome field: at the inline table.
    CHECK_FALSE(load(shipped_edited("races/human.toml", ", fresh_water = 15 }", " }")).ok());
    CHECK_FALSE(load(shipped_edited("races/human.toml", "{ biome = \"steppe\", weight = 100 }", "{ biome = \"steppe\" }")).ok());
    CHECK_FALSE(load(shipped_edited("races/human.toml", "fertility = 60,", "fertility = 60, magic = 1,")).ok());
    // P3-01 r11 3.4 (generator 0.19.0): each of the three new cell weights is required.
    for (const char* part : {", inland = 0", ", port = 1000", ", habitability = 1000"}) {
        CAPTURE(std::string_view(part));
        const Load l = load(shipped_edited("races/human.toml", part, ""));
        CHECK_FALSE(l.ok());
        // the field error (and the kind-level race_order_dense that follows from leaving human out, P3-01 3 r3)
        bool named = false;
        for (const auto& e : l.errors) named = named || contains(e.message, "race.human.cell_weights");
        CHECK(named);
    }
}

TEST_CASE("P3-04 r13 4.2, P5-03a: report_event.template is string(1024) from 0.25.0: 1024 bytes load, 1025 are refused") {
    // string(512) from 0.19.0; string(1024) from 0.25.0 (lead's ruling at P5-03a: the cause-list templates of
    // kinds 58 and 59 with the words of P5-03a 3.5 and 7.3 are longer than 512 bytes).
    const std::string old = "{creator} awakened the {race:plural} in {@region}.";
    REQUIRE(contains(history_data::REPORT_TOML, old));
    // plain text after the placeholders keeps the template valid (8.4); only the length changes
    const std::string at1024 = old + std::string(1024 - old.size(), 'x');
    REQUIRE(at1024.size() == 1024U);
    CHECK_MESSAGE(load(shipped_edited("history/report.toml", old, at1024)).ok(), "1024 bytes");
    const Load l = load(shipped_edited("history/report.toml", old, at1024 + "x"));
    const wd::LoadError e = only_error(l);
    CHECK(e.path == "history/report.toml");
    CHECK(contains(e.message, "template"));
    // the shipped cause-list templates of kinds 16 and 31 exceed the bound of 256 before 0.19.0 (P3-05 r12 22.3);
    // those of kinds 58 and 59 the bound of 512 before 0.25.0
    const std::string t(history_data::REPORT_TOML);
    const auto length_of = [&](const std::string& k) {
        const std::size_t at = t.find("kind = \"" + k + "\"");
        REQUIRE(at != std::string::npos);
        const std::size_t from = t.find("template = \"", at) + 12;
        const std::size_t to = t.find("\" }", from);
        return to - from;
    };
    for (const char* k : {"settlement_abandoned", "settlement_abandoned_to"}) {
        CHECK(length_of(k) > 256U);
        CHECK(length_of(k) <= 1024U);
    }
    for (const char* k : {"disposition_turned", "refugees_arrived"}) {
        CHECK(length_of(k) > 512U);
        CHECK(length_of(k) <= 1024U);
    }
}

TEST_CASE("P3-01 r11 3.5, P3-06 r14 21.9: history.conflict.odds_min is refused as an unknown field from 0.19.0") {
    const Load l = load(shipped_edited("history/conflict.toml", "search_years = 3\n", "search_years = 3\nodds_min = 700\n"));
    const wd::LoadError e = only_error(l);
    CHECK(e.path == "history/conflict.toml");
    CHECK(contains(e.message, "odds_min"));
    // and the shipped listing has no odds_min, and every race an odds_min_permille in int(0, 1000)
    CHECK_FALSE(contains(history_data::CONFLICT_TOML, "odds_min"));
    for (const history_data::Race& r : history_data::races()) {
        CAPTURE(std::string(r.id));
        CHECK(r.odds_min_permille == (r.id == "orc" ? 500 : 700));
        CHECK(r.awakening_quarrel == (r.id == "orc" ? 1 : 0));
        const bool neutral = r.cell_ore == 0 && r.cell_height == 0 && r.cell_inland == 0 && r.cell_port == 1000 &&
                             r.cell_habitability == 1000;
        CHECK(neutral == (r.id != "dwarf" && r.id != "faerie"));  // 3.4: the races with a cell term
    }
}

TEST_CASE("P3-01 r13 3.6, P3-06 r17 22.2: history.conflict.hidden_years is refused as an unknown field") {
    const Load l = load(shipped_edited("history/conflict.toml", "quarrel_min_settlements = 2\n",
                                       "quarrel_min_settlements = 2\nhidden_years = 300\n"));
    const wd::LoadError e = only_error(l);
    CHECK(e.path == "history/conflict.toml");
    CHECK(contains(e.message, "hidden_years"));
    CHECK_FALSE(contains(history_data::CONFLICT_TOML, "hidden_years"));
    for (const history_data::Race& r : history_data::races()) {
        CAPTURE(std::string(r.id));
        CHECK(r.hidden_years == (r.id == "orc" ? 100 : 300));
    }
}

TEST_CASE("P3-06 r17 22.1 item 4, 22.7: quarrel_min_settlements int(0, 1000) after quarrel_strain; conflict_quarrel_strain_max") {
    const std::string path = "history/conflict.toml";
    CHECK(load(shipped_edited(path, "quarrel_min_settlements = 2", "quarrel_min_settlements = 0")).ok());
    CHECK(load(shipped_edited(path, "quarrel_min_settlements = 2", "quarrel_min_settlements = 1000")).ok());
    for (const char* bad : {"quarrel_min_settlements = -1", "quarrel_min_settlements = 1001"}) {
        CAPTURE(std::string_view(bad));
        const wd::LoadError e = only_error(load(shipped_edited(path, "quarrel_min_settlements = 2", bad)));
        CHECK(e.path == path);
        CHECK(contains(e.message, "'history.conflict.quarrel_min_settlements'"));
    }
    // required: a listing without it is refused
    const wd::LoadError miss = only_error(load(shipped_edited(path, "quarrel_min_settlements = 2\n", "")));
    CHECK(miss.message == "missing required field 'history.conflict.quarrel_min_settlements'");
    // strain_max 500 with quarrel_strain 600 (and claim, declare fields that pass the other checks): fails
    // conflict_quarrel_strain_max with 22.7's message; 600 and 600 passes
    Files f = history_data::shipped_files();
    std::string t(history_data::CONFLICT_TOML);
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"strain_max = 5000", "strain_max = 500"},
             {"claim_strain = 300", "claim_strain = 100"},
             {"declare_min = 300", "declare_min = 100"},
             {"declare_span = 600", "declare_span = 400"}}) {
        const std::size_t at = t.find(from);
        REQUIRE(at != std::string::npos);
        t.replace(at, from.size(), to);
    }
    f[path] = t;
    const wd::LoadError e = only_error(load(f));
    CHECK(e.path == path);
    CHECK(contains(e.message,
                   "check 'conflict_quarrel_strain_max' failed for 'history.conflict': quarrel_strain must not be above strain_max"));
    const std::size_t at = t.find("strain_max = 500");
    t.replace(at, std::string_view("strain_max = 500").size(), "strain_max = 600");
    const std::size_t at2 = t.find("declare_span = 400");
    t.replace(at2, std::string_view("declare_span = 400").size(), "declare_span = 500");
    f[path] = t;
    CHECK(load(f).ok());
}

TEST_CASE("P3-04 r15 4.1: quiet_min_years int(0, 400) after quiet_events_max, required; engine_quiet_min") {
    const std::string path = "history/engine.toml";
    REQUIRE(contains(history_data::ENGINE_TOML, "quiet_events_max = 3\nquiet_min_years = 50\n"));
    CHECK(load(shipped_edited(path, "quiet_min_years = 50", "quiet_min_years = 0")).ok());
    for (const char* bad : {"quiet_min_years = -1", "quiet_min_years = 401"}) {
        CAPTURE(std::string_view(bad));
        const wd::LoadError e = only_error(load(shipped_edited(path, "quiet_min_years = 50", bad)));
        CHECK(contains(e.message, "'history.engine.quiet_min_years'"));
    }
    const wd::LoadError miss = only_error(load(shipped_edited(path, "quiet_min_years = 50\n", "")));
    CHECK(miss.message == "missing required field 'history.engine.quiet_min_years'");
    const wd::LoadError e = only_error(load(shipped_edited(path, "quiet_min_years = 50", "quiet_min_years = 51")));
    CHECK(contains(e.message, "check 'engine_quiet_min' failed for 'history.engine': quiet_min_years must not be above 2 x "
                              "quiet_step_years"));
    CHECK(load(shipped_edited(path, "quiet_step_years = 25", "quiet_step_years = 26")).ok());  // 50 <= 52
}

TEST_CASE("P3-01 r15 3.8, P3-06 r22 24.2, 24.5: quarrel_strength_permille int(0, 1000) and toll_spread_permille int(0, 500) after quarrel_min_settlements, required") {
    const std::string path = "history/conflict.toml";
    REQUIRE(contains(history_data::CONFLICT_TOML,
                     "quarrel_min_settlements = 2\nquarrel_strength_permille = 1000\ntoll_spread_permille = 200\n"
                     "capital_sacked_salience"));
    for (const char* ok : {"quarrel_strength_permille = 0", "quarrel_strength_permille = 1000"}) {
        CAPTURE(std::string_view(ok));
        CHECK(load(shipped_edited(path, "quarrel_strength_permille = 1000", ok)).ok());
    }
    for (const char* bad : {"quarrel_strength_permille = -1", "quarrel_strength_permille = 1001"}) {
        CAPTURE(std::string_view(bad));
        const wd::LoadError e = only_error(load(shipped_edited(path, "quarrel_strength_permille = 1000", bad)));
        CHECK(e.path == path);
        CHECK(contains(e.message, "'history.conflict.quarrel_strength_permille'"));
    }
    CHECK(load(shipped_edited(path, "toll_spread_permille = 200", "toll_spread_permille = 0")).ok());
    for (const char* bad : {"toll_spread_permille = -1", "toll_spread_permille = 501"}) {
        CAPTURE(std::string_view(bad));
        const wd::LoadError e = only_error(load(shipped_edited(path, "toll_spread_permille = 200", bad)));
        CHECK(contains(e.message, "'history.conflict.toll_spread_permille'"));
    }
    const wd::LoadError m1 = only_error(load(shipped_edited(path, "quarrel_strength_permille = 1000\n", "")));
    CHECK(m1.message == "missing required field 'history.conflict.quarrel_strength_permille'");
    const wd::LoadError m2 = only_error(load(shipped_edited(path, "toll_spread_permille = 200\n", "")));
    CHECK(m2.message == "missing required field 'history.conflict.toll_spread_permille'");
}

TEST_CASE("P3-01 r15 3.8, P3-06 r22 24.5: conflict_toll_spread keeps the victors' toll below the defeated's") {
    const std::string path = "history/conflict.toml";
    // shipped: 80 x 1200 = 96,000 <= min(250, 500) x 800 = 200,000
    CHECK(load(history_data::shipped_files()).ok());
    // loss_winner 200, loss_loser 250, spread 200: 240,000 > 200,000
    Files f = history_data::shipped_files();
    std::string t(history_data::CONFLICT_TOML);
    const std::size_t at = t.find("loss_winner_permille = 80");
    REQUIRE(at != std::string::npos);
    t.replace(at, std::string_view("loss_winner_permille = 80").size(), "loss_winner_permille = 200");
    f[path] = t;
    const wd::LoadError e = only_error(load(f));
    CHECK(e.path == path);
    CHECK(contains(e.message, "check 'conflict_toll_spread' failed for 'history.conflict': toll_spread_permille lets "
                              "the victors lose more than the defeated"));
    // the same with spread 0 passes (200 x 1000 <= 250 x 1000)
    const std::size_t at2 = t.find("toll_spread_permille = 200");
    t.replace(at2, std::string_view("toll_spread_permille = 200").size(), "toll_spread_permille = 0");
    f[path] = t;
    CHECK(load(f).ok());
    // the bound reads min(loss_loser, kill_max): kill_max 100 with the shipped spread fails (96,000 > 80,000)
    CHECK(contains(only_error(load(shipped_edited(path, "kill_max_permille = 500", "kill_max_permille = 100"))).message,
                   "check 'conflict_toll_spread' failed"));
}

TEST_CASE("P3-01 r15 3.8, P3-04 r19 22.3: flare_min_years int(1, 1000) after quiet_min_years, required; engine_flare_min") {
    const std::string path = "history/engine.toml";
    REQUIRE(contains(history_data::ENGINE_TOML, "quiet_min_years = 50\nflare_min_years = 10\nsimulated_min_permille"));
    CHECK(load(shipped_edited(path, "flare_min_years = 10", "flare_min_years = 1")).ok());
    CHECK(load(shipped_edited(path, "flare_min_years = 10", "flare_min_years = 30")).ok());  // = sparse flare_years
    for (const char* bad : {"flare_min_years = 0", "flare_min_years = 1001"}) {
        CAPTURE(std::string_view(bad));
        const wd::LoadError e = only_error(load(shipped_edited(path, "flare_min_years = 10", bad)));
        CHECK(contains(e.message, "'history.engine.flare_min_years'"));
    }
    const wd::LoadError miss = only_error(load(shipped_edited(path, "flare_min_years = 10\n", "")));
    CHECK(miss.message == "missing required field 'history.engine.flare_min_years'");
    const wd::LoadError e = only_error(load(shipped_edited(path, "flare_min_years = 10", "flare_min_years = 31")));
    CHECK(contains(e.message, "check 'engine_flare_min' failed for 'history.engine': sparse: flare_min_years must not "
                              "be above flare_years"));
}

TEST_CASE("P3-02 r17 15.2, P3-05 r18 24, P3-06 r22 24.6: the dwarven forbidden list, became_city, kinds 41 and 42") {
    REQUIRE(contains(history_data::DWARVEN_NAMING, "\"ndb\", \"rnk\", \"didin\", \"ririn\"]"));
    REQUIRE(contains(history_data::REPORT_TOML,
                     "{ kind = \"became_city\", salience = 45, template = \"{settlement} in {@region} grew into a city.\" }"));
    REQUIRE(contains(history_data::REPORT_TOML,
                     "template = \"{from} gave up {settlement:all} in {@region} to {to}.\" },\n"
                     "  { kind = \"claim_dropped\", salience = 15, template = \"{claimant} let the claim against {holder} "
                     "over {@region} lapse.\" },\n"
                     "  { kind = \"refuge_hidden\", salience = 10, template = \"{@settlement} lay hidden from {@year} to "
                     "{@end_year}.\" },\n  { kind = \"region_renamed\", salience = 60, "));   // P3-04 r21 23.2 (P5-12)
    // the listings load (and 8.1 test 1 compares them with game/data and both fixture roots)
    CHECK(load(history_data::shipped_files()).ok());
}

TEST_CASE("P3-01 5.1: history.chronology bounds, required fields and chronology_age_name_form") {
    const std::string path = "history/chronology.toml";
    struct Edit {
        const char* from;
        const char* ok;
        const char* bad;
        const char* key;
        std::uint32_t line;
    };
    // fx(0, "0.05"): raw 214748365; "0.050000001" is raw 214748369.
    for (const Edit& e : std::vector<Edit>{
             {"awakening_jitter_frac = \"0.01\"", "awakening_jitter_frac = \"0.05\"", "awakening_jitter_frac = \"0.050000001\"",
              "history.chronology.awakening_jitter_frac", 2},
             {"awakening_jitter_frac = \"0.01\"", "awakening_jitter_frac = 0", "awakening_jitter_frac = \"-0.000000001\"",
              "history.chronology.awakening_jitter_frac", 2},
             {"awakening_min_gap_years = 100", "awakening_min_gap_years = 1", "awakening_min_gap_years = 0",
              "history.chronology.awakening_min_gap_years", 3},
             {"awakening_min_gap_years = 100", "awakening_min_gap_years = 2000", "awakening_min_gap_years = 2001",
              "history.chronology.awakening_min_gap_years", 3},
         }) {
        CAPTURE(std::string_view(e.bad));
        CHECK(load(shipped_edited(path, e.from, e.ok)).ok());
        const wd::LoadError err = only_error(load(shipped_edited(path, e.from, e.bad)));
        CHECK(err.path == path);
        CHECK(err.line == e.line);
        CHECK(contains(err.message, "'" + std::string(e.key) + "'"));
    }
    CHECK(contains(dump(load(shipped_edited(path, "\"0.01\"", "0.01"))), "floating-point"));
    // age_name_form: string(64) and its check.
    const std::string form = "age_name_form = \"The Age of {plural}\"";
    const std::string sixty_four = "The " + std::string(50, 'x') + " {plural}";  // 63 bytes; + "x" is 64
    REQUIRE(sixty_four.size() == 63U);
    CHECK(load(shipped_edited(path, form, "age_name_form = \"" + sixty_four + "x\"")).ok());
    const wd::LoadError e65 = only_error(load(shipped_edited(path, form, "age_name_form = \"" + sixty_four + "xx\"")));
    CHECK(contains(e65.message, "'history.chronology.age_name_form' is 65 bytes long"));
    for (const char* good : {"The {adjective} Age", "{plural}", "Age of {plural} and {adjective}", "{plural}{plural}"}) {
        CAPTURE(std::string_view(good));
        CHECK(load(shipped_edited(path, form, std::string("age_name_form = \"") + good + "\"")).ok());
    }
    for (const char* bad : {"The Age", "", "The Age of {name}", "The Age of {plural} {", "{Plural}", "{plural} {root}",
                            "The Age of {plural"}) {
        CAPTURE(std::string_view(bad));
        at_definition(only_error(load(shipped_edited(path, form, std::string("age_name_form = \"") + bad + "\""))), path,
                      "chronology_age_name_form", "history.chronology",
                      "age_name_form must contain {plural} or {adjective} and no other placeholder");
    }
    // Every field is required (P3-01 5 "like worldgen"; P1-00 6.1: no code defaults).
    for (const char* line : {"awakening_jitter_frac = \"0.01\"\n", "awakening_min_gap_years = 100\n",
                             "age_name_form = \"The Age of {plural}\"\n"}) {
        CAPTURE(std::string_view(line));
        const wd::LoadError e = only_error(load(shipped_edited(path, line, "")));
        CHECK(e.line == 1U);
        CHECK(contains(e.message, "missing required field 'history.chronology."));
    }
    CHECK_FALSE(load(shipped_edited(path, "awakening_min_gap_years = 100\n", "awakening_min_gap_years = 100\nspan = 6000\n")).ok());
}

// --- The culture kind and its naming record (P3-01 4; P3-02 3) ----------------------------

TEST_CASE("P3-01 4, P3-02 3: the culture_naming record's grammars, lengths and named checks") {
    const std::string path = "cultures/elven.toml";
    const auto naming_line = [](const char* field) -> std::uint32_t {
        // Line of each field in a culture file (header at 1, naming table at 5).
        const std::vector<std::string_view> f = history_data::naming_fields();
        for (std::size_t i = 0; i < f.size(); ++i) {
            if (f[i] == field) {
                return static_cast<std::uint32_t>(6 + i);
            }
        }
        return 0;
    };
    const std::string syllable = "; expected a string of at most 8 bytes, a syllable (lower-case letters a-z, possibly empty)";
    const std::string pattern = "; expected a string of at most 16 bytes, a name pattern (O, N, C, ' and -, with at least one N)";
    const std::string form =
        "; expected a string of at most 48 bytes, a name form (letters, spaces, ' and -, with exactly one {root})";
    struct Case {
        const char* from;
        const char* to;
        const char* field;
        const char* key;
        std::string tail;
    };
    const std::vector<Case> grammar = {
        {"onsets = [\"\", \"l\",", "onsets = [\"\", \"L\",", "onsets", "culture.elven.naming.onsets[1]", "\"L\"" + syllable},
        {"onsets = [\"\", \"l\",", "onsets = [\"\", \"l1\",", "onsets", "culture.elven.naming.onsets[1]", "\"l1\"" + syllable},
        {"codas = [\"\", \"\", \"l\",", "codas = [\"\", \"\", \"l'\",", "codas", "culture.elven.naming.codas[2]", "\"l'\"" + syllable},
        {"forbidden = [\"aea\",", "forbidden = [\"a e\",", "forbidden", "culture.elven.naming.forbidden[0]", "\"a e\"" + syllable},
        {"patterns = [\"ONONC\",", "patterns = [\"OCC\",", "patterns", "culture.elven.naming.patterns[0]", "\"OCC\"" + pattern},
        {"patterns = [\"ONONC\",", "patterns = [\"'ON\",", "patterns", "culture.elven.naming.patterns[0]", "\"'ON\"" + pattern},
        {"patterns = [\"ONONC\",", "patterns = [\"ON-\",", "patterns", "culture.elven.naming.patterns[0]", "\"ON-\"" + pattern},
        {"patterns = [\"ONONC\",", "patterns = [\"ON'-ON\",", "patterns", "culture.elven.naming.patterns[0]", "\"ON'-ON\"" + pattern},
        {"patterns = [\"ONONC\",", "patterns = [\"ONX\",", "patterns", "culture.elven.naming.patterns[0]", "\"ONX\"" + pattern},
        {"god_forms = [\"{root}\"]", "god_forms = [\"Lord\"]", "god_forms", "culture.elven.naming.god_forms[0]", "\"Lord\"" + form},
        {"god_forms = [\"{root}\"]", "god_forms = [\"{root} {root}\"]", "god_forms", "culture.elven.naming.god_forms[0]",
         "\"{root} {root}\"" + form},
        {"god_forms = [\"{root}\"]", "god_forms = [\" {root}\"]", "god_forms", "culture.elven.naming.god_forms[0]", "\" {root}\"" + form},
        {"god_forms = [\"{root}\"]", "god_forms = [\"{root}  Hold\"]", "god_forms", "culture.elven.naming.god_forms[0]",
         "\"{root}  Hold\"" + form},
        {"god_forms = [\"{root}\"]", "god_forms = [\"{root} 2\"]", "god_forms", "culture.elven.naming.god_forms[0]",
         "\"{root} 2\"" + form},
        {"god_forms = [\"{root}\"]", "god_forms = [\"{name}\"]", "god_forms", "culture.elven.naming.god_forms[0]", "\"{name}\"" + form},
    };
    for (const Case& c : grammar) {
        CAPTURE(std::string_view(c.to));
        const wd::LoadError e = only_error(load(shipped_edited(path, c.from, c.to)));
        CHECK(e.path == path);
        CHECK(e.line == naming_line(c.field));
        CHECK(e.message == "'" + std::string(c.key) + "' is " + c.tail);
    }
    // Valid grammar edge cases load.
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"patterns = [\"ONONC\",", "patterns = [\"N\","},
             {"patterns = [\"ONONC\",", "patterns = [\"O'N-C\","},
             {"god_forms = [\"{root}\"]", "god_forms = [\"The O'Neill-{root} of Ys\"]"},
             {"codas = [\"\", \"\", \"l\",", "codas = [\"\", \"\", \"abcdefgh\","}}) {
        CAPTURE(to);
        const Load l = load(shipped_edited(path, from, to));
        CHECK_MESSAGE(l.ok(), dump(l));
    }
    // Length rules come before the grammar (P0-03 amendment 17).
    CHECK(contains(only_error(load(shipped_edited(path, "codas = [\"\", \"\", \"l\",", "codas = [\"\", \"\", \"abcdefghi\","))).message,
                   "'culture.elven.naming.codas[2]' is 9 bytes long"));
    CHECK(contains(only_error(load(shipped_edited(path, "patterns = [\"ONONC\",", "patterns = [\"ONONONONONONONONC\","))).message,
                   "'culture.elven.naming.patterns[0]' is 17 bytes long"));
    CHECK(contains(only_error(load(shipped_edited(path, "god_forms = [\"{root}\"]",
                                                  "god_forms = [\"{root} " + std::string(42, 'x') + "\"]")))
                       .message,
                   "'culture.elven.naming.god_forms[0]' is 49 bytes long"));
    CHECK(load(shipped_edited(path, "god_forms = [\"{root}\"]", "god_forms = [\"{root} " + std::string(41, 'x') + "\"]")).ok());

    // Named checks, at the culture's header, in the culture's name.
    at_definition(only_error(load(shipped_edited(path, "nuclei = [\"a\",", "nuclei = [\"\","))), path,
                  "naming_nuclei_nonempty", "culture.elven", "naming.nuclei[0] must not be empty");
    at_definition(only_error(load(shipped_edited(path, "min_letters = 4", "min_letters = 12"))), path,
                  "naming_letters_ordered", "culture.elven", "naming.min_letters must not be above naming.max_letters");
    CHECK(load(shipped_edited(path, "min_letters = 4", "min_letters = 11")).ok());
    at_definition(only_error(load(shipped_edited(path, "\"ea\", \"ia\"]", "\"ea\", \"\"]"))), path,
                  "naming_forbidden_nonempty", "culture.elven", "naming.forbidden[11] must not be empty");

    // Array lengths (P3-02 3) and the letter bounds int(1, 32).
    for (const auto& [from, to, key] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"min_letters = 4", "min_letters = 0", "culture.elven.naming.min_letters"},
             {"max_letters = 11", "max_letters = 33", "culture.elven.naming.max_letters"},
             {"god_forms = [\"{root}\"]", "god_forms = []", "culture.elven.naming.god_forms"},
             {"god_forms = [\"{root}\"]", "god_forms = [\"{root}\", \"{root}\", \"{root}\", \"{root}\", \"{root}\", \"{root}\", "
                                          "\"{root}\", \"{root}\", \"{root}\"]",
              "culture.elven.naming.god_forms"},
             {"onsets = [\"\", \"l\", \"th\", \"s\", \"f\", \"n\", \"m\", \"r\", \"v\", \"gl\", \"c\", \"el\", \"ith\"]",
              "onsets = []", "culture.elven.naming.onsets"}}) {
        CAPTURE(to);
        const wd::LoadError e = only_error(load(shipped_edited(path, from, to)));
        CHECK(contains(e.message, "'" + key + "'"));
    }
    CHECK(load(shipped_edited(path, "min_letters = 4", "min_letters = 1")).ok());
    CHECK(load(shipped_edited(path, "max_letters = 11", "max_letters = 32")).ok());
    // forbidden may be empty (0 to 32).
    CHECK(load(shipped_edited(path, "forbidden = [\"aea\", \"iei\", \"iae\", \"eae\", \"aa\", \"ii\", \"ee\", \"thth\", \"lll\", \"nnn\", \"ea\", \"ia\"]",
                              "forbidden = []"))
              .ok());
    // 64 onsets load, 65 do not; 16 patterns load, 17 do not.
    const auto list = [](const std::string& item, int n) {
        std::string s = "[";
        for (int i = 0; i < n; ++i) {
            s += (i ? ", \"" : "\"") + item + "\"";
        }
        return s + "]";
    };
    const std::string onsets = "[\"\", \"l\", \"th\", \"s\", \"f\", \"n\", \"m\", \"r\", \"v\", \"gl\", \"c\", \"el\", \"ith\"]";
    CHECK(load(shipped_edited(path, onsets, list("l", 64))).ok());
    CHECK(contains(only_error(load(shipped_edited(path, onsets, list("l", 65)))).message, "'culture.elven.naming.onsets'"));
    const std::string patterns = "[\"ONONC\", \"ONONC\", \"ONCON\", \"ONONON\", \"NCONC\"]";
    CHECK(load(shipped_edited(path, patterns, list("ONC", 16))).ok());
    CHECK(contains(only_error(load(shipped_edited(path, patterns, list("ONC", 17)))).message, "'culture.elven.naming.patterns'"));

    // Every naming field is required; no other key; reported at the naming table.
    for (const std::string_view field : history_data::naming_fields()) {
        CAPTURE(std::string(field));
        std::string text = history_data::culture_toml(history_data::culture_listings()[1]);
        const std::size_t at = text.find("\n" + std::string(field) + " = ") + 1;
        REQUIRE(at != 0U);
        text.erase(at, text.find('\n', at) + 1 - at);
        Files f = history_data::shipped_files();
        f[path] = text;
        const wd::LoadError e = only_error(load(f));
        CHECK(e.line == 5U);  // [culture.elven.naming]
        CHECK(e.message == "missing required field 'culture.elven.naming." + std::string(field) + "'");
    }
    CHECK(contains(only_error(load(shipped_edited(path, "min_letters = 4\n", "min_letters = 4\nperson_forms = [\"{root}\"]\n"))).message,
                   "culture.elven.naming.person_forms"));
    // The naming table is required too.
    {
        std::string text = history_data::culture_toml(history_data::culture_listings()[1]);
        text = text.substr(0, text.find("\n[culture.elven.naming]") + 1);
        Files f = history_data::shipped_files();
        f[path] = text;
        const wd::LoadError e = only_error(load(f));
        CHECK(e.message == "missing required field 'culture.elven.naming'");
    }
}

// --- Content hash (P3-01 7) -------------------------------------------------------------

TEST_CASE("P3-01 7: race, culture and history values are part of content_hash; layout is not") {
    const Load base = load(history_data::shipped_files());
    REQUIRE(base.ok());
    const std::string h = wd::content_hash_hex(*base.content);
    // One value changed: a different hash.
    for (const auto& [path, from, to] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"races/elf.toml", "growth_permille = 3", "growth_permille = 4"},
             {"races/elf.toml", "weight = 500", "weight = 501"},
             {"races/orc.toml", "awakening_share = \"0.93\"", "awakening_share = \"0.94\""},
             {"races/dwarf.toml", "name = \"Dwarf\"", "name = \"Dwarrow\""},
             {"races/human.toml", "camp = \"neutral\"", "camp = \"good\""},
             {"races/human.toml", "\"city\", \"fortress\"]", "\"fortress\", \"city\"]"},
             {"cultures/human.toml", "\"{root}wick\"", "\"{root}wich\""},
             {"cultures/goblin.toml", "min_letters = 3", "min_letters = 4"},
             {"history/chronology.toml", "awakening_min_gap_years = 100", "awakening_min_gap_years = 101"},
             {"history/chronology.toml", "The Age of {plural}", "The {adjective} Age"}}) {
        CAPTURE(to);
        const Load l = load(shipped_edited(path, from, to));
        INFO(dump(l));
        REQUIRE(l.ok());
        CHECK(wd::content_hash_hex(*l.content) != h);
    }
    // Comments, whitespace, key order, number spelling and file layout: the same hash.
    Files layout = shipped_edited("races/elf.toml", "order = 1\n", "# the first race\norder   =   1\n");
    layout = edited(layout, "races/goblin.toml", "awakening_share = \"0.35\"", "awakening_share = \"0.350\"");
    layout = edited(layout, "races/orc.toml", "upland = 100\ncoast = -100\n", "coast = -100\nupland = 100\n");
    layout = edited(layout, "history/chronology.toml", "awakening_min_gap_years = 100", "awakening_min_gap_years = 0x64");
    std::string all_races;
    for (const char* r : {"races/orc.toml", "races/elf.toml", "races/human.toml", "races/dwarf.toml",
                          "races/goblin.toml", "races/faerie.toml"}) {
        all_races += layout[r] + "\n";
        layout.erase(r);
    }
    layout["races/all.toml"] = all_races;
    const Load moved = load(layout);
    INFO(dump(moved));
    REQUIRE(moved.ok());
    CHECK(wd::content_hash_hex(*moved.content) == h);
    CHECK(*moved.content == *base.content);
}

TEST_CASE("P3-01 r14 3.7 (P3-11): race awakening_population is required, after hidden_years; orc 1200, others 0") {
    // required: an orc file without it is refused with the loader's message for a missing field
    const wd::LoadError miss =
        only_error(load(shipped_edited("races/orc.toml", "awakening_population = 1200\n", "")));
    CHECK(miss.path == "races/orc.toml");
    CHECK(miss.message == "missing required field 'race.orc.awakening_population'");
    // the shipped values (the listing) and the field order
    for (const history_data::Race& r : history_data::races()) {
        CAPTURE(std::string(r.id));
        CHECK(r.awakening_population == (r.id == "orc" ? 1200 : 0));
    }
    const std::vector<std::string_view>& f = history_data::race_fields();
    const auto at = [&](std::string_view n) { return std::find(f.begin(), f.end(), n) - f.begin(); };
    CHECK(at("awakening_population") == at("hidden_years") + 1);
    CHECK(at("settlement_kinds") == at("awakening_population") + 1);
    // the shipped root loads with the field at its bounds for every race
    CHECK(load(shipped_edited("races/elf.toml", "awakening_population = 0", "awakening_population = 100000")).ok());
}
