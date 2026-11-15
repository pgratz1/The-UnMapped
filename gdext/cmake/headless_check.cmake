# Headless check of the viewer (CTest gdext-headless-check; gdext/README.md).
#
#   cmake -DGODOT=<godot editor> -DWORLDGEN_CLI=<worldgen-cli>
#         -DPROJECT_DIR=<build>/game -DWORK_DIR=<scratch dir>
#         -P headless_check.cmake
#
# 1. worldgen-cli generate --seed 42 --test-pattern, and render each layer
#    to <layer>.png with the default palette (auto) and scale (1).
# 2. godot --headless --path PROJECT_DIR --import: the editor loads the
#    project and the extension and scans the project, as on a first open.
# 3. godot --headless --path PROJECT_DIR -- <world> --check <dir>
#    --expect-layers 3: the viewer scene opens the world, shows each layer
#    and compares its pixels with the PNGs (gdext/src/viewer/viewer_check.hpp).
# Fails unless every step exits 0 and step 3 prints UNMAPPED-CHECK: PASS.

cmake_minimum_required(VERSION 3.25)

foreach(_v GODOT WORLDGEN_CLI PROJECT_DIR WORK_DIR)
    if(NOT ${_v})
        message(FATAL_ERROR "headless_check.cmake: ${_v} is required")
    endif()
endforeach()

set(_layers test.ramp test.hash test.noise)
set(_world "${WORK_DIR}/world.db")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# Each step normally takes a few seconds. If the extension fails to load,
# the viewer scene cannot be instantiated and Godot idles instead of
# quitting; the timeout turns that into a failure.
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
        message(FATAL_ERROR "headless check: step '${name}' failed (exit ${_rc})")
    endif()
    set(STEP_OUTPUT "${_out}" PARENT_SCOPE)
endfunction()

run_step("generate" "${WORLDGEN_CLI}" generate --seed 42 --test-pattern --out "${_world}")
foreach(_layer IN LISTS _layers)
    run_step("render ${_layer}" "${WORLDGEN_CLI}" render --db "${_world}" --layer "${_layer}" --out "${WORK_DIR}/${_layer}.png")
endforeach()

run_step("editor import" "${GODOT}" --headless --path "${PROJECT_DIR}" --import)

run_step("viewer check" "${GODOT}" --headless --path "${PROJECT_DIR}" --
    "${_world}" --check "${WORK_DIR}" --expect-layers 3)
if(NOT STEP_OUTPUT MATCHES "UNMAPPED-CHECK: PASS")
    message(FATAL_ERROR "headless check: the viewer did not report UNMAPPED-CHECK: PASS")
endif()
message("headless check: PASS")
