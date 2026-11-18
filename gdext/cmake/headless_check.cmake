# Headless check of the viewer (CTest gdext-headless-check; gdext/README.md).
#
#   cmake -DGODOT=<godot editor> -DWORLDGEN_CLI=<worldgen-cli>
#         -DDATA_DIR=<repo>/game/data
#         -DPROJECT_DIR=<build>/game -DWORK_DIR=<scratch dir>
#         -P headless_check.cmake
#
# 1. Test pattern: worldgen-cli generate --seed 42 --test-pattern into
#    WORK_DIR/pattern/, and render each layer to <layer>.png with the default
#    palette (auto) and scale (1).
# 2. Real world: worldgen-cli generate --seed 42 --map-size 512 into
#    WORK_DIR/real/; its layers are listed from `worldgen-cli report`
#    ([layers]), the layers biome, elevation and rainfall must be among them,
#    and every layer is rendered to <layer>.png and the composites map and
#    biomes to composite.<name>.png.
# 3. godot --headless --path PROJECT_DIR --import: the editor loads the
#    project and the extension and scans the project, as on a first open.
# 4. For each world: godot --headless --path PROJECT_DIR -- <world> --check
#    <dir> --expect-layers <n> --expect-composites <m>: the viewer scene
#    opens the world, shows each layer and composite and compares its pixels
#    with the PNGs (gdext/src/viewer/viewer_check.hpp).
# Fails unless every step exits 0 and each step 4 prints UNMAPPED-CHECK: PASS.

cmake_minimum_required(VERSION 3.25)

foreach(_v GODOT WORLDGEN_CLI DATA_DIR PROJECT_DIR WORK_DIR)
    if(NOT ${_v})
        message(FATAL_ERROR "headless_check.cmake: ${_v} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/pattern" "${WORK_DIR}/real")

# Each step normally takes a few seconds. If the extension fails to load,
# the viewer scene cannot be instantiated and Godot idles instead of
# quitting; the timeout turns that into a failure. QUIET keeps a step's
# output out of the log (the report of the real world is long).
function(run_step name)
    cmake_parse_arguments(PARSE_ARGV 1 _step "QUIET" "" "")
    execute_process(COMMAND ${_step_UNPARSED_ARGUMENTS}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 120)
    string(STRIP "${_out}" _out)
    string(STRIP "${_err}" _err)
    message("---- ${name}: exit ${_rc}")
    if(_out AND NOT _step_QUIET)
        message("${_out}")
    endif()
    if(_err)
        message("[stderr]\n${_err}")
    endif()
    if(NOT _rc EQUAL 0)
        if(_out AND _step_QUIET)
            message("${_out}")
        endif()
        message(FATAL_ERROR "headless check: step '${name}' failed (exit ${_rc})")
    endif()
    set(STEP_OUTPUT "${_out}" PARENT_SCOPE)
endfunction()

# --- 1. Test-pattern world -----------------------------------------------------
set(_pattern_dir "${WORK_DIR}/pattern")
set(_pattern_world "${_pattern_dir}/world.db")
set(_pattern_layers test.ramp test.hash test.noise)
run_step("generate test pattern" "${WORLDGEN_CLI}" generate --seed 42 --test-pattern --out "${_pattern_world}")
foreach(_layer IN LISTS _pattern_layers)
    run_step("render ${_layer}" "${WORLDGEN_CLI}" render --db "${_pattern_world}" --layer "${_layer}"
        --out "${_pattern_dir}/${_layer}.png")
endforeach()
list(LENGTH _pattern_layers _pattern_count)

# --- 2. Real world -------------------------------------------------------------
set(_real_dir "${WORK_DIR}/real")
set(_real_world "${_real_dir}/world.db")
run_step("generate seed 42, map size 512" "${WORLDGEN_CLI}" generate --seed 42 --map-size 512 --data "${DATA_DIR}"
    --out "${_real_world}")
run_step("report (layer list)" "${WORLDGEN_CLI}" report --db "${_real_world}" QUIET)
# [layers] lines: "<name> <type> <w>x<h> codec=... bytes=... ..." up to the
# next section header.
string(REPLACE ";" "\\;" _report "${STEP_OUTPUT}")
string(REPLACE "\n" ";" _report_lines "${_report}")
set(_real_layers "")
set(_in_layers FALSE)
foreach(_line IN LISTS _report_lines)
    if(_line STREQUAL "[layers]")
        set(_in_layers TRUE)
    elseif(_line MATCHES "^\\[")
        set(_in_layers FALSE)
    elseif(_in_layers AND _line MATCHES "^([^ ]+) [ui][0-9]+ [0-9]+x[0-9]+ ")
        list(APPEND _real_layers "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(LENGTH _real_layers _real_count)
message("real world layers (${_real_count}): ${_real_layers}")
foreach(_required biome elevation rainfall)
    if(NOT _required IN_LIST _real_layers)
        message(FATAL_ERROR "headless check: the 512 world has no layer '${_required}' (layers: ${_real_layers})")
    endif()
endforeach()
foreach(_layer IN LISTS _real_layers)
    run_step("render ${_layer}" "${WORLDGEN_CLI}" render --db "${_real_world}" --layer "${_layer}"
        --out "${_real_dir}/${_layer}.png")
endforeach()
set(_real_composites map biomes)
foreach(_composite IN LISTS _real_composites)
    run_step("render composite ${_composite}" "${WORLDGEN_CLI}" render --db "${_real_world}"
        --composite "${_composite}" --out "${_real_dir}/composite.${_composite}.png")
endforeach()
list(LENGTH _real_composites _real_composite_count)

# --- 3. Import, 4. viewer checks ----------------------------------------------
run_step("editor import" "${GODOT}" --headless --path "${PROJECT_DIR}" --import)

function(viewer_check label world dir layers composites)
    run_step("viewer check, ${label}" "${GODOT}" --headless --path "${PROJECT_DIR}" --
        "${world}" --check "${dir}" --expect-layers ${layers} --expect-composites ${composites})
    if(NOT STEP_OUTPUT MATCHES "UNMAPPED-CHECK: PASS")
        message(FATAL_ERROR "headless check: the viewer did not report UNMAPPED-CHECK: PASS for the ${label}")
    endif()
endfunction()
viewer_check("test-pattern world" "${_pattern_world}" "${_pattern_dir}" ${_pattern_count} 0)
viewer_check("512 world" "${_real_world}" "${_real_dir}" ${_real_count} ${_real_composite_count})
message("headless check: PASS")
