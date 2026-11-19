// World generation pipeline (spec P0-04 section 5.1).

#include "worldgen/generate.hpp"

#include "stages/pipeline.hpp"
#include "stages/stage_faults.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/stages/stage.hpp"
#include "worldgen/stages/test_pattern.hpp"

namespace worldgen {

namespace {

// The geography stages params asks for (P1-00 10.1), after the checks of
// planned_stages.
std::vector<stages::StageInfo> geography_plan(const WorldParams& params) {
    if (params.test_pattern) {
        if (params.until) {
            fatal("generate_world: test_pattern cannot be combined with a stage limit");
        }
        return {};
    }
    if (params.until && stages::check_stage_limit(*params.until) != stages::StageLimitError::none) {
        fatal("generate_world: the stage limit is not 'none' or an implemented stage name");
    }
    return stages::stages_until(params.until);
}

}  // namespace

namespace stages::detail {

void write_history_params(db::WorldWriter& writer, const history::HistoryParams& history) {
    const std::string_view prefix = db::META_PARAM_PREFIX;
    for (const auto& [key, value] : history::history_meta_entries(history)) {
        writer.set_param_text(key.substr(prefix.size()), value);
    }
}

}  // namespace stages::detail

std::vector<std::string_view> planned_stages(const WorldParams& params) {
    std::vector<std::string_view> names;
    // P0-04 5.1 step 3, P1-00 10.1: --test-pattern runs only the test
    // pattern; otherwise the geography stages in the order of P1-00 3.
    if (params.test_pattern) {
        if (params.until) {
            fatal("planned_stages: test_pattern cannot be combined with a stage limit");
        }
        names.push_back(stages::TEST_PATTERN_STAGE);
        return names;
    }
    for (const stages::StageInfo& stage : geography_plan(params)) {
        names.push_back(stage.name);
    }
    return names;
}

db::Status generate_world(const WorldParams& params, std::string_view out_utf8, db::Overwrite overwrite) {
    const data::ContentSet empty;
    return stages::detail::generate_world(params, empty, out_utf8, overwrite, stages::StageFault::none);
}

db::Status generate_world(const WorldParams& params, const data::ContentSet& content, std::string_view out_utf8,
                          db::Overwrite overwrite) {
    if (data::content_hash(content) != params.content_hash) {
        fatal("generate_world: params.content_hash is not the content hash of the content given");
    }
    return stages::detail::generate_world(params, content, out_utf8, overwrite, stages::StageFault::none);
}

namespace stages {

namespace detail {

db::Status generate_world(const WorldParams& params, const data::ContentSet& content, std::string_view out_utf8,
                          db::Overwrite overwrite, StageFault fault, const std::span<const StageInfo>* stages_override) {
    if (fault == StageFault::fatal_inside_test_pattern && !params.test_pattern) {
        fatal("generate_world_with_fault: fatal_inside_test_pattern needs params.test_pattern");
    }
    // The plan first: an invalid limit or thread count fails before any
    // work (P1-00 10.1).
    std::vector<StageInfo> geography = geography_plan(params);
    if (stages_override != nullptr) {
        if (params.test_pattern || params.until) {
            fatal("generate_world_with_stages: test_pattern and until must be unset");
        }
        geography.assign(stages_override->begin(), stages_override->end());
    }
    if (params.threads < 1U || params.threads > MAX_THREADS) {
        fatal("generate_world: thread count outside [1, 256]");
    }
    // P3-03 4.1 step 2: the history settings, before any work.
    if (const std::string_view bad = history::history_params_problem(params.history); !bad.empty()) {
        throw FatalError("generate_world: params.history." + std::string(bad) + " is invalid");
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
    // P3-03 4.1 step 3: the eight history keys, for every world.
    write_history_params(writer, params.history);

    // Step 3: the stages in their fixed order, each in its own transaction
    // (P0-04 5.1, P1-00 3).
    if (params.test_pattern) {
        run_test_pattern(writer, fault);
    } else {
        Pipeline pipeline(writer, content, params.map_size, params.threads, params.history);
        for (const StageInfo& stage : geography) {
            pipeline.run(stage);
        }
    }

    // Steps 4-9.
    return writer.finalize(out_utf8, overwrite);
}

}  // namespace detail

namespace testing {

db::Status generate_world_with_fault(const WorldParams& params, std::string_view out_utf8, db::Overwrite overwrite,
                                     StageFault fault) {
    const data::ContentSet empty;
    return detail::generate_world(params, empty, out_utf8, overwrite, fault);
}

db::Status generate_world_with_stages(const WorldParams& params, const data::ContentSet& content,
                                     std::span<const StageInfo> stages, std::string_view out_utf8,
                                     db::Overwrite overwrite) {
    if (data::content_hash(content) != params.content_hash) {
        fatal("generate_world_with_stages: params.content_hash is not the content hash of the content given");
    }
    return detail::generate_world(params, content, out_utf8, overwrite, StageFault::none, &stages);
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
    detail::write_history_params(writer, history::HistoryParams{});  // the defaults (P3-03 4.1)
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
