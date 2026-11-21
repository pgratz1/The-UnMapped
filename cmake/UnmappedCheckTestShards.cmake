# Run by the <target>-partition CTest entry (UnmappedTestShards.cmake) with
# cmake -P. Checks that the shard filters and the rest entry together select
# every test case of the doctest executable EXE exactly once, with and
# without --no-skip, and that every shard selects at least one test case.
#
#   -DEXE=<path> -DSHARD_NAMES=a|b|... -DSHARD_FILTERS=<patterns>|<patterns>|...
#   -DREST_NAME=<name>
#
# With slow entries also
#   -DSLOW_NAMES=a|b|... -DSLOW_SUITES=<suite>|<suite>|...
#   -DSLOW_FILTERS=<patterns>|... -DSLOW_REST_NAME=<name> -DSLOW_REST_SUITE=<suite>
# and checks that the SLOW entries and SLOW_REST select every test case of
# the SLOW_REST suite (with --no-skip) exactly once, and that no SLOW entry
# is empty. The counts prove it: the entries' sum equals the count of the
# union of their suites (so no case is in two entries) and the count of the
# SLOW_REST suite (so every slow case is in one and nothing else is).

cmake_minimum_required(VERSION 3.25)

foreach(var EXE SHARD_NAMES SHARD_FILTERS REST_NAME)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "UnmappedCheckTestShards.cmake: ${var} is not set")
    endif()
endforeach()

string(REPLACE "|" ";" names "${SHARD_NAMES}")
string(REPLACE "|" ";" filters "${SHARD_FILTERS}")
string(REPLACE "|" "," all_patterns "${SHARD_FILTERS}")

# Prints the number of test cases EXE selects with the given arguments.
function(count_cases out_var)
    execute_process(COMMAND "${EXE}" --count ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "${EXE} --count ${ARGN} failed (${rc}):\n${out}${err}")
    endif()
    if(NOT out MATCHES "test cases passing the current filters: ([0-9]+)")
        message(FATAL_ERROR "cannot read the test case count of ${EXE} --count ${ARGN}:\n${out}")
    endif()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

set(failed FALSE)
foreach(mode "" "--no-skip")
    if(mode STREQUAL "")
        set(label "default")
        set(extra)
    else()
        set(label "${mode}")
        set(extra "${mode}")
    endif()
    count_cases(total ${extra})
    set(sum 0)
    set(report "")
    foreach(name filter IN ZIP_LISTS names filters)
        count_cases(n ${extra} "-sf=${filter}")
        math(EXPR sum "${sum} + ${n}")
        string(APPEND report "  ${name}: ${n}\n")
        if(n EQUAL 0 AND mode STREQUAL "--no-skip")
            message(SEND_ERROR "shard ${name} selects no test case (-sf=${filter}); a file was renamed?")
            set(failed TRUE)
        endif()
    endforeach()
    count_cases(rest ${extra} "-sfe=${all_patterns}")
    math(EXPR sum "${sum} + ${rest}")
    string(APPEND report "  ${REST_NAME}: ${rest}\n")
    message(STATUS "${label}: ${total} test cases in the executable, ${sum} in the shards\n${report}")
    if(NOT sum EQUAL total)
        message(SEND_ERROR "${label}: the shards hold ${sum} test cases but the executable has ${total}; "
            "a source file is matched by two shards")
        set(failed TRUE)
    endif()
endforeach()
if(DEFINED SLOW_NAMES AND NOT SLOW_NAMES STREQUAL "")
    string(REPLACE "|" ";" slow_names "${SLOW_NAMES}")
    string(REPLACE "|" ";" slow_suites "${SLOW_SUITES}")
    string(REPLACE "|" ";" slow_filters "${SLOW_FILTERS}")
    string(REPLACE "|" "," all_slow_patterns "${SLOW_FILTERS}")
    string(REPLACE "|" "," union_suites "${SLOW_SUITES},${SLOW_REST_SUITE}")
    count_cases(slow_total --no-skip "-ts=${SLOW_REST_SUITE}")
    count_cases(slow_union --no-skip "-ts=${union_suites}")
    set(sum 0)
    set(report "")
    foreach(name suite filter IN ZIP_LISTS slow_names slow_suites slow_filters)
        count_cases(n --no-skip "-ts=${suite}" "-sf=${filter}")
        math(EXPR sum "${sum} + ${n}")
        string(APPEND report "  ${name}: ${n}\n")
        if(n EQUAL 0)
            message(SEND_ERROR "slow entry ${name} selects no test case (-ts=${suite} -sf=${filter}); "
                "a file was renamed or its slow suite removed?")
            set(failed TRUE)
        endif()
    endforeach()
    count_cases(rest --no-skip "-ts=${SLOW_REST_SUITE}" "-sfe=${all_slow_patterns}")
    math(EXPR sum "${sum} + ${rest}")
    string(APPEND report "  ${SLOW_REST_NAME}: ${rest}\n")
    message(STATUS "slow (-ts=${SLOW_REST_SUITE} --no-skip): ${slow_total} test cases in the executable, "
        "${sum} in the slow entries\n${report}")
    if(NOT sum EQUAL slow_union)
        message(SEND_ERROR "slow: the slow entries hold ${sum} test cases but their suites have "
            "${slow_union}; a source file is matched by two slow entries, or by one whose suite "
            "misses its slow cases")
        set(failed TRUE)
    endif()
    if(NOT slow_union EQUAL slow_total)
        message(SEND_ERROR "slow: the slow entries' suites select ${slow_union} test cases but "
            "-ts=${SLOW_REST_SUITE} selects ${slow_total}; a SLOW suite is wider than the SLOW_REST suite")
        set(failed TRUE)
    endif()
endif()
if(failed)
    message(FATAL_ERROR "the CTest shards of ${EXE} do not partition its test cases")
endif()
