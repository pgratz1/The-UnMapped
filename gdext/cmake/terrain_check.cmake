# Terrain check of the 3D preview (CTest gdext-terrain-check; spec V3D-01
# 9.2 and V3D-02 11.2; gdext/README.md, "Terrain preview").
#
#   cmake -DGODOT=<godot editor> -DWORLDGEN_CLI=<worldgen-cli>
#         -DDATA_DIR=<repo>/game/data -DPROJECT_DIR=<build>/game
#         -DWORK_DIR=<scratch dir> -DTERRAIN3D=ON|OFF -P terrain_check.cmake
#
# 1. --seed 49 --map-size 512 with every stage (V3D-05 12.2: a history, so
#    markers; its layers are the bytes of the --until regions world, W25)
#    into WORK_DIR/w512/ and the geography-only --seed 1 --map-size 1024
#    --until regions into WORK_DIR/w1024/ (no history: no markers), each
#    with `render --layer biome` as biome.png and `render --composite map`
#    as map.png.
# 2. --seed 49 --map-size 512 --until elevation into WORK_DIR/elev/ (no
#    biome layer) and --seed 42 --test-pattern into WORK_DIR/pattern/ (no
#    elevation layer).
# 3. godot --headless --path PROJECT_DIR --import.
# 4. For each world, the preview scene with --check <dir> --expect-terrain3d
#    present (TERRAIN3D on) or absent (off): the 512 world in the default
#    mode, with --flat-colours --no-trees (V3D-01's look) and with
#    --no-terrain3d --expect-terrain3d absent; the 1024 world with
#    --instance-cap 50000 (V3D-03: a split far tier, so trees are in the
#    near tier), also with --veg-threads 0; the 512 world with
#    --veg-whole-map and with --veg-radius 0; the elevation-only and
#    test-pattern worlds. Runs with vegetation in a Terrain3D node also fly
#    the camera path of V3D-03 11.2 over frames. Each run must exit 0 and
#    print UNMAPPED-TERRAIN-CHECK: PASS. V3D-04 10.2: the 512 world also
#    with --look-v3d03 all (V3D-03's look in every item).
# 5. V3D-04 T29: the refusals of 9.1 (exit 1 and the text on stderr).
# 6. V3D-04 T30: --bench headless on the 512 world prints the look line,
#    equal to the line the default-mode check computed from its own counts.
# 7. V3D-05 12.2: the 512 world with --no-markers (every check of V3D-01 to
#    V3D-04 and T39), --year 3000, --at-marker rank:1, --at-marker
#    settlement:<the largest settlement, printed by the default run> and
#    --at-marker nowhere (markers only); T40's refusals.

cmake_minimum_required(VERSION 3.25)

foreach(_v GODOT WORLDGEN_CLI DATA_DIR PROJECT_DIR WORK_DIR)
    if(NOT ${_v})
        message(FATAL_ERROR "terrain_check.cmake: ${_v} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/w512" "${WORK_DIR}/w1024" "${WORK_DIR}/elev" "${WORK_DIR}/pattern")

function(run_step name)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 120)
    string(STRIP "${_out}" _out)
    string(STRIP "${_err}" _err)
    message("---- ${name}: exit ${_rc}")
    if(_out)
        message("${_out}")
    endif()
    if(_err)
        message("[stderr]\n${_err}")
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "terrain check: step '${name}' failed (exit ${_rc})")
    endif()
    set(STEP_OUTPUT "${_out}" PARENT_SCOPE)
endfunction()

# --- 1, 2. Worlds ---------------------------------------------------------------
run_step("generate seed 49, 512, every stage" "${WORLDGEN_CLI}" generate --seed 49 --map-size 512
    --data "${DATA_DIR}" --out "${WORK_DIR}/w512/world.db")
run_step("render biome, 512" "${WORLDGEN_CLI}" render --db "${WORK_DIR}/w512/world.db" --layer biome
    --out "${WORK_DIR}/w512/biome.png")
run_step("render map, 512" "${WORLDGEN_CLI}" render --db "${WORK_DIR}/w512/world.db" --composite map
    --out "${WORK_DIR}/w512/map.png")
run_step("generate seed 1, 1024, until regions" "${WORLDGEN_CLI}" generate --seed 1 --map-size 1024
    --until regions --data "${DATA_DIR}" --out "${WORK_DIR}/w1024/world.db")
run_step("render biome, 1024" "${WORLDGEN_CLI}" render --db "${WORK_DIR}/w1024/world.db" --layer biome
    --out "${WORK_DIR}/w1024/biome.png")
run_step("render map, 1024" "${WORLDGEN_CLI}" render --db "${WORK_DIR}/w1024/world.db" --composite map
    --out "${WORK_DIR}/w1024/map.png")
run_step("generate seed 49, 512, until elevation" "${WORLDGEN_CLI}" generate --seed 49 --map-size 512
    --until elevation --data "${DATA_DIR}" --out "${WORK_DIR}/elev/world.db")
run_step("generate test pattern" "${WORLDGEN_CLI}" generate --seed 42 --test-pattern
    --out "${WORK_DIR}/pattern/world.db")

# --- 3. Import ----------------------------------------------------------------------
run_step("editor import" "${GODOT}" --headless --path "${PROJECT_DIR}" --import)

# --- 4. Preview checks --------------------------------------------------------------
if(TERRAIN3D)
    set(_expect present)
else()
    set(_expect absent)
endif()
function(terrain_check label dir expect)
    run_step("terrain check, ${label}" "${GODOT}" --headless --path "${PROJECT_DIR}" res://viewer3d/terrain_preview.tscn
        -- "${WORK_DIR}/${dir}/world.db" --check "${WORK_DIR}/${dir}" --expect-terrain3d ${expect} ${ARGN})
    if(NOT STEP_OUTPUT MATCHES "UNMAPPED-TERRAIN-CHECK: PASS")
        message(FATAL_ERROR "terrain check: the preview did not report UNMAPPED-TERRAIN-CHECK: PASS for the ${label}")
    endif()
    set(STEP_OUTPUT "${STEP_OUTPUT}" PARENT_SCOPE)
endfunction()
terrain_check("512 world" w512 ${_expect})
set(_look_line "")
if(STEP_OUTPUT MATCHES "UNMAPPED-TERRAIN-CHECK: look line ([^\n]*)")
    set(_look_line "${CMAKE_MATCH_1}")
endif()
set(_largest "")
if(STEP_OUTPUT MATCHES "UNMAPPED-TERRAIN-CHECK: largest settlement ([0-9]+)")
    set(_largest "${CMAKE_MATCH_1}")
endif()
terrain_check("512 world, --look-v3d03 all" w512 ${_expect} --look-v3d03 all)
terrain_check("512 world, --flat-colours --no-trees" w512 ${_expect} --flat-colours --no-trees)
terrain_check("512 world, --no-terrain3d" w512 absent --no-terrain3d)
terrain_check("1024 world, --instance-cap 50000" w1024 ${_expect} --instance-cap 50000)
# V3D-03 11.2: the near tier on the main thread, V3D-02's whole-map
# placement and the far tier alone.
terrain_check("1024 world, --instance-cap 50000 --veg-threads 0" w1024 ${_expect} --instance-cap 50000 --veg-threads 0)
terrain_check("512 world, --veg-whole-map" w512 ${_expect} --veg-whole-map)
terrain_check("512 world, --veg-radius 0" w512 ${_expect} --veg-radius 0)
terrain_check("elevation-only world" elev ${_expect})
terrain_check("test-pattern world" pattern ${_expect})

# --- 5. Refusals (V3D-04 T29) ----------------------------------------------------------
function(terrain_refusal label text)
    execute_process(COMMAND "${GODOT}" --headless --path "${PROJECT_DIR}" res://viewer3d/terrain_preview.tscn
        -- "${WORK_DIR}/w512/world.db" --check "${WORK_DIR}/w512" --expect-terrain3d ${_expect} ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 120)
    message("---- refusal, ${label}: exit ${_rc}")
    string(FIND "${_err}${_out}" "${text}" _at)
    if(_rc EQUAL 0 OR _at LESS 0)
        message("${_out}\n[stderr]\n${_err}")
        message(FATAL_ERROR "terrain check: ${label} was not refused with '${text}' (exit ${_rc})")
    endif()
endfunction()
terrain_refusal("--look-v3d03 trees" "--look-v3d03 trees: all, or a comma list of rock, light, snow, shrubs, sea"
    --look-v3d03 trees)
terrain_refusal("--look-v3d03 rock," "--look-v3d03 rock,: all, or a comma list of rock, light, snow, shrubs, sea"
    --look-v3d03 rock,)
terrain_refusal("--at-peak --at-forest" "--at-peak cannot be combined with --at or --at-forest" --at-peak --at-forest)
terrain_refusal("--at-peak --at" "--at-peak cannot be combined with --at or --at-forest" --at-peak --at 10 10)

# --- 6. Bench look line (V3D-04 T30) -----------------------------------------------------
if(TERRAIN3D)
    if(_look_line STREQUAL "")
        message(FATAL_ERROR "terrain check: the 512 world's check printed no look line")
    endif()
    run_step("bench, 512 world" "${GODOT}" --headless --path "${PROJECT_DIR}" res://viewer3d/terrain_preview.tscn
        -- "${WORK_DIR}/w512/world.db" --bench)
    string(FIND "${STEP_OUTPUT}" "UNMAPPED-TERRAIN-BENCH: ${_look_line}" _at)
    if(_at LESS 0)
        message(FATAL_ERROR "terrain check: the bench did not print 'UNMAPPED-TERRAIN-BENCH: ${_look_line}'")
    endif()
    message("---- T30 bench look line equals the check's own: ${_look_line}")
endif()
# --- 7. Markers (V3D-05 12.2) ---------------------------------------------------------------
if(_largest STREQUAL "")
    message(FATAL_ERROR "terrain check: the 512 world's check printed no largest settlement")
endif()
terrain_check("512 world, --no-markers" w512 ${_expect} --no-markers)
terrain_check("512 world, --year 3000" w512 ${_expect} --year 3000)
terrain_check("512 world, --at-marker rank:1" w512 ${_expect} --at-marker rank:1)
terrain_check("512 world, --at-marker settlement:${_largest}" w512 ${_expect} --at-marker settlement:${_largest})
terrain_check("512 world, --at-marker nowhere" w512 ${_expect} --at-marker nowhere)
terrain_refusal("--year -1" "--year -1: a whole number from 0" --year -1)
terrain_refusal("--year x" "--year x: a whole number from 0" --year x)
set(_at_refusal "--at-marker cannot be combined with --at, --at-forest, --at-peak or --no-markers")
terrain_refusal("--at-marker --at" "${_at_refusal}" --at-marker rank:1 --at 10 10)
terrain_refusal("--at-marker --at-forest" "${_at_refusal}" --at-marker rank:1 --at-forest)
terrain_refusal("--at-marker --at-peak" "${_at_refusal}" --at-marker rank:1 --at-peak)
terrain_refusal("--at-marker --no-markers" "${_at_refusal}" --at-marker rank:1 --no-markers)
message("terrain check: all runs passed")
