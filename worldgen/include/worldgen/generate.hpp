#pragma once

// World generation pipeline (spec P0-04 section 5.1): create the working
// database, run the stages in their fixed order, write meta and finalize the
// file. Used by worldgen-cli generate; later by the create-world screen.
//
// Loading settings and data (P0-03) and resolving the seed and output path
// are the caller's part (P0-04 5.1 step 1); this takes their results.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/sha256.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"

namespace worldgen {

// Everything a world depends on (P0-04 section 10): seed or phrase, world
// parameters, the content hash of the loaded data, and which optional
// stages run. Never paths, time or environment.
struct WorldParams {
    // The world seed (meta.seed). When seed_phrase is non-empty it must equal
    // phrase_seed(seed_phrase) (P0-01 7.3); generate_world checks this.
    std::uint64_t seed = 0;
    // The scrubbed phrase the seed came from (seed_from_phrase), or
    // empty for a numeric seed (meta.seed_phrase, P0-02 5.5).
    std::string seed_phrase;
    // param.map_size: 512, 1024 or 2048 (P0-02 5.5, P0-03 6).
    std::int64_t map_size = 1024;
    // meta.content_hash: content_hash() of the loaded data (P0-03 8).
    Sha256Digest content_hash{};
    // Run the P0 test-pattern stage (P0-04 5.2, --test-pattern).
    bool test_pattern = false;
};

// The names of the stages generate_world runs for params, in run order
// (P0-04 5.1 step 3). P0: empty, or {"test_pattern"}.
[[nodiscard]] std::vector<std::string_view> planned_stages(const WorldParams& params);

// Generates a world and writes it to out_utf8 (P0-04 5.1, P0-02 section 6).
// Output-location problems are returned as errors (output_exists,
// output_parent_missing, output_write_failed, output_rename_failed);
// contract violations and failures inside a stage raise FatalError
// (P0-01 4.1). On every failure path nothing but an untouched existing
// <out> remains: no <out>.partial (P0-02 section 3).
[[nodiscard]] db::Status generate_world(const WorldParams& params, std::string_view out_utf8,
                                        db::Overwrite overwrite);

}  // namespace worldgen
