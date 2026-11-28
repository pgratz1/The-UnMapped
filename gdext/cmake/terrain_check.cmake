# Terrain check of the 3D preview (CTest gdext-terrain-check; spec V3D-01
# 9.2 and V3D-02 11.2; gdext/README.md, "Terrain preview").
#
#   cmake -DGODOT=<godot editor> -DWORLDGEN_CLI=<worldgen-cli>
#         -DDATA_DIR=<repo>/game/data -DPROJECT_DIR=<build>/game
#         -DWORK_DIR=<scratch dir> -DTERRAIN3D=ON|OFF -P terrain_check.cmake
#
# 1. Geography-only worlds: --seed 49 --map-size 512 --until regions into
#    WORK_DIR/w512/ and --seed 1 --map-size 1024 --until regions into
#    WORK_DIR/w1024/, each with `render --layer biome` as biome.png and
#    `render --composite map` as map.png.
# 2. --seed 49 --map-size 512 --until elevation into WORK_DIR/elev/ (no
#    biome layer) and --seed 42 --test-pattern into WORK_DIR/pattern/ (no
#    elevation layer).
# 3. godot --headless --path PROJECT_DIR --import.
# 4. For each world, the preview scene with --check <dir> --expect-terrain3d
#    present (TERRAIN3D on) or absent (off): the 512 world in the default
#    mode, with --flat-colours --no-trees (V3D-01's look) and with
#    --no-terrain3d --expect-terrain3d absent; the 1024 world with
#    --instance-cap 50000; the elevation-only and test-pattern worlds. Each
#    run must exit 0 and print UNMAPPED-TERRAIN-CHECK: PASS.

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
run_step("generate seed 49, 512, until regions" "${WORLDGEN_CLI}" generate --seed 49 --map-size 512
    --until regions --data "${DATA_DIR}" --out "${WORK_DIR}/w512/world.db")
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
endfunction()
terrain_check("512 world" w512 ${_expect})
terrain_check("512 world, --flat-colours --no-trees" w512 ${_expect} --flat-colours --no-trees)
terrain_check("512 world, --no-terrain3d" w512 absent --no-terrain3d)
terrain_check("1024 world, --instance-cap 50000" w1024 ${_expect} --instance-cap 50000)
terrain_check("elevation-only world" elev ${_expect})
terrain_check("test-pattern world" pattern ${_expect})
message("terrain check: all runs passed")
