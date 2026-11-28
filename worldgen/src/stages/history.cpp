// Stage history (specs docs/specs/p3-04-simulation-engine.md and
// docs/specs/p3-05-peoples.md, docs/specs/p3-06-expansion-conflict.md).

#include "worldgen/stages/history.hpp"

#include <string>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/alliances.hpp"
#include "worldgen/history/artifacts.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/figures.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/history/pantheon.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/history/relations.hpp"
#include "worldgen/history/renaming.hpp"

namespace worldgen::stages {

namespace {

// P5-02 10.4: every participant of entity type monster or site names a row
// of table monster or site (ids are 1, 2, ...).
void check_monster_participants(const HistoryResult& r) {
    const auto monsters = static_cast<std::int64_t>(r.monsters.size());
    const auto sites = static_cast<std::int64_t>(r.sites.size());
    const auto figures = static_cast<std::int64_t>(r.figures.size());  // P5-06 8.5
    const auto factions = static_cast<std::int64_t>(r.factions.size());  // P5-08 8.1
    const auto artifacts = static_cast<std::int64_t>(r.artifacts.size());  // P5-11 8.1
    for (const db::EventParticipantRow& p : r.event_participants) {
        const bool monster = p.entity_type == static_cast<std::int64_t>(history::EntityType::monster);
        const bool site = p.entity_type == static_cast<std::int64_t>(history::EntityType::site);
        const bool figure = p.entity_type == static_cast<std::int64_t>(history::EntityType::figure);
        const bool faction = p.entity_type == static_cast<std::int64_t>(history::EntityType::faction);
        const bool artifact = p.entity_type == static_cast<std::int64_t>(history::EntityType::artifact);
        const std::int64_t n =
            monster ? monsters : (site ? sites : (figure ? figures : (faction ? factions : artifacts)));
        if ((monster || site || figure || faction || artifact) && (p.entity_id < 1 || p.entity_id > n)) {
            const char* what = monster ? "monster "
                                       : (site ? "site " : (figure ? "figure " : (faction ? "faction " : "artifact ")));
            const db::EventRow& e = r.events[static_cast<std::size_t>(p.event - 1)];
            for (const db::EventKindRow& row : r.event_kinds) {
                if (row.id == e.kind) {
                    throw FatalError("history: event " + db::format_i64(p.event) + " (" + row.key + "): " + what +
                                     db::format_i64(p.entity_id) + " has no row");
                }
            }
            throw FatalError("history: event " + db::format_i64(p.event) + ": " + what + db::format_i64(p.entity_id) +
                             " has no row");
        }
    }
}

}  // namespace

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
    // P5-02 6.1, 7, 9.3: the monsters run after renaming and are the second
    // tension source; peoples reads their wild view, renaming their names.
    history::MonstersSubsystem monsters(peoples, content);
    history::RenamingSubsystem renaming(peoples, renaming_tuning, catalogue);
    peoples.set_wild(&monsters);
    renaming.set_monsters(&monsters);
    monsters.set_region_names(&renaming);  // P5-03 12.2
    // P5-03 6.1, 6.6 item 3: the relations run after the monsters; they are
    // peoples' ledger and conflict's disposition view.
    // P5-03a 8 (A2): the dispositions are valid from construction.
    history::RelationsSubsystem relations(peoples, conflict, content, inputs);
    peoples.set_ledger(&relations);
    conflict.set_dispositions(&relations);
    // P5-04 6.1, 6.10 item 3: the alliances run after the relations; they are
    // conflict's alliance view and open mines through the monsters' sites.
    history::AlliancesSubsystem alliances(peoples, conflict, relations, content);
    conflict.set_alliances(&alliances);
    alliances.set_sites(&monsters);
    // P5-06 6.13 item 3: the figures run after the alliances and are every
    // other subsystem's figure view.
    history::FiguresSubsystem figures(peoples, content);
    conflict.set_figures(&figures);
    peoples.set_figures(&figures);
    monsters.set_figures(&figures);
    monsters.set_conflict(&conflict);  // P5-07 7.3: the flight seam
    monsters.set_alliances(&alliances);  // P5-07 r2 7.6: the allies' aid at a waking
    monsters.set_relations(&relations);
    alliances.set_figures(&figures);
    figures.set_sites(&monsters);
    figures.set_conflict(&conflict);
    // P5-08 6.12 item 3: the pantheon runs after the figures; it is the
    // third tension source, the intervention source and applier, and the
    // divine view of peoples and conflict.
    history::PantheonSubsystem pantheon(peoples, content);
    pantheon.set_sites(&monsters);
    pantheon.set_figures(&figures);
    pantheon.set_alliances(&alliances);
    pantheon.set_conflict(&conflict);
    peoples.set_divine(&pantheon);
    conflict.set_divine(&pantheon);
    // P5-11 6.10 item 3: the artifacts run last; they are conflict's and the
    // monsters' artifact view.
    history::ArtifactsSubsystem artifacts(peoples, content);
    conflict.set_artifacts(&artifacts);
    monsters.set_artifacts(&artifacts);
    artifacts.set_figures(&figures);
    artifacts.set_alliances(&alliances);
    artifacts.set_sites(&monsters);
    artifacts.set_divine(&pantheon);  // P5-11 r4 7.9
    history::EngineConfig config;
    config.awakening = &awakening;
    config.subsystems = {&peoples,   &conflict,  &renaming, &monsters, &relations,
                         &alliances, &figures,   &pantheon, &artifacts};
    config.tension = {&conflict, &monsters, &pantheon};
    config.interventions = &pantheon;
    config.applier = &pantheon;
    config.catalogue = &catalogue;
    HistoryResult result = history::run_engine(content, inputs, config);
    result.region_names = renaming.sorted_rows();  // P3-04 r21 23.4
    monsters.finish(inputs.params.span);  // P5-02 6.2.2: the rows of year E - 1
    result.beasts = monsters.beast_rows();
    result.monster_kinds = monsters.monster_kind_rows();
    result.beast_populations = monsters.beast_population_rows();
    result.monsters = monsters.monster_rows();
    result.sites = monsters.site_rows();
    relations.finish(inputs.params.span);  // P5-03 6.7: the rows of year E - 1
    result.race_relations = relations.race_relation_rows();
    result.settlement_races = relations.settlement_race_rows();
    result.figures = figures.figure_rows();  // P5-06 9.1, 9.3
    result.books = figures.book_rows();
    result.factions = pantheon.faction_rows();  // P5-08 9
    result.artifact_kinds = artifacts.artifact_kind_rows();  // P5-11 9
    result.artifacts = artifacts.artifact_rows();
    result.artifact_holdings = artifacts.artifact_holding_rows();
    check_monster_participants(result);  // P5-02 10.4, P5-06 8.5
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
    // schema 9 (P5-02 11.1)
    for (const db::BeastRow& row : r.beasts) context.write_beast(row);
    for (const db::MonsterKindRow& row : r.monster_kinds) context.write_monster_kind(row);
    for (const db::BeastPopulationRow& row : r.beast_populations) context.write_beast_population(row);
    for (const db::MonsterRow& row : r.monsters) context.write_monster(row);
    for (const db::SiteRow& row : r.sites) context.write_site(row);
    // schema 10 (P5-03 16.1)
    for (const db::RaceRelationRow& row : r.race_relations) context.write_race_relation(row);
    for (const db::SettlementRaceRow& row : r.settlement_races) context.write_settlement_race(row);
    // schema 12 (P5-06 9.1, 9.3)
    for (const db::FigureRow& row : r.figures) context.write_figure(row);
    for (const db::BookRow& row : r.books) context.write_book(row);
    for (const db::FactionRow& row : r.factions) context.write_faction(row);  // schema 14 (P5-08 9)
    // schema 15 (P5-11 9)
    for (const db::ArtifactKindRow& row : r.artifact_kinds) context.write_artifact_kind(row);
    for (const db::ArtifactRow& row : r.artifacts) context.write_artifact(row);
    for (const db::ArtifactHoldingRow& row : r.artifact_holdings) context.write_artifact_holding(row);
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
