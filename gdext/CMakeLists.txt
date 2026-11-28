# gdext: the Godot GDExtension of The Unmapped (phase P2). Built only with
# -DUNMAPPED_BUILD_GDEXT=ON. It links worldgen; worldgen never links it.
#
# Where Godot finds the library (gdext/README.md, "How the project finds the
# extension"): the build assembles a run copy of the Godot project under the
# build tree, <build>/game/, whose entries are links to the files and
# folders of the repository's game/ and whose bin/ folder is the real output
# directory of this library. Godot is always run on that copy, so its
# .godot/ cache and the library live in the build tree and nothing is
# written into the repository.

set(UNMAPPED_GODOT_PROJECT_DIR "${CMAKE_BINARY_DIR}/game" CACHE INTERNAL
    "Run copy of the Godot project (links to game/ plus bin/)")

# The plain-C++ parts of the 3D terrain preview (spec V3D-01 6, V3D-02 8, V3D-03 5, 6): no Godot
# types, so the unit tests (gdext-terrain-tests) link them without Godot.
add_library(unmapped_terrain_core STATIC
    src/terrain/terrain_data.cpp
    src/terrain/fly_state.cpp
    src/terrain/ground.cpp
    src/terrain/vegetation.cpp
    src/terrain/placeholder_meshes.cpp
    src/terrain/near_cells.cpp
    src/terrain/look.cpp
    src/terrain/markers3d.cpp
)
target_include_directories(unmapped_terrain_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(unmapped_terrain_core PUBLIC worldgen::worldgen)
target_compile_features(unmapped_terrain_core PUBLIC cxx_std_20)
set_target_properties(unmapped_terrain_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
unmapped_project_target(unmapped_terrain_core)

add_library(unmapped_gdext SHARED
    src/register_types.cpp
    src/terrain/terrain_preview.cpp
    src/terrain/terrain_check.cpp
    src/terrain/terrain_check_ground.cpp
    src/terrain/terrain_check_near.cpp
    src/terrain/terrain_preview_near.cpp
    src/terrain/terrain_preview_markers.cpp
    src/terrain/terrain_check_markers.cpp
    src/world_file.cpp
    src/world_layer.cpp
    src/viewer/history_state.cpp
    src/viewer/browser_state.cpp
    src/viewer/playback_state.cpp
    src/viewer/viewer_model.cpp
    src/viewer/viewer_model_browser.cpp
    src/viewer/viewer_model_playback.cpp
    src/viewer/year_marks.cpp
    src/viewer/layer_map_view.cpp
    src/viewer/viewer_screen.cpp
    src/viewer/viewer_screen_browser.cpp
    src/viewer/viewer_screen_playback.cpp
    src/viewer/viewer_check.cpp
    src/viewer/viewer_check_browser.cpp
    src/viewer/viewer_check_playback.cpp
)
target_include_directories(unmapped_gdext PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(unmapped_gdext PRIVATE unmapped_terrain_core worldgen::worldgen unmapped::godot-cpp)
target_compile_features(unmapped_gdext PRIVATE cxx_std_20)
unmapped_project_target(unmapped_gdext)

# Library file name is the one in game/unmapped.gdextension:
#   Linux   bin/libunmapped_gdext.so
#   Windows bin/unmapped_gdext.dll
set_target_properties(unmapped_gdext PROPERTIES
    OUTPUT_NAME unmapped_gdext
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LIBRARY_OUTPUT_DIRECTORY "${UNMAPPED_GODOT_PROJECT_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${UNMAPPED_GODOT_PROJECT_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb"
)
# One output directory for every configuration (multi-config generators).
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(unmapped_gdext PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${UNMAPPED_GODOT_PROJECT_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${UNMAPPED_GODOT_PROJECT_DIR}/bin")
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Export only the GDExtension entry symbol, not worldgen or SQLite.
    target_link_options(unmapped_gdext PRIVATE "LINKER:--exclude-libs,ALL" "LINKER:--no-undefined")
endif()
if(MSVC)
    # No import library is needed: Godot loads the DLL by path.
    set_target_properties(unmapped_gdext PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
endif()

# --------------------------------------------------------------------------
# Terrain3D (spec V3D-01 3.3, 3.4): the vendored plug-in is built in
# extern/CMakeLists.txt; here it is placed in the run copy only, as
#   Linux   addons/terrain_3d/bin/libterrain3d.so
#   Windows addons/terrain_3d/bin/terrain3d.dll
# and its .gdextension file is generated into the build tree.
# --------------------------------------------------------------------------
if(UNMAPPED_BUILD_TERRAIN3D)
    set(_t3d_bin "${UNMAPPED_GODOT_PROJECT_DIR}/addons/terrain_3d/bin")
    set_target_properties(unmapped_terrain3d PROPERTIES
        OUTPUT_NAME terrain3d
        LIBRARY_OUTPUT_DIRECTORY "${_t3d_bin}"
        RUNTIME_OUTPUT_DIRECTORY "${_t3d_bin}"
        PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb-terrain3d")
    foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(unmapped_terrain3d PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${_t3d_bin}"
            RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${_t3d_bin}")
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(unmapped_terrain3d PRIVATE "LINKER:--exclude-libs,ALL" "LINKER:--no-undefined")
    endif()
    if(MSVC)
        set_target_properties(unmapped_terrain3d PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib-terrain3d")
    endif()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/terrain3d/terrain.gdextension.in"
        "${CMAKE_CURRENT_BINARY_DIR}/terrain3d/terrain.gdextension" @ONLY NEWLINE_STYLE LF)
    # The run copy is assembled after the extension, which needs the plug-in
    # in place for the scene to find the class.
    add_dependencies(unmapped_gdext unmapped_terrain3d)
    set(_unmapped_terrain3d_on ON)
else()
    set(_unmapped_terrain3d_on OFF)
endif()

# Assemble the run copy of the project after every build of the library, so
# new files in game/ appear without reconfiguring.
add_custom_command(TARGET unmapped_gdext POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/game"
        "-DPROJECT_DIR=${UNMAPPED_GODOT_PROJECT_DIR}"
        "-DTERRAIN3D=${_unmapped_terrain3d_on}"
        "-DTERRAIN3D_GDEXTENSION=${CMAKE_CURRENT_BINARY_DIR}/terrain3d/terrain.gdextension"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/assemble_project.cmake"
    VERBATIM)

# --------------------------------------------------------------------------
# Headless check (gdext/README.md): registered only when a Godot editor of
# the pinned version is found. The build never downloads it.
# --------------------------------------------------------------------------
set(UNMAPPED_GODOT_VERSION "4.7.2.stable" CACHE INTERNAL "Pinned Godot version (godot --version prefix)")
if(WIN32)
    set(_unmapped_tools_root "$ENV{LOCALAPPDATA}/the-unmapped/tools")
    set(_unmapped_godot_names Godot_v4.7.2-stable_win64_console.exe Godot_v4.7.2-stable_win64.exe)
else()
    set(_unmapped_tools_root "$ENV{HOME}/.cache/the-unmapped/tools")
    set(_unmapped_godot_names Godot_v4.7.2-stable_linux.x86_64)
endif()
# UNMAPPED_GODOT may be set on the command line (-DUNMAPPED_GODOT=/path) or in
# the environment; otherwise the pinned editor is looked for in the tools
# folder under the build root, then on PATH.
if(NOT UNMAPPED_GODOT AND DEFINED ENV{UNMAPPED_GODOT})
    set(UNMAPPED_GODOT "$ENV{UNMAPPED_GODOT}" CACHE FILEPATH "Godot editor of the pinned version")
endif()
find_program(UNMAPPED_GODOT
    NAMES ${_unmapped_godot_names}
    HINTS "${_unmapped_tools_root}/godot-4.7.2-stable"
    DOC "Godot editor of the pinned version, used by the headless check")

if(UNMAPPED_BUILD_TESTS)
    if(NOT UNMAPPED_GODOT)
        message(STATUS "gdext: no Godot ${UNMAPPED_GODOT_VERSION} editor found; the headless check is not registered "
            "(see gdext/README.md to install it under ${_unmapped_tools_root})")
    else()
        execute_process(COMMAND "${UNMAPPED_GODOT}" --version
            OUTPUT_VARIABLE _unmapped_godot_version OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET RESULT_VARIABLE _unmapped_godot_rc TIMEOUT 30)
        if(NOT _unmapped_godot_rc EQUAL 0 OR NOT _unmapped_godot_version MATCHES "^4\\.7\\.2\\.stable")
            message(WARNING "gdext: ${UNMAPPED_GODOT} reports version '${_unmapped_godot_version}', not "
                "${UNMAPPED_GODOT_VERSION}; the headless check is not registered")
        else()
            message(STATUS "gdext: headless check uses ${UNMAPPED_GODOT} (${_unmapped_godot_version})")
            add_test(NAME gdext-headless-check
                COMMAND "${CMAKE_COMMAND}"
                    "-DGODOT=${UNMAPPED_GODOT}"
                    "-DWORLDGEN_CLI=$<TARGET_FILE:worldgen-cli>"
                    "-DDATA_DIR=${PROJECT_SOURCE_DIR}/game/data"
                    "-DPROJECT_DIR=${UNMAPPED_GODOT_PROJECT_DIR}"
                    "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/headless-check"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/headless_check.cmake")
            # Both checks run --import on the same run copy (spec V3D-01 3.4).
            set_tests_properties(gdext-headless-check PROPERTIES TIMEOUT 300 RESOURCE_LOCK godot-run-copy)
            # The 3D terrain preview's check (spec V3D-01 9.2), with Terrain3D
            # on or off.
            add_test(NAME gdext-terrain-check
                COMMAND "${CMAKE_COMMAND}"
                    "-DGODOT=${UNMAPPED_GODOT}"
                    "-DWORLDGEN_CLI=$<TARGET_FILE:worldgen-cli>"
                    "-DDATA_DIR=${PROJECT_SOURCE_DIR}/game/data"
                    "-DPROJECT_DIR=${UNMAPPED_GODOT_PROJECT_DIR}"
                    "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/terrain-check"
                    "-DTERRAIN3D=${_unmapped_terrain3d_on}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/terrain_check.cmake")
            set_tests_properties(gdext-terrain-check PROPERTIES TIMEOUT 300 RESOURCE_LOCK godot-run-copy)
        endif()
    endif()
endif()

# --------------------------------------------------------------------------
# Unit tests of the terrain preview's plain-C++ parts (spec V3D-01 9.1): no
# Godot, so they run without the editor.
# --------------------------------------------------------------------------
if(UNMAPPED_BUILD_TESTS)
    add_executable(gdext-terrain-tests
        tests/main.cpp
        tests/terrain_data_test.cpp
        tests/fly_state_test.cpp
        tests/ground_test.cpp
        tests/vegetation_test.cpp
        tests/meshes_test.cpp
        tests/near_cells_test.cpp
        tests/look_test.cpp
        tests/markers3d_test.cpp
    )
    target_link_libraries(gdext-terrain-tests PRIVATE unmapped_terrain_core worldgen::worldgen unmapped::doctest)
    unmapped_project_target(gdext-terrain-tests)
    add_test(NAME gdext-terrain-tests COMMAND gdext-terrain-tests)
    set_tests_properties(gdext-terrain-tests PROPERTIES TIMEOUT 300)
endif()
