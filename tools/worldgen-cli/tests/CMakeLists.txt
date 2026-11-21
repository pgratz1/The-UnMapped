# Integration tests for worldgen-cli (spec P0-04 section 12), written by the
# test writer from the spec. A doctest executable that runs worldgen-cli and
# worldgen-cli-test-hooks as child processes, each with UNMAPPED_BUILD_ROOT
# and the current directory set to fresh directories under the build tree.

add_executable(worldgen-cli-tests
    main.cpp
    process.cpp
    cli_conventions_test.cpp
    cli_generate_test.cpp
    cli_render_test.cpp
    cli_report_test.cpp
    cli_hash_test.cpp
    cli_golden_test.cpp
)
target_link_libraries(worldgen-cli-tests PRIVATE worldgen::worldgen unmapped::doctest)
unmapped_project_target(worldgen-cli-tests)
# p0_04_reference.hpp: reference values shared with the library tests.
target_include_directories(worldgen-cli-tests PRIVATE "${PROJECT_SOURCE_DIR}/worldgen/tests")

# The binaries under test are built first and found by their exact paths.
add_dependencies(worldgen-cli-tests worldgen-cli worldgen-cli-test-hooks)
target_compile_definitions(worldgen-cli-tests PRIVATE
    WORLDGEN_CLI_PATH="$<TARGET_FILE:worldgen-cli>"
    WORLDGEN_CLI_TEST_HOOKS_PATH="$<TARGET_FILE:worldgen-cli-test-hooks>"
    # Scratch directories go under the build tree, never the source tree.
    UNMAPPED_CLI_TEST_TMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}/cli-test-tmp"
    # Golden world hashes (P0-04 section 9), read in place, never written.
    UNMAPPED_P0_WORLDS_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p0-worlds.txt"
    # P3-01: the history data directories that data roots built by the
    # tests copy (p3_01_history_fixture.hpp), read in place.
    UNMAPPED_HISTORY_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/worldgen/tests/data/geo"
)


# Behaviour fixed after the P0-04 determinism audit: stdout write errors,
# dangling symbolic links at --out, the render size limit, UTF-16 input.
target_sources(worldgen-cli-tests PRIVATE cli_audit_test.cpp)

# render palettes and --composite map, report v2 [geography] (P1-00 10.2,
# 10.3 with appendix C; P0-04 amendment 16), on fixture worlds.
target_sources(worldgen-cli-tests PRIVATE cli_geography_test.cpp)
# P1 foundation: generate --until and --threads (spec P1-00 section 10.1,
# P0-04 amendment 16 (a)); the P0 worlds unchanged.
target_sources(worldgen-cli-tests PRIVATE cli_p1_generate_test.cpp)
# report with the step, slope and edge-run statistics (P1-00 10.3, keys of
# 2026-11-15); generate --data "" (P0-03 7.6); the composite size limit.
target_sources(worldgen-cli-tests PRIVATE cli_slope_report_test.cpp)
# generate --until elevation (P1-00 14 X1, X3 for the elevation stage; P1-01).
target_sources(worldgen-cli-tests PRIVATE cli_elevation_test.cpp)
# generate --until hydrology, report and render with the hydrology layers
# (P1-02 H9; P1-00 10.1 to 10.3, 14 X1 to X3), checked against the hydrology
# golden vectors. Slow checks in the doctest suite "hydrology-slow".
target_sources(worldgen-cli-tests PRIVATE cli_hydrology_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P1_02_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p1-02-hydrology.txt"
)
# The elevation revision "natural passes and final talus" (P1-01 fourth
# version): schema 2, table `pass`, the report pass keys.
target_sources(worldgen-cli-tests PRIVATE cli_passes_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P1_01_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p1-01-elevation.txt"
)
# generate --until climate, report keys and render palettes of the climate
# layers (P1-03 3 and 9; P1-00 10.1 to 10.3, 14 X1 to X3), checked against
# values recomputed from the layers and the climate golden vectors. Slow
# checks in the doctest suite "climate-slow".
target_sources(worldgen-cli-tests PRIVATE cli_climate_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P1_03_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p1-03-climate.txt"
)
# generate --until biomes, schema 3 (tables biome and climate), report keys,
# corrupt climate rows, render --layer biome, --composite biomes and the
# render help (P1-04 3 and 9; P1-03 9; P1-00 10.1 to 10.3), against the
# biome golden vectors and pixels decoded from the PNG (png_decode.hpp).
# Slow checks in the doctest suite "biomes-slow".
target_sources(worldgen-cli-tests PRIVATE cli_biomes_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P1_04_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p1-04-biomes.txt"
)
# generate --until resources, report keys, corrupt resources layers, render
# with the `score` palette and the render help (P1-05 3 and 9; P1-00 10.1 to
# 10.3), against the resources golden vectors and pixels decoded from the
# PNG. Slow checks in the doctest suite "resources-slow".
target_sources(worldgen-cli-tests PRIVATE cli_resources_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P1_05_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p1-05-resources.txt"
)
# The default run with the regions stage (ids 1 to 19), hash --detail and
# report with the three region tables, the region keys, render of the layer
# region (`ids`) and the composite `regions`, the schema 5 refusal of
# version 4 worlds and the loader's messages for bad regions tuning (P1-06
# 3, 4 and 9; P0-02 amendment 17; P0-03 amendment 17; P0-04 amendment 18),
# against the regions golden vectors and pixels decoded from the PNG. Slow
# checks in the doctest suite "regions-slow".
target_sources(worldgen-cli-tests PRIVATE cli_regions_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P1_06_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p1-06-regions.txt"
)

# generate with the world-creation settings of P3-01 6 (flags and settings
# keys, their usage and input errors) and the --until none worlds of the
# three data roots against the content hashes of p3-01-data.txt (P3-01 8.2,
# 8.3). Shares p3_01_history_*.hpp with the library tests.
target_sources(worldgen-cli-tests PRIVATE cli_history_settings_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P3_01_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p3-01-data.txt"
    UNMAPPED_TEST_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
)

# generate with the chronology stage (P3-03): --until chronology and
# history, the unknown-stage message, [chronology] and hash --detail against
# p3-03-chronology.txt, the data checks as input errors, the history flags
# in meta (test pattern, every value, flag over file), the corrupt cases of
# report, and the hist-* cases of world-hashes.sh. Slow checks in the
# doctest suite "chronology-slow".
target_sources(worldgen-cli-tests PRIVATE cli_chronology_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P3_03_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p3-03-chronology.txt"
)

# generate with the history stage (P3-04): the default run and --until
# history, --until chronology, [history] and hash --detail against
# p3-04-engine.txt, the corrupt cases of report, and the case list of
# world-hashes.sh. Slow checks in the doctest suite "history-slow".
target_sources(worldgen-cli-tests PRIVATE cli_history_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P3_04_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p3-04-engine.txt"
)

# generate with peoples and conflict (P3-06, generator 0.16.0; replaces the
# P3-05 file, whose golden worlds are the peoples-only configuration checked
# through the library): a golden world's [history] and hash --detail against
# p3-06-conflict.txt, --threads 5, --event-intensity and --civs-per-race,
# --until chronology. Slow checks in "history-slow".
target_sources(worldgen-cli-tests PRIVATE cli_history_conflict_test.cpp)
target_compile_definitions(worldgen-cli-tests PRIVATE
    UNMAPPED_P3_06_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/worldgen/tests/golden/p3-06-conflict.txt"
)

# CTest entries, as for worldgen-tests (cmake/UnmappedTestShards.cmake):
# groups of source files run in parallel, worldgen-cli-tests-other for every
# file not named here, and worldgen-cli-tests-partition. Each test case uses
# its own Sandbox (counter and process id), so the groups never share a
# scratch path. Costs are seconds under linux-gcc-release (generator 0.10.0);
# timeout 3600 s per entry on every preset.
unmapped_add_doctest_shards(worldgen-cli-tests
    TIMEOUT 3600
    SHARD hydrology-elevation 13 "*cli_hydrology_test.cpp,*cli_elevation_test.cpp"
    SHARD passes-climate 12 "*cli_passes_test.cpp,*cli_climate_test.cpp"
    SHARD biomes 9 "*cli_biomes_test.cpp"
    SHARD resources-generate 10 "*cli_resources_test.cpp,*cli_generate_test.cpp"
    SHARD regions 12 "*cli_regions_test.cpp"
    SHARD history-settings 6 "*cli_history_settings_test.cpp"
    SHARD chronology 20 "*cli_chronology_test.cpp"
    SHARD engine 15 "*cli_history_test.cpp"
    SHARD conflict 27 "*cli_history_conflict_test.cpp"
    REST other 10
    # The *-slow suites (label slow), as for worldgen-tests; costs measured
    # 2026-11-21 at 0.18.0.
    SLOW slow-history-regions 210 "*-slow" "*cli_history_conflict_test.cpp,*cli_chronology_test.cpp,*cli_regions_test.cpp"
    SLOW slow-geography 210 "*-slow" "*cli_resources_test.cpp,*cli_hydrology_test.cpp,*cli_climate_test.cpp,*cli_biomes_test.cpp"
    SLOW_REST slow-other 10 "*-slow"
)
