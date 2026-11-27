#pragma once

// The one place where the P3-03 chronology tests touch library API that the
// spec introduces (docs/specs/p3-03-chronology.md). Written from the spec
// before the implementation existed (test-writer rule): every other test
// file uses only the test-owned types and functions below, plus API that
// existed before P3-03 (the loader, WorldReader, inspect, the P3-02 names
// through p3_02_names_api.hpp).
//
// Library names used here, as the spec gives them (sections 2, 4.1, 5.3, 6
// and the interim seams of 14); where the spec leaves a name open, the
// guess is marked GUESS and is the first thing to check at integration:
//
//   worldgen/history/params.hpp       history::HistoryParams (fields of 4.1),
//                                     history::history_params_from(Settings),
//                                     history::history_meta_entries(params),
//                                     history::history_params_from_meta(WorldReader)
//   worldgen/history/data_checks.hpp  check_history_data(ContentSet)
//                                     GUESS: in namespace worldgen::history
//   worldgen/stages/chronology.hpp    stages::ChronologyTuning {awakening_jitter_frac,
//                                     awakening_min_gap_years, age_name_form},
//                                     chronology_tuning_from, AwakeningBounds {gap, jmax},
//                                     awakening_bounds, awakening_years, age_name_text,
//                                     ChronologyResult {races, race_biomes, cultures, gods,
//                                     ages}, chronology_stage, run_chronology
//   worldgen/db/writer.hpp            db::RaceRow, RaceBiomeRow, CultureRow, GodRow, AgeRow
//                                     (columns of 5.1 in declared order, field names = column
//                                     names; RaceRow::awakening_year std::optional<int64_t>),
//                                     WorldWriter::set_param_text(name, value)
//   worldgen/db/schema.hpp            SCHEMA_VERSION, DDL_STATEMENTS (13 entries),
//                                     REQUIRED_META_KEYS (16 entries)
//   worldgen/stages/stage.hpp         STAGE_NAMES (8 names, renamed from
//                                     GEOGRAPHY_STAGE_NAMES, 4.3)
//   worldgen/generate.hpp             WorldParams::history
//
// Every contract violation is expected to throw worldgen::FatalError.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/settings.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/history/data_checks.hpp"
#include "worldgen/history/params.hpp"
#include "worldgen/stages/chronology.hpp"
#include "worldgen/stages/stage.hpp"

namespace chrono_api {

namespace wd = worldgen::data;

// --- HistoryParams (4.1), test-owned ------------------------------------------------------------

// The eight settings in the text of their meta values (4.2) where the
// library has an enum; races in race order.
struct Params {
    std::int64_t span = 6000;
    std::string detail = "normal";
    std::optional<std::int64_t> civs;  // nullopt = auto
    std::vector<std::string> races = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};
    std::string intensity = "normal";
    std::string pressure = "normal";
    std::string activity = "normal";
    std::string script;
    friend bool operator==(const Params&, const Params&) = default;
};

namespace detail {
inline wd::HistoryDetail detail_of(std::string_view s) {
    if (s == "sparse") return wd::HistoryDetail::sparse;
    if (s == "dense") return wd::HistoryDetail::dense;
    return wd::HistoryDetail::normal;
}
inline wd::EventIntensity intensity_of(std::string_view s) {
    if (s == "calm") return wd::EventIntensity::calm;
    if (s == "turbulent") return wd::EventIntensity::turbulent;
    return wd::EventIntensity::normal;
}
inline wd::MonsterPressure pressure_of(std::string_view s) {
    if (s == "low") return wd::MonsterPressure::low;
    if (s == "high") return wd::MonsterPressure::high;
    return wd::MonsterPressure::normal;
}
inline wd::DivineActivity activity_of(std::string_view s) {
    if (s == "distant") return wd::DivineActivity::distant;
    if (s == "meddling") return wd::DivineActivity::meddling;
    return wd::DivineActivity::normal;
}
}  // namespace detail

inline worldgen::history::HistoryParams to_lib(const Params& p) {
    worldgen::history::HistoryParams h;
    h.span = p.span;
    h.detail = detail::detail_of(p.detail);
    h.civs_per_race = p.civs;
    h.races = p.races;
    h.event_intensity = detail::intensity_of(p.intensity);
    h.monster_pressure = detail::pressure_of(p.pressure);
    h.divine_activity = detail::activity_of(p.activity);
    h.divine_script = p.script;
    return h;
}

inline Params from_lib(const worldgen::history::HistoryParams& h) {
    Params p;
    p.span = h.span;
    p.detail = std::string(wd::history_detail_name(h.detail));
    p.civs = h.civs_per_race;
    p.races.assign(h.races.begin(), h.races.end());
    p.intensity = std::string(wd::event_intensity_name(h.event_intensity));
    p.pressure = std::string(wd::monster_pressure_name(h.monster_pressure));
    p.activity = std::string(wd::divine_activity_name(h.divine_activity));
    p.script = std::string(h.divine_script);
    return p;
}

// The defaults of a default-constructed HistoryParams (4.1).
inline Params library_defaults() { return from_lib(worldgen::history::HistoryParams{}); }

// history_params_from(Settings): FatalError unless resolve_races ran.
inline Params params_from_settings(const wd::Settings& s) { return from_lib(worldgen::history::history_params_from(s)); }

// history_meta_entries: the eight (key, value) pairs, ascending key.
inline std::vector<std::pair<std::string, std::string>> meta_entries(const Params& p) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& [k, v] : worldgen::history::history_meta_entries(to_lib(p))) {
        out.emplace_back(std::string(k), std::string(v));
    }
    return out;
}

// history_params_from_meta: the inverse, read from a stored world.
inline Params params_from_meta(const worldgen::db::WorldReader& r) {
    return from_lib(worldgen::history::history_params_from_meta(r));
}

// WorldParams::history.
inline void set_history(worldgen::WorldParams& w, const Params& p) { w.history = to_lib(p); }

// WorldWriter::set_param_text (4.1).
inline void set_param_text(worldgen::db::WorldWriter& w, std::string_view name, std::string_view value) {
    w.set_param_text(name, value);
}

// --- Section 6 ----------------------------------------------------------------------------------

inline std::vector<wd::LoadError> check_data(const wd::ContentSet& c) {
    return worldgen::history::check_history_data(c);  // GUESS: namespace history
}

// --- C2 to C4 seams (14) ------------------------------------------------------------------------

struct Tuning {
    std::int64_t jitter_raw = 0;  // awakening_jitter_frac, Fx raw
    std::int64_t gap = 0;         // awakening_min_gap_years
    std::string form;             // age_name_form
    friend bool operator==(const Tuning&, const Tuning&) = default;
};

inline worldgen::stages::ChronologyTuning to_lib(const Tuning& t) {
    worldgen::stages::ChronologyTuning c{};
    c.awakening_jitter_frac = worldgen::Fx::from_raw(t.jitter_raw);
    c.awakening_min_gap_years = t.gap;
    c.age_name_form = t.form;
    return c;
}

inline Tuning tuning_from(const wd::ContentSet& c) {
    const worldgen::stages::ChronologyTuning t = worldgen::stages::chronology_tuning_from(c);
    return Tuning{t.awakening_jitter_frac.raw(), t.awakening_min_gap_years, std::string(t.age_name_form)};
}

struct Bounds {
    std::int64_t gap = 0;
    std::int64_t jmax = 0;
    friend bool operator==(const Bounds&, const Bounds&) = default;
};

inline Bounds bounds(std::int64_t span, std::int64_t present, const Tuning& t) {
    const auto b = worldgen::stages::awakening_bounds(span, present, to_lib(t));
    return Bounds{b.gap, b.jmax};
}

// C3: the years of the present races (ids = race ids in race order, shares raw).
inline std::vector<std::int64_t> awakening_years(std::uint64_t seed, std::int64_t span,
                                                 const std::vector<std::int64_t>& ids,
                                                 const std::vector<std::int64_t>& shares_raw, const Tuning& t) {
    std::vector<worldgen::Fx> shares;
    for (const std::int64_t r : shares_raw) {
        shares.push_back(worldgen::Fx::from_raw(r));
    }
    const auto y = worldgen::stages::awakening_years(seed, span, std::span<const std::int64_t>(ids),
                                                     std::span<const worldgen::Fx>(shares), to_lib(t));
    return std::vector<std::int64_t>(y.begin(), y.end());
}

inline std::string age_name_text(std::string_view form, std::string_view plural, std::string_view adjective) {
    return std::string(worldgen::stages::age_name_text(form, plural, adjective));
}

// --- Rows (5.2), test-owned ---------------------------------------------------------------------

struct RaceRow {
    std::int64_t id = 0;
    std::string key, name, plural, adjective;
    std::int64_t camp = 0, lifespan_years = 0, generation_years = 0, culture = 0, present = 0;
    std::optional<std::int64_t> awakening_year;
    std::int64_t creator_god = 0;
    friend bool operator==(const RaceRow&, const RaceRow&) = default;
};
struct RaceBiomeRow {
    std::int64_t race = 0, biome = 0, weight = 0;
    friend bool operator==(const RaceBiomeRow&, const RaceBiomeRow&) = default;
};
struct CultureRow {
    std::int64_t id = 0;
    std::string key, name;
    std::int64_t race = 0;
    friend bool operator==(const CultureRow&, const CultureRow&) = default;
};
struct GodRow {
    std::int64_t id = 0;
    std::string name;
    std::int64_t camp = 0;
    std::int64_t domain = 1;      // schema 14 (P5-08 9)
    std::string title;
    std::int64_t race = 0;
    friend bool operator==(const GodRow&, const GodRow&) = default;
};
struct AgeRow {
    std::int64_t id = 0, race = 0, start_year = 0, end_year = 0;
    std::string name;
    friend bool operator==(const AgeRow&, const AgeRow&) = default;
};
struct Rows {
    std::vector<RaceRow> races;
    std::vector<RaceBiomeRow> race_biomes;
    std::vector<CultureRow> cultures;
    std::vector<GodRow> gods;
    std::vector<AgeRow> ages;
    friend bool operator==(const Rows&, const Rows&) = default;
};

// chronology_stage (C1 to C5, 14): the seam takes the region names as given.
inline Rows stage(const wd::ContentSet& c, const Params& p, std::uint64_t seed,
                  const std::vector<std::string>& region_names) {
    const worldgen::stages::ChronologyResult r =
        worldgen::stages::chronology_stage(c, to_lib(p), seed, std::span<const std::string>(region_names));
    Rows out;
    for (const auto& x : r.races) {
        out.races.push_back(RaceRow{x.id, std::string(x.key), std::string(x.name), std::string(x.plural),
                                    std::string(x.adjective), x.camp, x.lifespan_years, x.generation_years, x.culture,
                                    x.present, x.awakening_year, x.creator_god});
    }
    for (const auto& x : r.race_biomes) {
        out.race_biomes.push_back(RaceBiomeRow{x.race, x.biome, x.weight});
    }
    for (const auto& x : r.cultures) {
        out.cultures.push_back(CultureRow{x.id, std::string(x.key), std::string(x.name), x.race});
    }
    for (const auto& x : r.gods) {
        out.gods.push_back(GodRow{x.id, std::string(x.name), x.camp, x.domain, std::string(x.title), x.race});
    }
    for (const auto& x : r.ages) {
        out.ages.push_back(AgeRow{x.id, x.race, x.start_year, x.end_year, std::string(x.name)});
    }
    return out;
}

// ChronologyResult::god_names (the engineer's addition): per god in id
// order, "id:name:attempt" joined by "|", "F" for the numeral fallback (the
// format of the golden file's gods item).
inline std::string god_choices(const wd::ContentSet& c, const Params& p, std::uint64_t seed,
                               const std::vector<std::string>& region_names) {
    const worldgen::stages::ChronologyResult r =
        worldgen::stages::chronology_stage(c, to_lib(p), seed, std::span<const std::string>(region_names));
    std::string out;
    for (std::size_t i = 0; i < r.god_names.size(); ++i) {
        const auto& g = r.god_names[i];
        out += (out.empty() ? "" : "|") + std::to_string(i < r.gods.size() ? r.gods[i].id : 0) + ":" +
               std::string(g.name) + ":" + (g.fallback ? std::string("F") : std::to_string(g.attempt));
    }
    return out;
}

// WorldWriter::set_param_integer and set_param_decimal (the same canonical
// check for the eight keys).
inline void set_param_integer(worldgen::db::WorldWriter& w, std::string_view name, std::int64_t value) {
    w.set_param_integer(name, value);
}
inline void set_param_decimal(worldgen::db::WorldWriter& w, std::string_view name, std::string_view value) {
    w.set_param_decimal(name, value);
}

// The stage entry for generate_world_with_stages.
inline worldgen::stages::StageInfo stage_info() {
    return worldgen::stages::StageInfo{"chronology", &worldgen::stages::run_chronology};
}

// --- Schema and stage list (4.2, 4.3, 5.1) -------------------------------------------------------

inline constexpr std::int64_t schema_version() { return worldgen::db::SCHEMA_VERSION; }

inline std::vector<std::string> ddl_statements() {
    std::vector<std::string> out;
    for (const std::string_view s : worldgen::db::DDL_STATEMENTS) {
        out.emplace_back(s);
    }
    return out;
}

inline std::vector<std::string> required_meta_keys() {
    std::vector<std::string> out;
    for (const std::string_view s : worldgen::db::REQUIRED_META_KEYS) {
        out.emplace_back(s);
    }
    return out;
}

inline std::vector<std::string> stage_names() {
    std::vector<std::string> out;
    for (const std::string_view s : worldgen::stages::STAGE_NAMES) {
        out.emplace_back(s);
    }
    return out;
}

}  // namespace chrono_api
