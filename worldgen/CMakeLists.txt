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

if(UNMAPPED_BUILD_TESTS)
    add_subdirectory(tests)
endif()
