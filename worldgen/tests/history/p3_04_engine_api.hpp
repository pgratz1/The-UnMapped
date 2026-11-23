#pragma once

// The one place where the P3-04 engine tests touch library API that the
// spec introduces (docs/specs/p3-04-simulation-engine.md). Written from the
// spec before the implementation existed (test-writer rule). Every other
// P3-04 test file uses the test-owned types below, the spec's seam types
// re-exported here under `eng` (their names and members are the spec's,
// sections 3.3, 6.1, 6.4, 6.8, 8.2, 8.5), and API older than P3-04.
//
// Names used here as the spec gives them:
//   worldgen/history/engine.hpp          EntityType, PhaseKind, SettlementKind, SettlementStatus,
//                                        Civilization, Settlement, RegionState, RelationKey,
//                                        RelationState, State, Step, Participant, EventSpec,
//                                        StepContext, Subsystem, AwakeningHandler, TensionReport,
//                                        TensionSource, EngineConfig, run_engine (6.1, 6.4)
//   worldgen/history/inputs.hpp          HistoryInputs, history_inputs_from,
//                                        history_inputs_from_world (3.2, 3.3)
//   worldgen/history/event_catalogue.hpp RoleDef, KindDef, EventCatalogue, compiled_event_catalogue,
//                                        event_catalogue, read_event_catalogue, template_problem,
//                                        HistoryView::load (-> db::Result), render_event (8.2, 8.5)
//   worldgen/stages/history.hpp          HistoryResult, history_stage, run_history (3.3)
//   worldgen/db/schema.hpp               SCHEMA_VERSION 7, DDL_STATEMENTS (26), REQUIRED_META_KEYS (17)
//
// Where the spec leaves a name open, the guess is marked GUESS and is the
// first thing to check at integration:
//   GUESS: Intervention, InterventionSource and EmptyInterventionSource
//          (6.8) are declared in worldgen/history/engine.hpp.
//   GUESS: history::testing::ForcedTension (7.4) is declared in
//          worldgen/history/testing.hpp and constructed as
//          ForcedTension(period, value, trigger).
//   GUESS: the stub awakening handler (7) is history::StubAwakening in
//          worldgen/history/stubs.hpp, default-constructible.
//   GUESS: the members of stages::HistoryResult are named after the
//          tables, in DDL order: event_kinds, event_kind_roles,
//          event_kind_values, phases, events, event_participants,
//          event_causes, event_values, civilizations, settlements,
//          settlement_census, region_control, relations.
//   GUESS: the row types db::EventKindRow ... db::RelationRow (9.3) have
//          one member per column named as the column, in declared order,
//          except EventKindRow's `template` (a C++ keyword), guessed as
//          `templ` like KindDef::templ.
//   GUESS: db::StageWriter has write_event_kind ... write_relation like
//          the StageContext writers of 9.3 (precedent: write_region on
//          both).
//   GUESS: EngineConfig::interventions is an InterventionSource* and
//          EngineConfig::catalogue a const EventCatalogue* (6.4 literally).
//
// Every contract violation is expected to throw worldgen::FatalError.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/inputs.hpp"
#include "worldgen/history/stubs.hpp"    // GUESS (StubAwakening)
// ForcedTension is declared in worldgen/history/stubs.hpp (integration).
#include "worldgen/stages/history.hpp"
#include "worldgen/stages/stage.hpp"

namespace eng {

namespace h = worldgen::history;
namespace db = worldgen::db;

// --- The spec's seam types (6.1, 6.4, 6.8, 8.2) -------------------------------------------------

using h::AwakeningHandler;
using h::Civilization;
using h::EngineConfig;
using h::EntityType;
using h::EventCatalogue;
using h::EventSpec;
using h::HistoryInputs;
using h::KindDef;
using h::Participant;
using h::PhaseKind;
using h::RelationKey;
using h::RoleDef;
using h::Settlement;
using h::SettlementKind;
using h::SettlementStatus;
using h::State;
using h::Step;
using h::StepContext;
using h::Subsystem;
using h::TensionReport;
using h::TensionSource;
using h::Intervention;           // GUESS: in engine.hpp
using h::InterventionSource;     // GUESS: in engine.hpp
using h::EmptyInterventionSource;  // GUESS: in engine.hpp

// --- Test-owned rows ---------------------------------------------------------------------------

// One stored value: NULL, an integer or a text (the history tables hold
// nothing else, 9.1).
using Cell = db::ColumnValue;
using Row = std::vector<Cell>;

inline Cell I(std::int64_t v) {
    Cell c;
    c.type = db::ValueType::integer;
    c.integer = v;
    return c;
}
inline Cell T(std::string_view s) {
    Cell c;
    c.type = db::ValueType::text;
    c.bytes = std::string(s);
    return c;
}
inline Cell N() { return Cell{}; }
inline Cell O(const std::optional<std::int64_t>& v) { return v.has_value() ? I(*v) : N(); }

// The fourteen tables of 9.1 and r21 23.4 (region_name, schema 8), rows in write order (primary-key order, 9.3).
struct Tables {
    std::vector<Row> event_kind, event_kind_role, event_kind_value, phase, event, event_participant, event_cause,
        event_value, civilization, settlement, settlement_census, region_control, relation, region_name;

    // By table name (the names of 9.1).
    [[nodiscard]] const std::vector<Row>& of(std::string_view t) const {
        if (t == "event_kind") return event_kind;
        if (t == "event_kind_role") return event_kind_role;
        if (t == "event_kind_value") return event_kind_value;
        if (t == "phase") return phase;
        if (t == "event") return event;
        if (t == "event_participant") return event_participant;
        if (t == "event_cause") return event_cause;
        if (t == "event_value") return event_value;
        if (t == "civilization") return civilization;
        if (t == "settlement") return settlement;
        if (t == "settlement_census") return settlement_census;
        if (t == "region_control") return region_control;
        if (t == "relation") return relation;
        if (t == "region_name") return region_name;
        throw worldgen::FatalError("eng::Tables::of: unknown table");
    }
    [[nodiscard]] std::vector<Row>& of(std::string_view t) {
        return const_cast<std::vector<Row>&>(static_cast<const Tables&>(*this).of(t));
    }
    friend bool operator==(const Tables&, const Tables&) = default;
};

// HistoryResult as test-owned rows. GUESS: member and field names (see above).
inline Tables tables_of(const worldgen::stages::HistoryResult& r) {
    Tables t;
    for (const auto& x : r.event_kinds) {
        t.event_kind.push_back({I(x.id), T(x.key), I(x.caused), I(x.region_use), I(x.settlement_use), I(x.salience),
                                T(x.template_text)});
    }
    for (const auto& x : r.event_kind_roles) {
        t.event_kind_role.push_back({I(x.kind), I(x.seq), T(x.role), I(x.entity_type), I(x.min_count), I(x.max_count)});
    }
    for (const auto& x : r.event_kind_values) {
        t.event_kind_value.push_back({I(x.kind), I(x.seq), T(x.key)});
    }
    for (const auto& x : r.phases) {
        t.phase.push_back({I(x.id), I(x.age), I(x.kind), I(x.start_year), I(x.end_year), I(x.trigger_event)});
    }
    for (const auto& x : r.events) {
        t.event.push_back({I(x.id), I(x.year), I(x.end_year), I(x.kind), I(x.phase), I(x.region), I(x.settlement),
                           I(x.salience)});
    }
    for (const auto& x : r.event_participants) {
        t.event_participant.push_back({I(x.event), I(x.seq), I(x.role), I(x.entity_type), I(x.entity_id)});
    }
    for (const auto& x : r.event_causes) {
        t.event_cause.push_back({I(x.event), I(x.cause)});
    }
    for (const auto& x : r.event_values) {
        t.event_value.push_back({I(x.event), I(x.seq), I(x.value)});
    }
    for (const auto& x : r.civilizations) {
        t.civilization.push_back({I(x.id), T(x.name), I(x.race), I(x.culture), I(x.home_region), I(x.capital),
                                  I(x.founded_year), I(x.founding_event), O(x.ended_year), I(x.ending_event)});
    }
    for (const auto& x : r.settlements) {
        t.settlement.push_back({I(x.id), T(x.name), I(x.x), I(x.y), I(x.region), I(x.founder_civ), I(x.founded_year),
                                I(x.founding_event), I(x.owner_civ), I(x.kind), I(x.status), I(x.population),
                                O(x.ruined_year), I(x.ruin_event)});
    }
    for (const auto& x : r.settlement_census) {
        t.settlement_census.push_back({I(x.settlement), I(x.year), I(x.owner_civ), I(x.kind), I(x.status),
                                       I(x.population)});
    }
    for (const auto& x : r.region_control) {
        t.region_control.push_back({I(x.region), I(x.from_year), O(x.to_year), I(x.civ), I(x.event)});
    }
    for (const auto& x : r.relations) {
        t.relation.push_back({I(x.a_type), I(x.a_id), I(x.b_type), I(x.b_id), T(x.kind), I(x.since), O(x.until),
                              I(x.since_event), I(x.until_event)});
    }
    // P3-04 r21 23.4 (P5-12): HistoryResult::region_names, sorted by (region, from_year); the row type is
    // db::RegionNameRow with the column names of the table.
    for (const auto& x : r.region_names) {
        t.region_name.push_back({I(x.region), I(x.from_year), T(x.name), I(x.culture), I(x.event)});
    }
    return t;
}

// --- Seams --------------------------------------------------------------------------------------

inline Tables history_stage(const worldgen::data::ContentSet& c, const HistoryInputs& in) {
    return tables_of(worldgen::stages::history_stage(c, in));
}

inline Tables run_engine(const worldgen::data::ContentSet& c, const HistoryInputs& in, const EngineConfig& cfg) {
    return tables_of(h::run_engine(c, in, cfg));
}

inline HistoryInputs inputs_from_world(const db::WorldReader& r, const worldgen::data::ContentSet& c) {
    return h::history_inputs_from_world(r, c);
}
inline HistoryInputs inputs_from_context(const worldgen::stages::StageContext& ctx) {
    return h::history_inputs_from(ctx);
}

// The stage entry for generate_world_with_stages.
inline worldgen::stages::StageInfo stage_info() {
    return worldgen::stages::StageInfo{"history", &worldgen::stages::run_history};
}

// The P3-04 stage with the stub configuration (7): since generator 0.15.0
// the shipped stage installs the peoples subsystem (P3-05 13.1), so the
// stub-world tests run the engine with the stub through this seam. The
// layers P3-05 adds to the inputs are read when present (a synthetic
// geography may not write them; the stub does not read them).
inline void run_stub_history(worldgen::stages::StageContext& ctx) {
    if (ctx.regions().empty()) {
        throw worldgen::FatalError("history: input precondition: no region rows");
    }
    if (ctx.ages().empty()) {
        throw worldgen::FatalError("history: input precondition: no age rows");
    }
    HistoryInputs in;
    in.seed = ctx.seed();
    in.map_size = ctx.map_size();
    in.params = ctx.history_params();
    in.regions.assign(ctx.regions().begin(), ctx.regions().end());
    in.region_borders.assign(ctx.region_borders().begin(), ctx.region_borders().end());
    in.region_biomes.assign(ctx.region_biomes().begin(), ctx.region_biomes().end());
    in.races.assign(ctx.races().begin(), ctx.races().end());
    in.race_biomes.assign(ctx.race_biomes().begin(), ctx.race_biomes().end());
    in.cultures.assign(ctx.cultures().begin(), ctx.cultures().end());
    in.gods.assign(ctx.gods().begin(), ctx.gods().end());
    in.ages.assign(ctx.ages().begin(), ctx.ages().end());
    in.region_layer = ctx.layer("region");
    in.water_layer = ctx.layer("water");
    in.biomes.assign(ctx.biomes().begin(), ctx.biomes().end());
    const h::EngineTuning tuning = h::engine_tuning_from(ctx.content());
    h::check_engine_tuning(tuning);
    const h::EventCatalogue catalogue = h::event_catalogue(ctx.content());
    (void)h::check_engine_inputs(ctx.content(), in);
    h::StubAwakening stub;
    h::EmptyInterventionSource none;
    EngineConfig config;
    config.awakening = &stub;
    config.interventions = &none;
    config.catalogue = &catalogue;
    worldgen::stages::write_history_result(ctx, h::run_engine(ctx.content(), in, config));
}
inline worldgen::stages::StageInfo stub_stage_info() {
    return worldgen::stages::StageInfo{"history", &run_stub_history};
}

// The stub (7.1, 7.2). GUESS: type name and header.
inline std::unique_ptr<AwakeningHandler> make_stub() { return std::make_unique<h::StubAwakening>(); }

// S4 (7.4). GUESS: header; the constructor arguments are the spec's.
inline std::unique_ptr<TensionSource> forced_tension(std::int64_t period, std::int64_t value, std::int64_t trigger) {
    return std::make_unique<h::testing::ForcedTension>(period, value, trigger);
}

// --- Catalogue (8.2, 8.4, 8.5) -------------------------------------------------------------------

inline const EventCatalogue& compiled() { return h::compiled_event_catalogue(); }
inline EventCatalogue catalogue_of(const worldgen::data::ContentSet& c) { return h::event_catalogue(c); }
inline EventCatalogue catalogue_of_world(const db::WorldReader& r) { return h::read_event_catalogue(r); }
inline std::optional<std::string> template_problem(const KindDef& k, std::string_view text) {
    return h::template_problem(k, text);
}
// HistoryView::load returns db::Result<HistoryView> (P3-04 r1 8.5): on
// corrupt history tables an error value with code corrupt_layer and message
// `corrupt table '<table>': <detail>` (the detail wording is the
// engineer's, 16), never a FatalError; load validates every stored template
// and every reference, and accepts kinds this build does not compile.
inline h::HistoryView load_view_or_fail(const db::WorldReader& r) {
    auto v = h::HistoryView::load(r);
    if (!v.ok()) {
        throw worldgen::FatalError("test: HistoryView::load failed: " + v.error().message);
    }
    return std::move(v).value();
}
// The load error, or nullopt when the view loads.
struct ViewError {
    db::ErrorCode code;
    std::string message;
};
inline std::optional<ViewError> view_error(const db::WorldReader& r) {
    auto v = h::HistoryView::load(r);
    if (v.ok()) {
        return std::nullopt;
    }
    return ViewError{v.code(), v.error().message};
}
inline std::string render(const db::WorldReader& r, std::int64_t id) {
    const h::HistoryView v = load_view_or_fail(r);
    return h::render_event(v, id);
}
// Every event of the world rendered, ids 1 ... n (n = rows of `event`).
inline std::vector<std::string> render_all(const db::WorldReader& r, std::int64_t n) {
    const h::HistoryView v = load_view_or_fail(r);
    std::vector<std::string> out;
    for (std::int64_t id = 1; id <= n; ++id) {
        out.push_back(h::render_event(v, id));
    }
    return out;
}
// The view's read accessors (catalogue, events, names; amendment item 2)
// are the engineer's names: the tests do not use them beyond loading, so
// none is guessed here.

// --- Schema (9) ----------------------------------------------------------------------------------

inline constexpr std::int64_t schema_version() { return db::SCHEMA_VERSION; }
inline std::vector<std::string> ddl_statements() {
    std::vector<std::string> out;
    for (const std::string_view s : db::DDL_STATEMENTS) {
        out.emplace_back(s);
    }
    return out;
}
inline std::vector<std::string> required_meta_keys() {
    std::vector<std::string> out;
    for (const std::string_view s : db::REQUIRED_META_KEYS) {
        out.emplace_back(s);
    }
    return out;
}

// StageWriter row writers (9.3). GUESS: names on StageWriter. Each takes a
// test-owned row (cells in column order) and builds the library row.
inline void write_row(db::StageWriter& w, std::string_view table, const Row& c) {
    auto i = [&](std::size_t k) { return c.at(k).integer; };
    auto s = [&](std::size_t k) { return c.at(k).bytes; };
    auto o = [&](std::size_t k) -> std::optional<std::int64_t> {
        return c.at(k).type == db::ValueType::null ? std::nullopt : std::optional<std::int64_t>(c.at(k).integer);
    };
    if (table == "event_kind") {
        w.write_event_kind(db::EventKindRow{i(0), s(1), i(2), i(3), i(4), i(5), s(6)});
    } else if (table == "event_kind_role") {
        w.write_event_kind_role(db::EventKindRoleRow{i(0), i(1), s(2), i(3), i(4), i(5)});
    } else if (table == "event_kind_value") {
        w.write_event_kind_value(db::EventKindValueRow{i(0), i(1), s(2)});
    } else if (table == "phase") {
        w.write_phase(db::PhaseRow{i(0), i(1), i(2), i(3), i(4), i(5)});
    } else if (table == "event") {
        w.write_event(db::EventRow{i(0), i(1), i(2), i(3), i(4), i(5), i(6), i(7)});
    } else if (table == "event_participant") {
        w.write_event_participant(db::EventParticipantRow{i(0), i(1), i(2), i(3), i(4)});
    } else if (table == "event_cause") {
        w.write_event_cause(db::EventCauseRow{i(0), i(1)});
    } else if (table == "event_value") {
        w.write_event_value(db::EventValueRow{i(0), i(1), i(2)});
    } else if (table == "civilization") {
        w.write_civilization(db::CivilizationRow{i(0), s(1), i(2), i(3), i(4), i(5), i(6), i(7), o(8), i(9)});
    } else if (table == "settlement") {
        w.write_settlement(db::SettlementRow{i(0), s(1), i(2), i(3), i(4), i(5), i(6), i(7), i(8), i(9), i(10), i(11),
                                             o(12), i(13)});
    } else if (table == "settlement_census") {
        w.write_settlement_census(db::SettlementCensusRow{i(0), i(1), i(2), i(3), i(4), i(5)});
    } else if (table == "region_control") {
        w.write_region_control(db::RegionControlRow{i(0), i(1), o(2), i(3), i(4)});
    } else if (table == "relation") {
        w.write_relation(db::RelationRow{i(0), i(1), i(2), i(3), s(4), i(5), o(6), i(7), i(8)});
    } else if (table == "region_name") {   // P3-04 r21 23.4: write_region_name and db::RegionNameRow
        w.write_region_name(db::RegionNameRow{i(0), i(1), s(2), i(3), i(4)});
    } else {
        throw worldgen::FatalError("eng::write_row: unknown table");
    }
}

}  // namespace eng
