# Tests for worldgen, written by the test writer from the specs.

add_executable(worldgen-tests
    main.cpp
    build_smoke_test.cpp
    vendored_headers_test.cpp
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

# World database layer tests (spec P0-02 section 12). Files they write go
# under a scratch directory in the build tree, never in the source tree.
target_sources(worldgen-tests PRIVATE
    db_layer_blob_test.cpp
    db_writer_reader_test.cpp
    db_determinism_test.cpp
)
target_compile_definitions(worldgen-tests PRIVATE
    UNMAPPED_DB_TEST_TMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}/db-test-tmp"
)
