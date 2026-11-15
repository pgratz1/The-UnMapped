# Vendored third-party code. See extern/README.md for versions, sources,
# licenses and checksums. Nothing here touches the network.
#
# Vendored targets keep the project's floating-point flags (set globally in
# cmake/UnmappedCompilerFlags.cmake) but are built with warnings suppressed,
# and their headers are exposed as SYSTEM includes so that the project's
# warnings-as-errors never fire inside them.

# --------------------------------------------------------------------------
# SQLite amalgamation
# --------------------------------------------------------------------------

# ==========================================================================
# SQLITE CONFIGURATION: SINGLE POINT OF CONFIGURATION
#
# Every SQLite compile-time option for the whole project is listed here and
# nowhere else. These options can change the bytes of world.db, so they are
# part of the reproducibility contract (design doc section 4, CLAUDE.md hard
# rule 1). The authoritative values are the table in
# docs/specs/p0-02-world-db.md section 4.1.
#
# Not allowed (P0-02 4.1): SQLITE_ENABLE_STAT3/4 or anything else that makes
# SQLite write statistics tables, and any option that makes WAL the default
# journal mode.
#
# Runtime settings (page_size, journal_mode, encoding, auto_vacuum and other
# pragmas) are not compile options. They belong in one place in
# worldgen/src/db/, owned by the generator engineer, following the same spec.
#
# Changing this list changes the generator's output contract: it needs a spec
# update and a proposals entry, like a schema change.
# ==========================================================================
set(UNMAPPED_SQLITE_COMPILE_DEFINITIONS
    # Fixed page size (P0-02 4.1).
    SQLITE_DEFAULT_PAGE_SIZE=4096
    # Stops SQLite choosing a larger default page size from the device's
    # sector size.
    SQLITE_MAX_DEFAULT_PAGE_SIZE=4096
    # Fixed schema format number.
    SQLITE_DEFAULT_FILE_FORMAT=4
    # No auto-vacuum pointer-map pages.
    SQLITE_DEFAULT_AUTOVACUUM=0
    # Deleted content is zeroed; no stale bytes survive in a page.
    SQLITE_SECURE_DELETE=1
    # Temporary data always in memory; no temp files.
    SQLITE_TEMP_STORE=3
    # Reject double-quoted string literals (SQLite's recommended setting).
    # Affects SQL parsing only, not the file format.
    SQLITE_DQS=0
    # The generator and the game never load SQLite extensions.
    SQLITE_OMIT_LOAD_EXTENSION=1
    # The viewer may read from several threads. No effect on the file.
    SQLITE_THREADSAFE=1
    # Performance only.
    SQLITE_DEFAULT_MEMSTATUS=0
)

add_library(unmapped_sqlite3 STATIC sqlite/sqlite3.c)
add_library(unmapped::sqlite3 ALIAS unmapped_sqlite3)
target_include_directories(unmapped_sqlite3 SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite")
# PUBLIC so that every translation unit that includes sqlite3.h sees the
# same configuration as the library itself.
target_compile_definitions(unmapped_sqlite3 PUBLIC ${UNMAPPED_SQLITE_COMPILE_DEFINITIONS})
find_package(Threads REQUIRED)
target_link_libraries(unmapped_sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
unmapped_vendored_target(unmapped_sqlite3)

# --------------------------------------------------------------------------
# doctest (header only)
# --------------------------------------------------------------------------
add_library(unmapped_doctest INTERFACE)
add_library(unmapped::doctest ALIAS unmapped_doctest)
target_include_directories(unmapped_doctest SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/doctest")

# --------------------------------------------------------------------------
# toml++ (single header)
# --------------------------------------------------------------------------
add_library(unmapped_tomlplusplus INTERFACE)
add_library(unmapped::tomlplusplus ALIAS unmapped_tomlplusplus)
target_include_directories(unmapped_tomlplusplus SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/tomlplusplus")
# ==========================================================================
# TOML++ CONFIGURATION: SINGLE POINT OF CONFIGURATION
#
# INTERFACE, so every translation unit that includes toml.hpp (worldgen's
# src/data/toml_document.cpp and the tests) sees the same settings; the
# loader refuses to compile with any other values (spec P0-03).
#   TOML_EXCEPTIONS=1              parse errors are thrown as toml::parse_error
#                                  and caught in one place in the loader.
#   TOML_HEADER_ONLY=1             the default; no separate implementation TU.
#   TOML_ENABLE_FORMATTERS=0       the loader never formats parsed values.
#   TOML_ENABLE_WINDOWS_COMPAT=0   no wide-string overloads: the same API on
#                                  every platform (the loader reads files
#                                  itself and passes bytes).
#   TOML_ENABLE_UNRELEASED_FEATURES=0  TOML 1.0.0 only.
#   TOML_FLOAT_CHARCONV=1          float literals are converted with
#                                  std::from_chars on every compiler. toml++
#                                  defaults to an istringstream on GCC/Clang
#                                  and from_chars on MSVC; the two disagree on
#                                  literals that underflow (1e-400 is a float
#                                  node with the stream on libstdc++, a parse
#                                  error with from_chars), which would change
#                                  the error list between platforms. from_chars
#                                  is locale-free and its range rule is fixed
#                                  by the standard; needs GCC 11 or newer.
#   TOML_INT_CHARCONV=1            integers are formatted (only in parser
#                                  messages) with std::to_chars everywhere;
#                                  integer parsing is toml++'s own code and
#                                  does not depend on this.
#   TOML_ENABLE_SIMD=0             no SSE code paths chosen by target ISA, so
#                                  x86-64 and arm64 compile the same code.
#   TOML_ENABLE_FLOAT16=0          the default; stated so it cannot change.
#   TOML_MAX_NESTED_VALUES=256     the default parser recursion limit; stated
#                                  so it cannot change (the loader's own limit
#                                  of 8 applies after parsing).
# ==========================================================================
target_compile_definitions(unmapped_tomlplusplus INTERFACE
    TOML_EXCEPTIONS=1
    TOML_HEADER_ONLY=1
    TOML_ENABLE_FORMATTERS=0
    TOML_ENABLE_WINDOWS_COMPAT=0
    TOML_ENABLE_UNRELEASED_FEATURES=0
    TOML_FLOAT_CHARCONV=1
    TOML_INT_CHARCONV=1
    TOML_ENABLE_SIMD=0
    TOML_ENABLE_FLOAT16=0
    TOML_MAX_NESTED_VALUES=256
)

# --------------------------------------------------------------------------
# stb_image_write (single header; implementation compiled once here)
# --------------------------------------------------------------------------
add_library(unmapped_stb_image_write STATIC stb/stb_image_write_impl.c)
add_library(unmapped::stb_image_write ALIAS unmapped_stb_image_write)
target_include_directories(unmapped_stb_image_write SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/stb")
unmapped_vendored_target(unmapped_stb_image_write)

# --------------------------------------------------------------------------
# godot-cpp (C++ bindings for GDExtension), only with UNMAPPED_BUILD_GDEXT
# --------------------------------------------------------------------------
if(UNMAPPED_BUILD_GDEXT)
    # ======================================================================
    # GODOT-CPP CONFIGURATION: SINGLE POINT OF CONFIGURATION
    #
    # godot-cpp generates its bindings at configure time with Python 3 from
    # the vendored gdextension/extension_api-4-7.json; nothing is fetched.
    #   GODOTCPP_API_VERSION 4.7        the API of the pinned engine (4.7.2:
    #                                   its --dump-extension-api equals this
    #                                   file apart from the version header,
    #                                   see extern/README.md).
    #   GODOTCPP_TARGET template_debug  loaded by the editor and by debug
    #                                   runs; release exports come later.
    #   GODOTCPP_DISABLE_EXCEPTIONS OFF godot-cpp defaults to -fno-exceptions
    #                                   and, on MSVC, a PUBLIC _HAS_EXCEPTIONS=0
    #                                   that would reach the extension's
    #                                   translation units while worldgen is
    #                                   built with exceptions: one standard
    #                                   library configuration everywhere.
    #   GODOTCPP_SYSTEM_HEADERS ON      SYSTEM includes, so the project's
    #                                   warnings-as-errors never fire inside.
    #   GODOTCPP_PRECISION single       Godot's official builds.
    #   CMAKE_MSVC_RUNTIME_LIBRARY      godot-cpp would cache the static CRT
    #                                   (/MT), which cannot link with worldgen
    #                                   (/MD). Cached here first with CMake's
    #                                   own default, which is what worldgen
    #                                   already uses.
    # The determinism flags (no fast-math, no contraction) reach godot-cpp
    # through add_compile_options like every other target.
    # ======================================================================
    set(GODOTCPP_API_VERSION "4.7" CACHE STRING "GDExtension API version (pinned engine 4.7.2)" FORCE)
    set(GODOTCPP_TARGET "template_debug" CACHE STRING "godot-cpp target" FORCE)
    set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "Keep C++ exceptions as in worldgen" FORCE)
    set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Expose godot-cpp headers as SYSTEM" FORCE)
    set(GODOTCPP_PRECISION "single" CACHE STRING "Godot real_t precision" FORCE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING
        "MSVC runtime library, the same for worldgen, godot-cpp and the extension")

    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    # godot-cpp runs its Python generator from its own source folder, at
    # configure time (file list) and at build time (bindings). Without this,
    # Python writes __pycache__/ into extern/godot-cpp/, i.e. into the
    # repository. The environment covers configure; RULE_LAUNCH_CUSTOM (Ninja
    # and Makefile generators, which every preset uses) covers the build.
    set(ENV{PYTHONDONTWRITEBYTECODE} 1)
    add_subdirectory(godot-cpp)
    set_property(DIRECTORY godot-cpp PROPERTY RULE_LAUNCH_CUSTOM
        "\"${CMAKE_COMMAND}\" -E env PYTHONDONTWRITEBYTECODE=1")
    unmapped_vendored_target(godot-cpp)
    # godot-cpp puts its Windows PDB under ${CMAKE_SOURCE_DIR}/bin, which is
    # the repository; keep it in the build tree.
    set_target_properties(godot-cpp PROPERTIES
        PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp/pdb"
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp/pdb")
    add_library(unmapped::godot-cpp ALIAS godot-cpp)
endif()
