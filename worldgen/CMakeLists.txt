# worldgen: the world generator library. No engine dependencies: it must
# never link or include Godot.

add_library(worldgen STATIC
    src/version.cpp
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
