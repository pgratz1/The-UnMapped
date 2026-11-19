cmake_minimum_required(VERSION 3.25...4.2)

# --------------------------------------------------------------------------
# Build output lives outside the source tree (CLAUDE.md, "Build and test").
# The checkout may sit in a synced folder, so refuse any build directory that
# is the source directory or inside it. Use a preset: cmake --preset <name>.
# --------------------------------------------------------------------------
file(REAL_PATH "${CMAKE_SOURCE_DIR}" _unmapped_src)
file(REAL_PATH "${CMAKE_BINARY_DIR}" _unmapped_bin)
cmake_path(IS_PREFIX _unmapped_src "${_unmapped_bin}" NORMALIZE _unmapped_bin_inside_src)
if(_unmapped_bin_inside_src)
    # CMake writes CMakeCache.txt and CMakeFiles/ even when configuration
    # fails, so removing them here would not help; the message asks for it.
    message(FATAL_ERROR
        "Refusing to build inside the source tree.\n"
        "  source: ${_unmapped_src}\n"
        "  build:  ${_unmapped_bin}\n"
        "Build output must live outside the repository. Use a preset, e.g.\n"
        "  cmake --preset linux-gcc-debug\n"
        "which builds under $HOME/.cache/the-unmapped/build/ (or under\n"
        "UNMAPPED_BUILD_ROOT, see CLAUDE.md). Now delete the CMakeCache.txt\n"
        "and CMakeFiles/ that this attempt left in ${_unmapped_bin}.")
endif()

# UNMAPPED_BUILD_ROOT overrides the default build root. Presets cannot read
# an optional environment variable with a fallback, so the override is given
# with -B (see CLAUDE.md). Warn when the variable is set but not used.
if(DEFINED ENV{UNMAPPED_BUILD_ROOT} AND NOT "$ENV{UNMAPPED_BUILD_ROOT}" STREQUAL "")
    file(REAL_PATH "$ENV{UNMAPPED_BUILD_ROOT}" _unmapped_root)
    cmake_path(IS_PREFIX _unmapped_root "${_unmapped_bin}" NORMALIZE _unmapped_bin_inside_root)
    if(NOT _unmapped_bin_inside_root)
        message(WARNING
            "UNMAPPED_BUILD_ROOT is set to '$ENV{UNMAPPED_BUILD_ROOT}' but this build "
            "directory (${_unmapped_bin}) is not under it. To use it, configure with\n"
            "  cmake --preset <name> -B \"$UNMAPPED_BUILD_ROOT/build/<name>\"")
    endif()
endif()

project(TheUnmapped LANGUAGES C CXX)

option(UNMAPPED_USE_CCACHE "Use ccache as the compiler launcher when it is installed" ON)
if(UNMAPPED_USE_CCACHE)
    find_program(UNMAPPED_CCACHE_PROGRAM ccache)
    if(UNMAPPED_CCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${UNMAPPED_CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${UNMAPPED_CCACHE_PROGRAM}")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The project does not use C++ modules; skip dependency scanning.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# The build never downloads anything. Belt and braces for any future use of
# FetchContent: it may only use already-populated sources.
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "No network access during the build" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(UnmappedCompilerFlags)
include(UnmappedTestShards)

option(UNMAPPED_BUILD_TESTS "Build the test executables" ON)
if(UNMAPPED_BUILD_TESTS)
    enable_testing()
endif()

# The Godot GDExtension (gdext/) and the vendored godot-cpp it needs. OFF by
# default: godot-cpp is large and slow to build, and worldgen and
# worldgen-cli never depend on it (hard rule 2). With it OFF, nothing below
# differs from a build without gdext/. Turn it on for a preset with
#   cmake --preset linux-gcc-debug -DUNMAPPED_BUILD_GDEXT=ON
option(UNMAPPED_BUILD_GDEXT "Build the Godot GDExtension (gdext/) and the vendored godot-cpp" OFF)
if(UNMAPPED_BUILD_GDEXT AND UNMAPPED_SANITIZE)
    # A sanitized shared library cannot be loaded into the stock Godot
    # editor, Clang leaves the sanitizer runtime unlinked in it, and
    # godot-cpp refuses sanitizer builds without engine-side macros.
    message(FATAL_ERROR "UNMAPPED_BUILD_GDEXT cannot be combined with UNMAPPED_SANITIZE "
        "(e.g. the linux-clang-asan preset): the extension is loaded into the uninstrumented "
        "Godot editor. Configure the sanitizer preset with -DUNMAPPED_BUILD_GDEXT=OFF.")
endif()
if(UNMAPPED_BUILD_GDEXT)
    # The extension is a shared library that links the static worldgen and
    # SQLite libraries, so those must be position independent. This changes
    # code generation only (no effect on integer results); the world hash
    # check is run with the option on as well (CLAUDE.md, "Build and test").
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

add_subdirectory(extern)
add_subdirectory(worldgen)
add_subdirectory(tools/worldgen-cli)

if(UNMAPPED_BUILD_GDEXT)
    add_subdirectory(gdext)
endif()
