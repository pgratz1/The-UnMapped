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

# Committed loader fixtures (P0-03 section 13), read in place from the source
# tree; tests never write there (scratch files go to the system temp dir).
target_compile_definitions(worldgen-tests PRIVATE
    UNMAPPED_LOADER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/loader"
)

# worldgen-cli library parts (spec P0-04 section 12, amendment 12): the
# test-pattern stage, the generation pipeline, render_rgb, the report
# builder, the logical hashes and the reader's raw row access.
target_sources(worldgen-tests PRIVATE
    stages_test_pattern_test.cpp
    inspect_render_test.cpp
    inspect_report_test.cpp
    inspect_hashes_test.cpp
)

# Output-location handling after the P0-04 audit (dangling symbolic links).
target_sources(worldgen-tests PRIVATE generate_output_test.cpp)
