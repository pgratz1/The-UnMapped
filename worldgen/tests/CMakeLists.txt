# Tests for worldgen, written by the test writer from the specs.

add_executable(worldgen-tests
    main.cpp
    build_smoke_test.cpp
    vendored_headers_test.cpp
    # TOML loader, spec P0-03.
    data_loader_values_test.cpp
    data_loader_discovery_test.cpp
    data_loader_encoding_test.cpp
    data_loader_errors_test.cpp
    data_settings_test.cpp
)
target_link_libraries(worldgen-tests PRIVATE
    worldgen::worldgen
    unmapped::sqlite3
    unmapped::doctest
    unmapped::tomlplusplus
    unmapped::stb_image_write
)
unmapped_project_target(worldgen-tests)

add_test(NAME worldgen-tests COMMAND worldgen-tests)

# Committed loader fixtures (P0-03 section 13), read in place from the source
# tree; tests never write there (scratch files go to the system temp dir).
target_compile_definitions(worldgen-tests PRIVATE
    UNMAPPED_LOADER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/loader"
)
