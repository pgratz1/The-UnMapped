#pragma once

// The one place of the P5-02 tests' guesses at library names the spec (docs/specs/p5-02-beasts-monsters.md r0)
// does not give. Every name the spec declares is used as declared (13's Timeline reads, 7.1's WildView and
// set_wild, 7.4's StepContext::phase_trigger_event, 9.3's MonstersSubsystem::monster_name, 6.5's tension); the
// rest is marked GUESS and listed in the test writer's report:
//
//
// Checked at integration (phase 2, against origin/p5/02-impl-r1): every guess below held as written except one.
//   (11.2) HistoryResult gains beasts, monster_kinds, beast_populations, monsters, sites; SettlementRow::ending.
//   db::StageWriter / StageContext gain write_beast ... write_site.
//   (9.3) NOT a fourth constructor parameter: RenamingSubsystem::set_monsters(const MonsterNames*) installs the
//     monsters' roots (MonsterNames, worldgen/history/renaming.hpp, implemented by MonstersSubsystem); without it
//     a monster name_role gives no {name}.
//   (3) history::chance_ppb is declared in worldgen/history/monsters.hpp.
//   (13) SettlementState::ending, history::SiteState and MonsterState, Timeline::monster_count() (size_t) and
//     site_count(); HistoryTables::sites, monsters, beast_populations.
//   (9.1) NameKind monster and site; CultureNaming::site_forms and monster_forms.

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
