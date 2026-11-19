// The stage interface and the stage pipeline (P1-00 3, 5.1, 10.1).

#include "worldgen/stages/stage.hpp"

#include "stages/pipeline.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/stages/biomes.hpp"
#include "worldgen/stages/chronology.hpp"
#include "worldgen/stages/climate.hpp"
#include "worldgen/stages/elevation.hpp"
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
}

void StageContext::write_region_biome(const db::RegionBiomeRow& row) {
    state_->stage.write_region_biome(row);
}

void StageContext::write_race(const db::RaceRow& row) {
    state_->stage.write_race(row);
}

void StageContext::write_race_biome(const db::RaceBiomeRow& row) {
    state_->stage.write_race_biome(row);
}

void StageContext::write_culture(const db::CultureRow& row) {
    state_->stage.write_culture(row);
}

void StageContext::write_god(const db::GodRow& row) {
    state_->stage.write_god(row);
}

void StageContext::write_age(const db::AgeRow& row) {
    state_->stage.write_age(row);
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
        StageContext::State{*this, writer_.begin_stage(stage.name), {}, {}, {}}));
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
}

}  // namespace detail

// --- Stage catalogue and limit (P1-00 3, 10.1) ------------------------------------------

std::span<const StageInfo> implemented_stages() noexcept {
    // Each stage is added here as it is implemented, in the order of
    // STAGE_NAMES (P1-00 3, P3-00 5).
    static constexpr std::array<StageInfo, 7> STAGES = {
        StageInfo{ELEVATION_STAGE, &run_elevation},    // P1-01
        StageInfo{HYDROLOGY_STAGE, &run_hydrology},    // P1-02
        StageInfo{CLIMATE_STAGE, &run_climate},        // P1-03
        StageInfo{BIOMES_STAGE, &run_biomes},          // P1-04
        StageInfo{RESOURCES_STAGE, &run_resources},    // P1-05
        StageInfo{REGIONS_STAGE, &run_regions},        // P1-06
        StageInfo{CHRONOLOGY_STAGE, &run_chronology},  // P3-03
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
