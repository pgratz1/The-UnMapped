#pragma once

// The one place of the P5-02 tests' guesses at library names the spec (docs/specs/p5-02-beasts-monsters.md r0)
// does not give. Every name the spec declares is used as declared (13's Timeline reads, 7.1's WildView and
// set_wild, 7.4's StepContext::phase_trigger_event, 9.3's MonstersSubsystem::monster_name, 6.5's tension); the
// rest is marked GUESS and listed in the test writer's report:
//
//   GUESS (11.2, 17 step 6 "HistoryResult members and writers"): stages::HistoryResult and history::HistoryResult
//     gain `beasts`, `monster_kinds`, `beast_populations`, `monsters`, `sites` (vectors of db::BeastRow,
//     db::MonsterKindRow, db::BeastPopulationRow, db::MonsterRow, db::SiteRow), and db::SettlementRow gains a last
//     member `ending`; the row types' members are the columns of 11.1 in order, the nullable ones
//     std::optional<std::int64_t> (monster.died, site.ended_year), grouped an std::int64_t (0 or 1).
//   GUESS: db::StageWriter gains write_beast, write_monster_kind, write_beast_population, write_monster and
//     write_site, as write_region_name (P5-12).
//   GUESS (9.3): RenamingSubsystem's constructor takes a fourth parameter, `const MonstersSubsystem*` (null: no
//     monster is installed; a rule naming a monster role is then never met), declared in
//     worldgen/history/renaming.hpp; MonstersSubsystem is declared in worldgen/history/monsters.hpp (5).
//   GUESS (3): chance_ppb is declared beside chance_pm and chance_ppm (worldgen/history/peoples.hpp) as
//     history::chance_ppb(std::uint64_t u, std::int64_t p).
//   GUESS (13): the Timeline's SettlementState gains `ending` (the spec's text) and the two structs are
//     history::SiteState and history::MonsterState in worldgen/history/timeline.hpp; Timeline::monster_count()
//     gives the number of monster rows (the dumps' loop); HistoryTables gains `sites`, `monsters` and
//     `beast_populations`; StageContext gains write_beast ... write_site (the fixture builder).
//   GUESS (9.1, P3-02 4.5's rule): NameKind gains `monster` and `site` after `settlement`; CultureNaming gains
//     `site_forms` (the culture field) and `monster_forms` (no culture field: the subsystem sets it to the kind's
//     name_forms), read by name_forms(naming, kind); name_stream and name_kind_text name them.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/writer.hpp"
#include "worldgen/history/monsters.hpp"   // GUESS: the header of 5
#include "worldgen/history/names.hpp"
#include "worldgen/history/peoples.hpp"

namespace p502api {

namespace db = worldgen::db;
namespace h = worldgen::history;

// 6.4's status codes and 11.2's site codes (normative).
inline constexpr std::int64_t AWAKE = 1, ASLEEP = 2, SLAIN = 3, DIED = 4;
inline constexpr std::int64_t LAIR = 1, OUTPOST = 2;

inline bool chance_ppb(std::uint64_t u, std::int64_t p) { return h::chance_ppb(u, p); }   // GUESS (3)

// 9.1: a monster's naming data, its naming culture's with the kind's name_forms (GUESS CultureNaming::monster_forms).
inline h::CultureNaming monster_naming(h::CultureNaming culture, std::vector<std::string> forms) {
    culture.monster_forms = std::move(forms);
    return culture;
}

}  // namespace p502api
