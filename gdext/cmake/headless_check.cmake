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
#    ([layers]), the layers biome, elevation, rainfall and region must be
#    among them,
#    and every layer is rendered to <layer>.png and the composites map,
#    biomes and regions to composite.<name>.png.
#    With history (spec P4-03 10.1): `report --history --level full` into
#    history.txt; from it the end year E, the age start years and the year F
#    of the first region_ceded event; the world must have a ceded region, a
#    war, a ruined settlement and an ended civilization; the territory image
#    is rendered at E (composite.territory.png) and at each year of
#    L = {0, every age start, F - 1, F, floor(E / 2), E - 1, E} into
#    territory-<Y>.png.
# 2b. Geography-only world: the same seed and size with --until regions into
#    WORK_DIR/geo/, layers and composites rendered as for the real world.
# 3. godot --headless --path PROJECT_DIR --import: the editor loads the
#    project and the extension and scans the project, as on a first open.
# 4. For each world: godot --headless --path PROJECT_DIR -- <world> --check
#    <dir> --expect-layers <n> --expect-composites <m> --expect-history
#    <none|loaded> [...]: the viewer scene opens the world, shows each layer
#    and composite and compares its pixels with the PNGs, and checks the
#    year bar and the territory view (gdext/src/viewer/viewer_check.hpp).
# Fails unless every step exits 0 and each step 4 prints UNMAPPED-CHECK: PASS.

cmake_minimum_required(VERSION 3.25)

foreach(_v GODOT WORLDGEN_CLI DATA_DIR PROJECT_DIR WORK_DIR)
    if(NOT ${_v})
        message(FATAL_ERROR "headless_check.cmake: ${_v} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/pattern" "${WORK_DIR}/real" "${WORK_DIR}/geo")

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
# The layers of a world from `worldgen-cli report` ([layers] lines "<name>
# <type> <w>x<h> codec=... bytes=... ..." up to the next section header), each
# rendered to <dir>/<layer>.png, and the composites map, biomes and regions
# to <dir>/composite.<name>.png. Sets <prefix>_layers and <prefix>_count.
function(render_world label world dir prefix)
    run_step("report (layer list), ${label}" "${WORLDGEN_CLI}" report --db "${world}" QUIET)
    string(REPLACE ";" "\\;" _report "${STEP_OUTPUT}")
    string(REPLACE "\n" ";" _report_lines "${_report}")
    set(_layers "")
    set(_in_layers FALSE)
    foreach(_line IN LISTS _report_lines)
        if(_line STREQUAL "[layers]")
            set(_in_layers TRUE)
        elseif(_line MATCHES "^\\[")
            set(_in_layers FALSE)
        elseif(_in_layers AND _line MATCHES "^([^ ]+) [ui][0-9]+ [0-9]+x[0-9]+ ")
            list(APPEND _layers "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(LENGTH _layers _count)
    message("${label} layers (${_count}): ${_layers}")
    foreach(_required biome elevation rainfall region)
        if(NOT _required IN_LIST _layers)
            message(FATAL_ERROR "headless check: the ${label} has no layer '${_required}' (layers: ${_layers})")
        endif()
    endforeach()
    foreach(_layer IN LISTS _layers)
        run_step("render ${_layer}, ${label}" "${WORLDGEN_CLI}" render --db "${world}" --layer "${_layer}"
            --out "${dir}/${_layer}.png")
    endforeach()
    foreach(_composite map biomes regions)
        run_step("render composite ${_composite}, ${label}" "${WORLDGEN_CLI}" render --db "${world}"
            --composite "${_composite}" --out "${dir}/composite.${_composite}.png")
    endforeach()
    set(${prefix}_layers "${_layers}" PARENT_SCOPE)
    set(${prefix}_count ${_count} PARENT_SCOPE)
endfunction()

set(_real_dir "${WORK_DIR}/real")
set(_real_world "${_real_dir}/world.db")
run_step("generate seed 42, map size 512" "${WORLDGEN_CLI}" generate --seed 42 --map-size 512 --data "${DATA_DIR}"
    --out "${_real_world}")
render_world("512 world" "${_real_world}" "${_real_dir}" _real)
set(_real_composite_count 4)  # territory, map, biomes, regions

# The history (P4-03 10.1): report, E, age starts, F, content asserts, L.
set(_history_report "${_real_dir}/history.txt")
run_step("report --history" "${WORLDGEN_CLI}" report --db "${_real_world}" --history --level full
    --out "${_history_report}" QUIET)
file(READ "${_history_report}" _history_text)
string(REPLACE ";" "\\;" _history_text "${_history_text}")
string(REPLACE "\n" ";" _history_lines "${_history_text}")
set(_section "")
set(_end_year "")
set(_age_starts "")
set(_first_ceded "")
foreach(_line IN LISTS _history_lines)
    if(_line MATCHES "^\\[([a-z]+)\\]$")
        set(_section "${CMAKE_MATCH_1}")
    elseif(_section STREQUAL "stats" AND _line MATCHES "^end_year = ([0-9]+)$")
        set(_end_year "${CMAKE_MATCH_1}")
    elseif(_section STREQUAL "stats" AND _line MATCHES "^(events_region_ceded|wars|settlements_ruined|civilizations_ended) = ([0-9]+)$")
        set(_stat_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    elseif(_section STREQUAL "ages" AND _line MATCHES "^a[0-9]+ [^:]+: ([0-9]+)-[0-9]+")
        list(APPEND _age_starts "${CMAKE_MATCH_1}")
    elseif(_section STREQUAL "chronicle" AND _first_ceded STREQUAL "" AND
           _line MATCHES "^([0-9]+)(-[0-9]+)? e[0-9]+ region_ceded/")
        set(_first_ceded "${CMAKE_MATCH_1}")
    endif()
endforeach()
foreach(_feature events_region_ceded wars settlements_ruined civilizations_ended)
    if(NOT DEFINED _stat_${_feature} OR _stat_${_feature} LESS 1)
        message(FATAL_ERROR "headless check: the 512 world has no ${_feature} ([stats] of ${_history_report}); "
            "choose another seed and record it in gdext/README.md")
    endif()
endforeach()
if(_end_year STREQUAL "" OR _first_ceded STREQUAL "" OR NOT _age_starts)
    message(FATAL_ERROR "headless check: cannot read the end year, the ages or the first region_ceded event "
        "from ${_history_report}")
endif()
math(EXPR _ceded_before "${_first_ceded} - 1")
math(EXPR _half "${_end_year} / 2")
math(EXPR _end_before "${_end_year} - 1")
set(_years 0 ${_age_starts} ${_ceded_before} ${_first_ceded} ${_half} ${_end_before} ${_end_year})
list(REMOVE_DUPLICATES _years)
list(SORT _years COMPARE NATURAL)
list(JOIN _years "," _years_arg)
message("history: E = ${_end_year}, first region_ceded in ${_first_ceded}, years ${_years_arg}")
run_step("render composite territory" "${WORLDGEN_CLI}" render --db "${_real_world}" --composite territory
    --out "${_real_dir}/composite.territory.png")
foreach(_year IN LISTS _years)
    run_step("render territory ${_year}" "${WORLDGEN_CLI}" render --db "${_real_world}" --composite territory
        --year ${_year} --out "${_real_dir}/territory-${_year}.png")
endforeach()

# --- 2b. Geography-only world --------------------------------------------------
set(_geo_dir "${WORK_DIR}/geo")
set(_geo_world "${_geo_dir}/world.db")
run_step("generate seed 42, map size 512, until regions" "${WORLDGEN_CLI}" generate --seed 42 --map-size 512
    --data "${DATA_DIR}" --until regions --out "${_geo_world}")
render_world("geography-only world" "${_geo_world}" "${_geo_dir}" _geo)

# --- 3. Import, 4. viewer checks ----------------------------------------------
run_step("editor import" "${GODOT}" --headless --path "${PROJECT_DIR}" --import)

function(viewer_check label world dir layers composites)
    run_step("viewer check, ${label}" "${GODOT}" --headless --path "${PROJECT_DIR}" --
        "${world}" --check "${dir}" --expect-layers ${layers} --expect-composites ${composites} ${ARGN})
    if(NOT STEP_OUTPUT MATCHES "UNMAPPED-CHECK: PASS")
        message(FATAL_ERROR "headless check: the viewer did not report UNMAPPED-CHECK: PASS for the ${label}")
    endif()
endfunction()
viewer_check("test-pattern world" "${_pattern_world}" "${_pattern_dir}" ${_pattern_count} 0 --expect-history none)
viewer_check("geography-only world" "${_geo_world}" "${_geo_dir}" ${_geo_count} 3 --expect-history none)
viewer_check("512 world" "${_real_world}" "${_real_dir}" ${_real_count} ${_real_composite_count}
    --expect-history loaded --expect-end-year ${_end_year} --expect-years ${_years_arg}
    --history-report "${_history_report}")
message("headless check: PASS")
