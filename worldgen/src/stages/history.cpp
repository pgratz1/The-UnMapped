// Stage P3-04 history (spec docs/specs/p3-04-simulation-engine.md).

#include "worldgen/stages/history.hpp"

#include <string>

#include "worldgen/core/error.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/stubs.hpp"

namespace worldgen::stages {

HistoryResult history_stage(const data::ContentSet& content, const history::HistoryInputs& inputs) {
    // 3.4 item 2: tuning.
    const history::EngineTuning tuning = history::engine_tuning_from(content);
    history::check_engine_tuning(tuning);
    // Item 3: catalogue data (history: report: <message>).
    const history::EventCatalogue catalogue = history::event_catalogue(content);
    // Items 2, 4 and 5 (the tuning again, then settings and chronology rows).
    (void)history::check_engine_inputs(content, inputs);
    // The shipped configuration (6.4).
    history::StubAwakening stub;
    history::EmptyInterventionSource none;
    history::EngineConfig config;
    config.awakening = &stub;
    config.interventions = &none;
    config.catalogue = &catalogue;
    return history::run_engine(content, inputs, config);
}

void write_history_result(StageContext& context, const HistoryResult& r) {
    for (const db::EventKindRow& row : r.event_kinds) context.write_event_kind(row);
    for (const db::EventKindRoleRow& row : r.event_kind_roles) context.write_event_kind_role(row);
    for (const db::EventKindValueRow& row : r.event_kind_values) context.write_event_kind_value(row);
    for (const db::PhaseRow& row : r.phases) context.write_phase(row);
    for (const db::EventRow& row : r.events) context.write_event(row);
    for (const db::EventParticipantRow& row : r.event_participants) context.write_event_participant(row);
    for (const db::EventCauseRow& row : r.event_causes) context.write_event_cause(row);
    for (const db::EventValueRow& row : r.event_values) context.write_event_value(row);
    for (const db::CivilizationRow& row : r.civilizations) context.write_civilization(row);
    for (const db::SettlementRow& row : r.settlements) context.write_settlement(row);
    for (const db::SettlementCensusRow& row : r.settlement_census) context.write_settlement_census(row);
    for (const db::RegionControlRow& row : r.region_control) context.write_region_control(row);
    for (const db::RelationRow& row : r.relations) context.write_relation(row);
}

void run_history(StageContext& context) {
    if (context.regions().empty()) {
        throw FatalError("history: input precondition: no region rows");
    }
    if (context.ages().empty()) {
        throw FatalError("history: input precondition: no age rows");
    }
    const HistoryResult result = history_stage(context.content(), history::history_inputs_from(context));
    write_history_result(context, result);
}

}  // namespace worldgen::stages
