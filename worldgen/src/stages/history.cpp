// Stage history (specs docs/specs/p3-04-simulation-engine.md and
// docs/specs/p3-05-peoples.md, docs/specs/p3-06-expansion-conflict.md).

#include "worldgen/stages/history.hpp"

#include <string>

#include "worldgen/core/error.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/history/renaming.hpp"

namespace worldgen::stages {

HistoryResult history_stage(const data::ContentSet& content, const history::HistoryInputs& inputs) {
    // 3.4 item 2: tuning.
    const history::EngineTuning tuning = history::engine_tuning_from(content);
    history::check_engine_tuning(tuning);
    // Item 3: catalogue data (history: report: <message>).
    const history::EventCatalogue catalogue = history::event_catalogue(content);
    // Items 2, 4 and 5 (the tuning again, then settings and chronology rows).
    (void)history::check_engine_inputs(content, inputs);
    // P3-02 r5 11.3 (r6): the stage check names_form_text, before any name
    // is drawn.
    history::check_names_form_text(content, history::name_filter_from(content));
    // P5-12 4 (P3-01 r16 5.4): the stage check renaming_rules against the
    // compiled catalogue, before the engine runs.
    const history::RenamingTuning renaming_tuning = history::renaming_tuning_from(content);
    history::check_renaming_rules(renaming_tuning, catalogue);
    // The shipped configuration (P3-04 6.4, P3-05 13.1 item 3, P3-06 8.3
    // item 3, P5-12 6): the peoples awakening and subsystem, then conflict,
    // which is the one tension source, then renaming; the stubs remain test
    // seams only.
    history::PeoplesSubsystem peoples;
    history::PeoplesAwakening awakening(peoples);
    history::ConflictSubsystem conflict(peoples, content);
    history::RenamingSubsystem renaming(peoples, renaming_tuning, catalogue);
    history::EmptyInterventionSource none;
    history::EngineConfig config;
    config.awakening = &awakening;
    config.subsystems = {&peoples, &conflict, &renaming};
    config.tension = {&conflict};
    config.interventions = &none;
    config.catalogue = &catalogue;
    HistoryResult result = history::run_engine(content, inputs, config);
    result.region_names = renaming.sorted_rows();  // P3-04 r21 23.4
    return result;
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
    for (const db::RegionNameRow& row : r.region_names) context.write_region_name(row);  // schema 8
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
