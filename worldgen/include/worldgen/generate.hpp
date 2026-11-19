#pragma once

// World generation pipeline (spec P0-04 section 5.1): create the working
// database, run the stages in their fixed order, write meta and finalize the
// file. Used by worldgen-cli generate; later by the create-world screen.
//
// Loading settings and data (P0-03) and resolving the seed and output path
// are the caller's part (P0-04 5.1 step 1); this takes their results.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/params.hpp"

namespace worldgen {

// Everything a world depends on (P0-04 section 10): seed or phrase, world
// parameters, the content hash of the loaded data, and which stages run;
// plus the thread count, which is not part of what a world depends on
// (P1-00 D10, P0-04 amendment 16 (a)). Never paths, time or environment.
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
    // Run the P0 test-pattern stage only, and no geography stage (P0-04
    // 5.2, --test-pattern; P1-00 10.1). Exclusive with `until`.
    bool test_pattern = false;
    // The stage limit (P1-00 10.1, --until): std::nullopt runs every
    // implemented geography stage in the order of P1-00 section 3;
    // stages::UNTIL_NONE ("none") runs none (the P0 empty world); a stage
    // name runs the stages up to and including it. It must be accepted by
    // stages::check_stage_limit. With no limit, `chronology` runs after
    // the geography stages (P3-03 4.3).
    std::optional<std::string> until;
    // The history settings (P3-03 4.1): recorded in meta for every world
    // (the eight param.* keys of P3-03 4.2) and handed to every stage
    // (StageContext::history_params). generate_world checks them first:
    // FatalError "generate_world: params.history.<field> is invalid".
    history::HistoryParams history;
    // Threads for parallel steps, 1 to 256 (P1-00 D10). Used only to
    // schedule work: it is never recorded in the world and never changes
    // a byte of it.
    std::uint32_t threads = 1;
};

// The names of the stages generate_world runs for params, in run order
// (P0-04 5.1 step 3, P1-00 10.1): {"test_pattern"} with test_pattern;
// otherwise the implemented stages up to params.until (by default every
// implemented stage: "elevation" ... "regions", "chronology"). FatalError if test_pattern is
// combined with until, or until is not accepted by check_stage_limit.
[[nodiscard]] std::vector<std::string_view> planned_stages(const WorldParams& params);

// Generates a world and writes it to out_utf8 (P0-04 5.1, P0-02 section 6).
// Output-location problems are returned as errors (output_exists,
// output_parent_missing, output_write_failed, output_rename_failed);
// contract violations and failures inside a stage raise FatalError
// (P0-01 4.1). On every failure path nothing but an untouched existing
// <out> remains: no <out>.partial (P0-02 section 3).
[[nodiscard]] db::Status generate_world(const WorldParams& params, std::string_view out_utf8,
                                        db::Overwrite overwrite);

// As above, with the loaded content handed to the stages (P1-00 5.1, 6.1).
// params.content_hash must equal data::content_hash(content) (FatalError
// otherwise). The overload without content uses an empty ContentSet and
// does not check the hash; a geography stage run with it fails for lack
// of its tuning.
[[nodiscard]] db::Status generate_world(const WorldParams& params, const data::ContentSet& content,
                                        std::string_view out_utf8, db::Overwrite overwrite);

}  // namespace worldgen
