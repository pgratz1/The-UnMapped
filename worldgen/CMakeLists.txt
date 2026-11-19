# worldgen: the world generator library. No engine dependencies: it must
# never link or include Godot.

add_library(worldgen STATIC
    src/version.cpp
    # Deterministic core, spec P0-01.
    src/core/checked.cpp
    src/core/decimal.cpp
    src/core/error.cpp
    src/core/fx.cpp
    src/core/hash.cpp
    src/core/math.cpp
    src/core/noise.cpp
    src/core/rng.cpp
    src/core/sha256.cpp
    src/core/wide.cpp
    # World database layer, spec P0-02.
    src/db/file_header.cpp
    src/db/layer_blob.cpp
    src/db/reader.cpp
    src/db/result.cpp
    src/db/schema.cpp
    src/db/sqlite_util.cpp
    src/db/testing.cpp
    src/db/utf8_path.cpp
    src/db/writer.cpp
    # TOML loader, spec P0-03. toml_document.cpp is the only file that
    # includes toml++.
    src/data/common.cpp
    src/data/content.cpp
    src/data/encoding.cpp
    src/data/kinds.cpp
    src/data/kinds_history.cpp
    src/data/load_error.cpp
    src/data/schema.cpp
    src/data/settings.cpp
    src/data/text.cpp
    src/data/toml_document.cpp
    src/data/value.cpp
    # worldgen-cli support, spec P0-04: generation pipeline, the P0
    # test-pattern stage, and render / report / hash.
    src/generate.cpp
    src/stages/test_pattern.cpp
    src/inspect/hashes.cpp
    src/inspect/render.cpp
    src/inspect/report.cpp
    # P1 foundation (P1-00): parallel blocks (P0-01 4.2 as amended), the
    # stage pipeline (P1-00 3, 5.1, 10.1), the data root (P0-03 7.6) and the
    # shared geography algorithms (P1-00 7, P1-01 4.1).
    src/core/parallel.cpp
    src/stages/stage.cpp
    src/data/data_root.cpp
    src/geo/components.cpp
    src/geo/flow.cpp
    src/geo/grid.cpp
    src/geo/noise.cpp
    src/geo/noise_schema.cpp
    src/geo/select.cpp
    src/geo/terrain.cpp
    # Geography stages (P1-01 onward).
    src/stages/elevation.cpp
    src/stages/elevation_passes.cpp
    src/stages/hydrology.cpp
    src/stages/biomes.cpp
    src/stages/regions.cpp
    src/stages/resources.cpp
    src/stages/climate.cpp
)
add_library(worldgen::worldgen ALIAS worldgen)
target_include_directories(worldgen
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(worldgen PUBLIC cxx_std_20)
# toml++ stays out of the public headers (P0-03); PRIVATE so the parser is
# an implementation detail of worldgen.
target_link_libraries(worldgen PRIVATE unmapped::tomlplusplus)
unmapped_project_target(worldgen)
# SQLite is used by src/db/ only; no public header includes sqlite3.h.
target_link_libraries(worldgen PRIVATE unmapped::sqlite3)

# Inspection additions of P1 (spec P1-00 10.3): the [geography] statistics
# of report v2. Kept in its own target_sources call so that branches adding
# other sources merge cleanly.
target_sources(worldgen PRIVATE
    src/inspect/geography.cpp
)

# Parallel blocks (src/core/parallel.cpp) use std::thread.
find_package(Threads REQUIRED)
target_link_libraries(worldgen PUBLIC Threads::Threads)

# The default data root of P0-03 7.6: game/data/ in the source tree, recorded
# at configure time into a header in the build tree (never the source tree).
# Only src/data/data_root.cpp includes it; the path locates data files and
# never reaches a world file or a hash.
cmake_path(SET UNMAPPED_CONFIGURED_DATA_ROOT NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../game/data")
configure_file(src/data/data_root_config.hpp.in
               "${CMAKE_CURRENT_BINARY_DIR}/generated/data_root_config.hpp" @ONLY)
target_include_directories(worldgen PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(UNMAPPED_BUILD_TESTS)
    add_subdirectory(tests)
endif()
