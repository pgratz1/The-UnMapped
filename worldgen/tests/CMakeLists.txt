# Tests for worldgen, written by the test writer from the specs.

add_executable(worldgen-tests
    main.cpp
    build_smoke_test.cpp
    vendored_headers_test.cpp
    # Deterministic core, spec P0-01 section 13.
    core_fx_test.cpp
    core_decimal_test.cpp
    core_wide_math_test.cpp
    core_rng_test.cpp
    core_hash_test.cpp
    core_noise_test.cpp
    core_sha256_test.cpp
    core_determinism_test.cpp
    core_golden_test.cpp
)
target_link_libraries(worldgen-tests PRIVATE
    worldgen::worldgen
    unmapped::sqlite3
    unmapped::doctest
    unmapped::tomlplusplus
    unmapped::stb_image_write
)
unmapped_project_target(worldgen-tests)

# The golden vector file is read in place from the source tree (read only;
# nothing is written there). Spec P0-01 13.8.
target_compile_definitions(worldgen-tests PRIVATE
    UNMAPPED_CORE_VECTORS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/golden/core_vectors.txt"
)

add_test(NAME worldgen-tests COMMAND worldgen-tests)

# World database layer tests (spec P0-02 section 12). Files they write go
# under a scratch directory in the build tree, never in the source tree.
target_sources(worldgen-tests PRIVATE
    db_layer_blob_test.cpp
    db_writer_reader_test.cpp
    db_determinism_test.cpp
    db_finalize_test.cpp
)
target_compile_definitions(worldgen-tests PRIVATE
    UNMAPPED_DB_TEST_TMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}/db-test-tmp"
)
