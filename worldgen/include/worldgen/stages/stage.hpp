#pragma once

// The stage interface of the generation pipeline (spec P1-00 sections 3,
// 5.1 and 10.1; P0-04 amendment 16 (a); P0-02 section 6 step 3).
//
// Stages run in a fixed order, each in its own stage transaction. A stage is
// a function of one StageContext. Through it, and only through it, the
// stage sees what a stage may depend on (P1-00 5.1): the seed, the world
// parameters, the loaded content, and the stored layers written by earlier
// stages, exactly as written (the grids that were encoded into the world,
// which decode to the same values). It writes its own layers through the
// context, and the pipeline commits its transaction when it returns. A
// stage is never handed another stage's working arrays: the only grids a
// context exposes are committed layers.
//
// The thread count (P1-00 D10) is in the context for scheduling parallel
// steps; it must never change a result.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/params.hpp"

namespace worldgen::stages {

namespace detail {
class Pipeline;
}

// The context of one stage run (P1-00 5.1). Created by the pipeline for one
// stage and destroyed when the stage returns; not copyable or movable.
class StageContext {
public:
    StageContext(const StageContext&) = delete;
    StageContext& operator=(const StageContext&) = delete;
    StageContext(StageContext&&) = delete;
    StageContext& operator=(StageContext&&) = delete;
    ~StageContext();

    // The stage's name, as recorded in meta.stages (P1-00 3).
    [[nodiscard]] std::string_view stage_name() const noexcept;
    // meta.seed: the world seed every stream of the stage derives from
    // (P0-01 7.3).
    [[nodiscard]] std::uint64_t seed() const noexcept;
    // param.map_size: the map side N (P1-00 2.1).
    [[nodiscard]] std::int64_t map_size() const noexcept;
    // Threads for parallel steps, 1 to 256 (P1-00 D10). Scheduling only.
    [[nodiscard]] std::uint32_t threads() const noexcept;
    // The loaded content (P0-03): the tuning definitions and data kinds.
    [[nodiscard]] const data::ContentSet& content() const noexcept;
    // The history settings of the world (P3-03 4.1 step 4): exactly the
    // values generate_world records in meta.
    [[nodiscard]] const history::HistoryParams& history_params() const noexcept;

    // A layer written by an earlier stage of this run, by name, as stored
    // (P1-00 5.1), or nullptr if no earlier stage wrote it. Layers this
    // stage has written itself are not returned.
    [[nodiscard]] const db::LayerGrid* find_layer(std::string_view name) const noexcept;
    // As find_layer, but FatalError if absent (a stage's declared input is
    // missing: the stage list or a fixture world is wrong).
    [[nodiscard]] const db::LayerGrid& layer(std::string_view name) const;

    // The rows of table `pass` written by earlier stages of this run, in id
    // order, exactly as written (P1-06 2: the regions stage reads them).
    // Rows this stage has written itself are not returned.
    [[nodiscard]] std::span<const db::PassRow> passes() const noexcept;

    // The rows of table `region` written by earlier stages of this run, in
    // id order, exactly as written (P3-03 3: the chronology stage reads the
    // names). Rows this stage has written itself are not returned.
    [[nodiscard]] std::span<const db::RegionRow> regions() const noexcept;

    // The rows of tables region_border, region_biome, race, race_biome,
    // culture, god and age written by earlier stages of this run, in key
    // order, exactly as written (P3-04 3.3: the history stage reads them).
    // Rows this stage has written itself are not returned.
    [[nodiscard]] std::span<const db::RegionBorderRow> region_borders() const noexcept;
    // The rows of table `biome` written by earlier stages of this run, in id
    // order, exactly as written (P3-05 3.2: the history stage reads them).
    [[nodiscard]] std::span<const db::BiomeRow> biomes() const noexcept;
    [[nodiscard]] std::span<const db::RegionBiomeRow> region_biomes() const noexcept;
    [[nodiscard]] std::span<const db::RaceRow> races() const noexcept;
    [[nodiscard]] std::span<const db::RaceBiomeRow> race_biomes() const noexcept;
    [[nodiscard]] std::span<const db::CultureRow> cultures() const noexcept;
    [[nodiscard]] std::span<const db::GodRow> gods() const noexcept;
    [[nodiscard]] std::span<const db::AgeRow> ages() const noexcept;

    // Writes one of this stage's layers with the next layer id and codec 1
    // (P1-00 4 rules 1 and 2; P0-02 5.6). Layers are written in the order of
    // these calls. FatalError as StageWriter::write_layer (invalid name or
    // grid, a name already used), and if called after the stage returned.
    void write_layer(std::string_view name, db::LayerGrid grid);

    // Writes one row of table `pass` (P1-01 9.1). Ids are 1, 2, 3, ... in
    // the order of these calls. FatalError as StageWriter::write_pass.
    void write_pass(const db::PassRow& row);

    // Writes one row of table `biome` (P1-04 B6), in ascending id.
    // FatalError as StageWriter::write_biome.
    void write_biome(const db::BiomeRow& row);

    // Writes the row of table `climate` (P1-03 C7). FatalError as
    // StageWriter::write_climate.
    void write_climate(const db::ClimateRow& row);

    // Write one row of tables `region`, `region_border` and `region_biome`
    // (P1-06 RG14), each in ascending key. FatalError as the StageWriter
    // functions of the same names.
    void write_region(const db::RegionRow& row);
    void write_region_border(const db::RegionBorderRow& row);
    void write_region_biome(const db::RegionBiomeRow& row);

    // Write one row of tables `race`, `race_biome`, `culture`, `god` and
    // `age` (P3-03 5.3), each in ascending key. FatalError as the
    // StageWriter functions of the same names.
    void write_race(const db::RaceRow& row);
    void write_race_biome(const db::RaceBiomeRow& row);
    void write_culture(const db::CultureRow& row);
    void write_god(const db::GodRow& row);
    void write_age(const db::AgeRow& row);

    // Write one row of each of the thirteen history tables (P3-04 9.3),
    // each in ascending key. FatalError as the StageWriter functions of the
    // same names.
    void write_event_kind(const db::EventKindRow& row);
    void write_event_kind_role(const db::EventKindRoleRow& row);
    void write_event_kind_value(const db::EventKindValueRow& row);
    void write_phase(const db::PhaseRow& row);
    void write_event(const db::EventRow& row);
    void write_event_participant(const db::EventParticipantRow& row);
    void write_event_cause(const db::EventCauseRow& row);
    void write_event_value(const db::EventValueRow& row);
    void write_civilization(const db::CivilizationRow& row);
    void write_settlement(const db::SettlementRow& row);
    void write_settlement_census(const db::SettlementCensusRow& row);
    void write_region_control(const db::RegionControlRow& row);
    void write_relation(const db::RelationRow& row);
    void write_region_name(const db::RegionNameRow& row);  // schema 8 (P5-12)
    // schema 9 (P5-02)
    void write_beast(const db::BeastRow& row);
    void write_monster_kind(const db::MonsterKindRow& row);
    void write_beast_population(const db::BeastPopulationRow& row);
    void write_monster(const db::MonsterRow& row);
    void write_site(const db::SiteRow& row);

private:
    friend class detail::Pipeline;
    struct State;
    explicit StageContext(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
};

// A stage entry point: one function of the context (P1-00 3). It raises
// FatalError (P0-01 4.1) on any contract violation; the stage's transaction
// is then rolled back and generation fails.
using StageEntry = void (*)(StageContext& context);

// A stage of the pipeline: its name and entry point.
struct StageInfo {
    std::string_view name;
    StageEntry run = nullptr;
};

// The names of the stages of P1-00 section 3 and P3-00 5, in run order,
// implemented or not: the six geography stages, then `chronology` (P3-03)
// and `history` (P3-04 to P3-06).
inline constexpr std::array<std::string_view, 8> STAGE_NAMES = {
    "elevation", "hydrology", "climate", "biomes", "resources", "regions", "chronology", "history",
};

// The implemented stages, in run order (P1-00 3, 10.1; P3-03 4.3; P3-04
// 9.5): the stages generate runs by default. Each implemented stage is the
// next one of STAGE_NAMES: `elevation` (P1-01), `hydrology` (P1-02),
// `climate` (P1-03), `biomes` (P1-04), `resources` (P1-05), `regions`
// (P1-06), `chronology` (P3-03) and `history` (P3-04, the engine with stub
// subsystems).
[[nodiscard]] std::span<const StageInfo> implemented_stages() noexcept;

// The former name of implemented_stages(), from before the history stages.
[[nodiscard]] inline std::span<const StageInfo> implemented_geography_stages() noexcept {
    return implemented_stages();
}

// --- Stage limit (P1-00 10.1, --until) ------------------------------------------

// `--until none`: run no stage (the P0 empty world).
inline constexpr std::string_view UNTIL_NONE = "none";

// Why a stage limit is not usable (P1-00 10.1: each is a usage error, exit 2).
enum class StageLimitError : std::uint8_t {
    none = 0,         // usable: "none" or an implemented stage name
    not_implemented,  // a name of STAGE_NAMES with no implementation yet
    unknown,          // not a stage name at all
};

// Checks a --until value (P1-00 10.1).
[[nodiscard]] StageLimitError check_stage_limit(std::string_view until) noexcept;

// The stages to run, in order, for a limit (P1-00 10.1): every implemented
// stage when `until` is std::nullopt; none for UNTIL_NONE; otherwise the
// implemented stages up to and including `until`. FatalError if
// check_stage_limit(until) is not none.
[[nodiscard]] std::vector<StageInfo> stages_until(const std::optional<std::string>& until);

// The former name of stages_until(), from before the history stages.
[[nodiscard]] inline std::vector<StageInfo> geography_stages_until(const std::optional<std::string>& until) {
    return stages_until(until);
}

}  // namespace worldgen::stages
