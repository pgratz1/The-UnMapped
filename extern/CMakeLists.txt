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
# rule 1). The authoritative values come from the database-layer spec in
# docs/specs/; until it is approved this list holds only options that do not
# affect the file format.
#
# Runtime settings (page_size, journal_mode, encoding, auto_vacuum and other
# pragmas) are not compile options. They belong in one place in
# worldgen/src/db/, owned by the generator engineer, following the same spec.
#
# Changing this list changes the generator's output contract: it needs a spec
# update and a proposals entry, like a schema change.
# ==========================================================================
set(UNMAPPED_SQLITE_COMPILE_DEFINITIONS
    # Reject double-quoted string literals (SQLite's recommended setting).
    # Affects SQL parsing only, not the file format.
    SQLITE_DQS=0
    # The generator and the game never load SQLite extensions.
    SQLITE_OMIT_LOAD_EXTENSION=1
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

# --------------------------------------------------------------------------
# stb_image_write (single header; implementation compiled once here)
# --------------------------------------------------------------------------
add_library(unmapped_stb_image_write STATIC stb/stb_image_write_impl.c)
add_library(unmapped::stb_image_write ALIAS unmapped_stb_image_write)
target_include_directories(unmapped_stb_image_write SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/stb")
unmapped_vendored_target(unmapped_stb_image_write)
