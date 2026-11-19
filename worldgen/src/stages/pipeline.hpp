#pragma once

// Private: runs stages in their transactions and keeps the layers they
// committed (P1-00 3, 5.1; P0-02 6 step 3).

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/params.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages::detail {

// One committed layer: its name and the grid as written.
struct StoredLayer {
    std::string name;
    db::LayerGrid grid;
};

class Pipeline {
public:
    // The writer must have its seed and param.map_size set; content, the
    // writer and the pipeline must outlive every run().
    // history is the world's HistoryParams (StageContext::history_params);
    // it must outlive every run().
    Pipeline(db::WorldWriter& writer, const data::ContentSet& content, std::int64_t map_size, std::uint32_t threads,
             const history::HistoryParams& history);

    // Runs one stage (P0-02 6 step 3): begin_stage(stage.name), the entry
    // point with a fresh StageContext, commit. If the entry point throws,
    // the transaction is rolled back (the StageWriter is destroyed) and the
    // exception propagates.
    void run(const StageInfo& stage);

    [[nodiscard]] const std::vector<StoredLayer>& layers() const noexcept { return layers_; }
    // The committed rows of table pass, in id order.
    [[nodiscard]] const std::vector<db::PassRow>& passes() const noexcept { return passes_; }
    // The committed rows of table region, in id order.
    [[nodiscard]] const std::vector<db::RegionRow>& regions() const noexcept { return regions_; }
    // The committed rows of the region and chronology tables (P3-04 3.3),
    // in key order.
    struct CommittedRows {
        std::vector<db::BiomeRow> biomes;  // P3-05 3.2
        std::vector<db::RegionBorderRow> region_borders;
        std::vector<db::RegionBiomeRow> region_biomes;
        std::vector<db::RaceRow> races;
        std::vector<db::RaceBiomeRow> race_biomes;
        std::vector<db::CultureRow> cultures;
        std::vector<db::GodRow> gods;
        std::vector<db::AgeRow> ages;

        // Appends every row of `more` (a stage's rows, in writing order).
        void append(CommittedRows&& more);
    };
    [[nodiscard]] const CommittedRows& rows() const noexcept { return rows_; }
    [[nodiscard]] const history::HistoryParams& history_params() const noexcept { return history_; }
    [[nodiscard]] db::WorldWriter& writer() noexcept { return writer_; }
    [[nodiscard]] const data::ContentSet& content() const noexcept { return content_; }
    [[nodiscard]] std::int64_t map_size() const noexcept { return map_size_; }
    [[nodiscard]] std::uint32_t threads() const noexcept { return threads_; }

private:
    db::WorldWriter& writer_;
    const data::ContentSet& content_;
    std::int64_t map_size_;
    std::uint32_t threads_;
    const history::HistoryParams& history_;
    std::vector<StoredLayer> layers_;  // committed layers, in id order
    std::vector<db::PassRow> passes_;  // committed rows of table pass, in id order
    std::vector<db::RegionRow> regions_;  // committed rows of table region, in id order
    CommittedRows rows_;                  // committed rows of the tables history reads
};

}  // namespace worldgen::stages::detail
