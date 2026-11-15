# worldgen-cli: command-line front end to the generator (spec P0-04). No Godot.

set(WORLDGEN_CLI_SOURCES main.cpp cli.cpp output.cpp platform.cpp)

add_executable(worldgen-cli ${WORLDGEN_CLI_SOURCES})
target_link_libraries(worldgen-cli PRIVATE worldgen::worldgen unmapped::stb_image_write)
unmapped_project_target(worldgen-cli)

if(UNMAPPED_BUILD_TESTS)
    # Test-hook build of the CLI (P0-04 12.2): the same sources plus the
    # hidden generate option --test-fatal-in-stage. Never shipped.
    add_executable(worldgen-cli-test-hooks ${WORLDGEN_CLI_SOURCES})
    target_link_libraries(worldgen-cli-test-hooks PRIVATE worldgen::worldgen unmapped::stb_image_write)
    target_compile_definitions(worldgen-cli-test-hooks PRIVATE WORLDGEN_CLI_TEST_HOOKS=1)
    unmapped_project_target(worldgen-cli-test-hooks)

    add_test(NAME worldgen-cli-version COMMAND worldgen-cli --version)
    set_tests_properties(worldgen-cli-version PROPERTIES PASS_REGULAR_EXPRESSION "^worldgen-cli [^\r\n]+")
    add_test(NAME worldgen-cli-unknown-command COMMAND worldgen-cli no-such-command)
    set_tests_properties(worldgen-cli-unknown-command PROPERTIES WILL_FAIL TRUE)
endif()
