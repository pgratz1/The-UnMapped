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

# Committed loader fixtures (P0-03 section 13), read in place from the source
# tree; tests never write there (scratch files go to the system temp dir).
target_compile_definitions(worldgen-tests PRIVATE
    UNMAPPED_LOADER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/loader"
)
