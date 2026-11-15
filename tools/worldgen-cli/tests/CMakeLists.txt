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
