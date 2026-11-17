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
)

add_test(NAME worldgen-cli-tests COMMAND worldgen-cli-tests)
# Explicit timeout, above CTest's default of 1500 s: under linux-clang-asan
# this entry takes about 480 s on 6 cores and grows with every stage.
# One value for every preset, so a hang is still caught within an hour.
set_tests_properties(worldgen-cli-tests PROPERTIES TIMEOUT 3600)

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
