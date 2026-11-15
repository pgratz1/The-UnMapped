# worldgen-cli: command-line front end to the generator. No Godot.

add_executable(worldgen-cli main.cpp)
target_link_libraries(worldgen-cli PRIVATE worldgen::worldgen)
unmapped_project_target(worldgen-cli)

if(UNMAPPED_BUILD_TESTS)
    add_test(NAME worldgen-cli-version COMMAND worldgen-cli --version)
    set_tests_properties(worldgen-cli-version PROPERTIES PASS_REGULAR_EXPRESSION "^worldgen-cli [^\r\n]+")
    add_test(NAME worldgen-cli-unknown-command COMMAND worldgen-cli no-such-command)
    set_tests_properties(worldgen-cli-unknown-command PROPERTIES WILL_FAIL TRUE)
endif()
