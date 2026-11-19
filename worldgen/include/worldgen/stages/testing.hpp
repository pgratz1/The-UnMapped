#pragma once

// TEST SUPPORT ONLY. Hooks into the generation pipeline and fixture worlds
// that the tests of spec P0-04 section 12 need. Never call these from
// generator code or from the production CLI: they make generation fail on
// purpose or write worlds that no stage would write.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages::testing {

// A failure forced inside a stage (P0-04 12.2, "a test build hook that
// forces the fatal error inside a stage").
enum class StageFault : std::uint8_t {
    none = 0,                   // normal operation
    fatal_inside_test_pattern,  // test_pattern raises FatalError after writing test.ramp,
                                // with its stage transaction still open
};

// generate_world with a forced fault (P0-04 12.2). With
// fatal_inside_test_pattern, params.test_pattern must be true; the call then
// raises FatalError from inside the stage and no output or <out>.partial is
// left. The test-hook build of worldgen-cli calls this for
// --test-fatal-in-stage.
[[nodiscard]] db::Status generate_world_with_fault(const WorldParams& params, std::string_view out_utf8,
                                                   db::Overwrite overwrite, StageFault fault);

// generate_world with the given stages in place of the implemented
// geography stages (P1-00 3, 5.1), so the pipeline can be tested before any
// geography stage exists: each StageInfo runs in its own transaction, in
// the order given, with a StageContext that sees the stages before it. The
// names must be valid stage names; params.test_pattern must be false and
// params.until std::nullopt (FatalError otherwise); params.content_hash must
// equal data::content_hash(content). Output errors as generate_world.
[[nodiscard]] db::Status generate_world_with_stages(const WorldParams& params, const data::ContentSet& content,
                                                    std::span<const StageInfo> stages, std::string_view out_utf8,
                                                    db::Overwrite overwrite = db::Overwrite::refuse);

// One layer of a fixture world (P0-04 12.3, 12.5).
struct FixtureLayer {
    std::string name;  // a valid layer name; tests use the reserved "test." prefix
    db::LayerGrid grid;
    db::Codec codec = db::Codec::delta_varint;
};

// The stage name under which write_fixture_world records its layers.
inline constexpr std::string_view FIXTURE_STAGE = "test_fixture";

// Writes a valid world (meta as an empty P0 world for that seed:
// param.map_size 1024, the eight history keys of HistoryParams{} (P3-03
// 4.1), empty content hash) whose layer table holds the given
// layers, ids 1, 2, ... in the order given, written by one stage
// "test_fixture" (so meta.stages = test_fixture). Test-only fixtures of
// P0-04 12.3 (a constant layer; a synthetic i16 layer with values -32767, 0,
// 32767) and 12.5 (the same grid with codec 0 and with codec 1). With no
// layers, no stage runs and the world is the empty world. Output errors are
// returned as by generate_world; invalid layers raise FatalError.
[[nodiscard]] db::Status write_fixture_world(std::string_view out_utf8, std::uint64_t seed,
                                             std::span<const FixtureLayer> layers,
                                             db::Overwrite overwrite = db::Overwrite::refuse);

}  // namespace worldgen::stages::testing
