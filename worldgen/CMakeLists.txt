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
)
add_library(worldgen::worldgen ALIAS worldgen)
target_include_directories(worldgen
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(worldgen PUBLIC cxx_std_20)
unmapped_project_target(worldgen)
# SQLite is used by src/db/ only; no public header includes sqlite3.h.
target_link_libraries(worldgen PRIVATE unmapped::sqlite3)

if(UNMAPPED_BUILD_TESTS)
    add_subdirectory(tests)
endif()
