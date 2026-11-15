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

if(UNMAPPED_BUILD_TESTS)
    add_subdirectory(tests)
endif()
