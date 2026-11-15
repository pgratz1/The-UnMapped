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
)
add_library(worldgen::worldgen ALIAS worldgen)
target_include_directories(worldgen
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(worldgen PUBLIC cxx_std_20)
unmapped_project_target(worldgen)

if(UNMAPPED_BUILD_TESTS)
    add_subdirectory(tests)
endif()
