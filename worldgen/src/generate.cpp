// World generation pipeline (spec P0-04 section 5.1).

#include "worldgen/generate.hpp"

#include "stages/stage_faults.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/stages/test_pattern.hpp"

namespace worldgen {

std::vector<std::string_view> planned_stages(const WorldParams& params) {
    std::vector<std::string_view> names;
    // P0 stages (P0-04 5.1 step 3). P1 adds the geography stages here, in
    // design doc order, before any later phase's stages.
    if (params.test_pattern) {
        names.push_back(stages::TEST_PATTERN_STAGE);
    }
    return names;
}

db::Status generate_world(const WorldParams& params, std::string_view out_utf8, db::Overwrite overwrite) {
    return stages::detail::generate_world(params, out_utf8, overwrite, stages::StageFault::none);
}

namespace stages {

namespace detail {

db::Status generate_world(const WorldParams& params, std::string_view out_utf8, db::Overwrite overwrite,
                          StageFault fault) {
    if (fault == StageFault::fatal_inside_test_pattern && !params.test_pattern) {
        fatal("generate_world_with_fault: fatal_inside_test_pattern needs params.test_pattern");
    }
    // Fail early on the output location, before any work (P0-04 4.4).
    if (db::Status pre = db::WorldWriter::check_output(out_utf8, overwrite); !pre.ok()) {
        return pre;
    }

    // Steps 1-2 (P0-02 section 6).
    db::WorldWriter writer;
    if (params.seed_phrase.empty()) {
        writer.set_seed(params.seed);
    } else {
        if (phrase_seed(params.seed_phrase) != params.seed) {
            fatal("generate_world: seed does not match phrase_seed(seed_phrase)");
        }
        writer.set_seed_phrase(params.seed_phrase);  // FatalError unless already scrubbed
    }
    writer.set_content_hash(params.content_hash);
    writer.set_param_integer("map_size", params.map_size);  // FatalError unless 512, 1024, 2048

    // Step 3: the stages in their fixed order.
    for (const std::string_view stage : planned_stages(params)) {
        if (stage == TEST_PATTERN_STAGE) {
            run_test_pattern(writer, fault);
        } else {
            fatal("generate_world: no entry point for a planned stage");
        }
    }

    // Steps 4-9.
    return writer.finalize(out_utf8, overwrite);
}

}  // namespace detail

namespace testing {

db::Status generate_world_with_fault(const WorldParams& params, std::string_view out_utf8, db::Overwrite overwrite,
                                     StageFault fault) {
    return detail::generate_world(params, out_utf8, overwrite, fault);
}

db::Status write_fixture_world(std::string_view out_utf8, std::uint64_t seed, std::span<const FixtureLayer> layers,
                               db::Overwrite overwrite) {
    if (db::Status pre = db::WorldWriter::check_output(out_utf8, overwrite); !pre.ok()) {
        return pre;
    }
    db::WorldWriter writer;
    writer.set_seed(seed);
    writer.set_content_hash(sha256(std::string_view{}));
    writer.set_param_integer("map_size", 1024);
    if (!layers.empty()) {
        db::StageWriter stage = writer.begin_stage(FIXTURE_STAGE);
        std::int64_t id = writer.next_layer_id();
        for (const FixtureLayer& layer : layers) {
            stage.write_layer(id, layer.name, layer.grid, layer.codec);
            ++id;
        }
        stage.commit();
    }
    return writer.finalize(out_utf8, overwrite);
}

}  // namespace testing

}  // namespace stages

}  // namespace worldgen
