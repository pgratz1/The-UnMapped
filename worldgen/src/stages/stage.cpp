// The stage interface and the stage pipeline (P1-00 3, 5.1, 10.1).

#include "worldgen/stages/stage.hpp"

#include "stages/pipeline.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/stages/biomes.hpp"
#include "worldgen/stages/chronology.hpp"
#include "worldgen/stages/climate.hpp"
#include "worldgen/stages/elevation.hpp"
#include "worldgen/stages/history.hpp"
#include "worldgen/stages/hydrology.hpp"
#include "worldgen/stages/regions.hpp"
#include "worldgen/stages/resources.hpp"

namespace worldgen::stages {

// --- StageContext ------------------------------------------------------------------

struct StageContext::State {
    detail::Pipeline& pipeline;
    db::StageWriter stage;
    std::vector<detail::StoredLayer> written;  // this stage's layers, in writing order
    std::vector<db::PassRow> written_passes;   // this stage's pass rows, in writing order
    std::vector<db::RegionRow> written_regions;  // this stage's region rows, in writing order
    detail::Pipeline::CommittedRows written_rows;  // this stage's rows of the tables history reads
};

StageContext::StageContext(std::unique_ptr<State> state) : state_(std::move(state)) {}
StageContext::~StageContext() = default;

std::string_view StageContext::stage_name() const noexcept {
    return state_->stage.name();
}

std::uint64_t StageContext::seed() const noexcept {
    return state_->pipeline.writer().seed();
}

std::int64_t StageContext::map_size() const noexcept {
    return state_->pipeline.map_size();
}

std::uint32_t StageContext::threads() const noexcept {
    return state_->pipeline.threads();
}

const data::ContentSet& StageContext::content() const noexcept {
    return state_->pipeline.content();
}

const history::HistoryParams& StageContext::history_params() const noexcept {
    return state_->pipeline.history_params();
}

const db::LayerGrid* StageContext::find_layer(std::string_view name) const noexcept {
    for (const detail::StoredLayer& layer : state_->pipeline.layers()) {
        if (layer.name == name) {
            return &layer.grid;
        }
    }
    return nullptr;
}

const db::LayerGrid& StageContext::layer(std::string_view name) const {
    const db::LayerGrid* grid = find_layer(name);
    if (grid == nullptr) {
        fatal("StageContext::layer: no earlier stage wrote the layer this stage reads");
    }
    return *grid;
}

void StageContext::write_layer(std::string_view name, db::LayerGrid grid) {
    db::WorldWriter& writer = state_->pipeline.writer();
    state_->stage.write_layer(writer.next_layer_id(), name, grid, db::Codec::delta_varint);
    state_->written.push_back(detail::StoredLayer{std::string(name), std::move(grid)});
}

std::span<const db::PassRow> StageContext::passes() const noexcept {
    return state_->pipeline.passes();
}

void StageContext::write_pass(const db::PassRow& row) {
    state_->stage.write_pass(row);
    state_->written_passes.push_back(row);
}

void StageContext::write_biome(const db::BiomeRow& row) {
    state_->stage.write_biome(row);
    state_->written_rows.biomes.push_back(row);
}

std::span<const db::BiomeRow> StageContext::biomes() const noexcept {
    return state_->pipeline.rows().biomes;
}

void StageContext::write_climate(const db::ClimateRow& row) {
    state_->stage.write_climate(row);
}

std::span<const db::RegionRow> StageContext::regions() const noexcept {
    return state_->pipeline.regions();
}

void StageContext::write_region(const db::RegionRow& row) {
    state_->stage.write_region(row);
    state_->written_regions.push_back(row);
}

void StageContext::write_region_border(const db::RegionBorderRow& row) {
    state_->stage.write_region_border(row);
    state_->written_rows.region_borders.push_back(row);
}

void StageContext::write_region_biome(const db::RegionBiomeRow& row) {
    state_->stage.write_region_biome(row);
    state_->written_rows.region_biomes.push_back(row);
}

void StageContext::write_race(const db::RaceRow& row) {
    state_->stage.write_race(row);
    state_->written_rows.races.push_back(row);
}

void StageContext::write_race_biome(const db::RaceBiomeRow& row) {
    state_->stage.write_race_biome(row);
    state_->written_rows.race_biomes.push_back(row);
}

void StageContext::write_culture(const db::CultureRow& row) {
    state_->stage.write_culture(row);
    state_->written_rows.cultures.push_back(row);
}

void StageContext::write_god(const db::GodRow& row) {
    state_->stage.write_god(row);
    state_->written_rows.gods.push_back(row);
}

void StageContext::write_age(const db::AgeRow& row) {
    state_->stage.write_age(row);
    state_->written_rows.ages.push_back(row);
}

std::span<const db::RegionBorderRow> StageContext::region_borders() const noexcept {
    return state_->pipeline.rows().region_borders;
}

std::span<const db::RegionBiomeRow> StageContext::region_biomes() const noexcept {
    return state_->pipeline.rows().region_biomes;
}

std::span<const db::RaceRow> StageContext::races() const noexcept {
    return state_->pipeline.rows().races;
}

std::span<const db::RaceBiomeRow> StageContext::race_biomes() const noexcept {
    return state_->pipeline.rows().race_biomes;
}

std::span<const db::CultureRow> StageContext::cultures() const noexcept {
    return state_->pipeline.rows().cultures;
}

std::span<const db::GodRow> StageContext::gods() const noexcept {
    return state_->pipeline.rows().gods;
}

std::span<const db::AgeRow> StageContext::ages() const noexcept {
    return state_->pipeline.rows().ages;
}

void StageContext::write_event_kind(const db::EventKindRow& row) {
    state_->stage.write_event_kind(row);
}

void StageContext::write_event_kind_role(const db::EventKindRoleRow& row) {
    state_->stage.write_event_kind_role(row);
}

void StageContext::write_event_kind_value(const db::EventKindValueRow& row) {
    state_->stage.write_event_kind_value(row);
}

void StageContext::write_phase(const db::PhaseRow& row) {
    state_->stage.write_phase(row);
}

void StageContext::write_event(const db::EventRow& row) {
    state_->stage.write_event(row);
}

void StageContext::write_event_participant(const db::EventParticipantRow& row) {
    state_->stage.write_event_participant(row);
}

void StageContext::write_event_cause(const db::EventCauseRow& row) {
    state_->stage.write_event_cause(row);
}

void StageContext::write_event_value(const db::EventValueRow& row) {
    state_->stage.write_event_value(row);
}

void StageContext::write_civilization(const db::CivilizationRow& row) {
    state_->stage.write_civilization(row);
}

void StageContext::write_settlement(const db::SettlementRow& row) {
    state_->stage.write_settlement(row);
}

void StageContext::write_settlement_census(const db::SettlementCensusRow& row) {
    state_->stage.write_settlement_census(row);
}

void StageContext::write_region_control(const db::RegionControlRow& row) {
    state_->stage.write_region_control(row);
}

void StageContext::write_relation(const db::RelationRow& row) {
    state_->stage.write_relation(row);
}

void StageContext::write_region_name(const db::RegionNameRow& row) {
    state_->stage.write_region_name(row);
}

void StageContext::write_beast(const db::BeastRow& row) {
    state_->stage.write_beast(row);
}

void StageContext::write_monster_kind(const db::MonsterKindRow& row) {
    state_->stage.write_monster_kind(row);
}

void StageContext::write_beast_population(const db::BeastPopulationRow& row) {
    state_->stage.write_beast_population(row);
}

void StageContext::write_monster(const db::MonsterRow& row) {
    state_->stage.write_monster(row);
}

void StageContext::write_site(const db::SiteRow& row) {
    state_->stage.write_site(row);
}

void StageContext::write_race_relation(const db::RaceRelationRow& row) {
    state_->stage.write_race_relation(row);
}

void StageContext::write_settlement_race(const db::SettlementRaceRow& row) {
    state_->stage.write_settlement_race(row);
}

void StageContext::write_figure(const db::FigureRow& row) {
    state_->stage.write_figure(row);
}

void StageContext::write_book(const db::BookRow& row) {
    state_->stage.write_book(row);
}

void StageContext::write_faction(const db::FactionRow& row) {
    state_->stage.write_faction(row);
}

void StageContext::write_artifact_kind(const db::ArtifactKindRow& row) {
    state_->stage.write_artifact_kind(row);
}

void StageContext::write_artifact(const db::ArtifactRow& row) {
    state_->stage.write_artifact(row);
}

void StageContext::write_artifact_holding(const db::ArtifactHoldingRow& row) {
    state_->stage.write_artifact_holding(row);
}

// --- Pipeline ------------------------------------------------------------------------

namespace detail {

Pipeline::Pipeline(db::WorldWriter& writer, const data::ContentSet& content, std::int64_t map_size,
                   std::uint32_t threads, const history::HistoryParams& history)
    : writer_(writer), content_(content), map_size_(map_size), threads_(threads), history_(history) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("generate_world: thread count outside [1, 256]");
    }
}

void Pipeline::run(const StageInfo& stage) {
    if (stage.run == nullptr) {
        fatal("generate_world: a stage has no entry point");
    }
    StageContext context(std::make_unique<StageContext::State>(
        StageContext::State{*this, writer_.begin_stage(stage.name), {}, {}, {}, {}}));
    stage.run(context);
    context.state_->stage.commit();
    for (StoredLayer& layer : context.state_->written) {
        layers_.push_back(std::move(layer));
    }
    context.state_->written.clear();
    for (db::PassRow& row : context.state_->written_passes) {
        passes_.push_back(std::move(row));
    }
    context.state_->written_passes.clear();
    for (db::RegionRow& row : context.state_->written_regions) {
        regions_.push_back(std::move(row));
    }
    context.state_->written_regions.clear();
    rows_.append(std::move(context.state_->written_rows));
}

void Pipeline::CommittedRows::append(CommittedRows&& more) {
    const auto move_all = [](auto& to, auto& from) {
        for (auto& row : from) {
            to.push_back(std::move(row));
        }
        from.clear();
    };
    move_all(biomes, more.biomes);
    move_all(region_borders, more.region_borders);
    move_all(region_biomes, more.region_biomes);
    move_all(races, more.races);
    move_all(race_biomes, more.race_biomes);
    move_all(cultures, more.cultures);
    move_all(gods, more.gods);
    move_all(ages, more.ages);
}

}  // namespace detail

// --- Stage catalogue and limit (P1-00 3, 10.1) ------------------------------------------

std::span<const StageInfo> implemented_stages() noexcept {
    // Each stage is added here as it is implemented, in the order of
    // STAGE_NAMES (P1-00 3, P3-00 5).
    static constexpr std::array<StageInfo, 8> STAGES = {
        StageInfo{ELEVATION_STAGE, &run_elevation},    // P1-01
        StageInfo{HYDROLOGY_STAGE, &run_hydrology},    // P1-02
        StageInfo{CLIMATE_STAGE, &run_climate},        // P1-03
        StageInfo{BIOMES_STAGE, &run_biomes},          // P1-04
        StageInfo{RESOURCES_STAGE, &run_resources},    // P1-05
        StageInfo{REGIONS_STAGE, &run_regions},        // P1-06
        StageInfo{CHRONOLOGY_STAGE, &run_chronology},  // P3-03
        StageInfo{HISTORY_STAGE, &run_history},        // P3-04
    };
    return STAGES;
}

StageLimitError check_stage_limit(std::string_view until) noexcept {
    if (until == UNTIL_NONE) {
        return StageLimitError::none;
    }
    for (const StageInfo& s : implemented_stages()) {
        if (s.name == until) {
            return StageLimitError::none;
        }
    }
    for (const std::string_view name : STAGE_NAMES) {
        if (name == until) {
            return StageLimitError::not_implemented;
        }
    }
    return StageLimitError::unknown;
}

std::vector<StageInfo> stages_until(const std::optional<std::string>& until) {
    const std::span<const StageInfo> all = implemented_stages();
    std::vector<StageInfo> out;
    if (!until) {
        out.assign(all.begin(), all.end());
        return out;
    }
    if (check_stage_limit(*until) != StageLimitError::none) {
        fatal("stages_until: not 'none' or an implemented stage name");
    }
    if (*until == UNTIL_NONE) {
        return out;
    }
    for (const StageInfo& s : all) {
        out.push_back(s);
        if (s.name == *until) {
            break;
        }
    }
    return out;
}

}  // namespace worldgen::stages
