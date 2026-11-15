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
    Pipeline(db::WorldWriter& writer, const data::ContentSet& content, std::int64_t map_size, std::uint32_t threads);

    // Runs one stage (P0-02 6 step 3): begin_stage(stage.name), the entry
    // point with a fresh StageContext, commit. If the entry point throws,
    // the transaction is rolled back (the StageWriter is destroyed) and the
    // exception propagates.
    void run(const StageInfo& stage);

    [[nodiscard]] const std::vector<StoredLayer>& layers() const noexcept { return layers_; }
    [[nodiscard]] db::WorldWriter& writer() noexcept { return writer_; }
    [[nodiscard]] const data::ContentSet& content() const noexcept { return content_; }
    [[nodiscard]] std::int64_t map_size() const noexcept { return map_size_; }
    [[nodiscard]] std::uint32_t threads() const noexcept { return threads_; }

private:
    db::WorldWriter& writer_;
    const data::ContentSet& content_;
    std::int64_t map_size_;
    std::uint32_t threads_;
    std::vector<StoredLayer> layers_;  // committed layers, in id order
};

}  // namespace worldgen::stages::detail
