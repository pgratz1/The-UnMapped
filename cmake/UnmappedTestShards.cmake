# CTest entries that each run a disjoint part of one doctest executable, so
# that `ctest -j` runs them in parallel. The executable itself is unchanged:
# run it directly for doctest options such as -tc= or -ts=.
#
#   unmapped_add_doctest_shards(<target>
#       TIMEOUT <seconds>
#       SHARD <name> <cost> <patterns>
#       [SHARD <name> <cost> <patterns> ...]
#       REST <name> <cost>
#       [SLOW <name> <cost> <suite> <patterns> ...]
#       [SLOW_REST <name> <cost> <suite>])
#
# Each SHARD becomes the CTest entry <target>-<name>, running
#   <target> -sf=<patterns>
# where <patterns> is a comma-separated list of doctest source-file filters
# (wildcards * and ?, matched against the test case's __FILE__, case
# insensitive). Write them as "*<file name>" or "*<file name prefix>*", never
# with a directory separator, so that they match on every platform.
#
# REST becomes <target>-<name>, running every test case that no SHARD
# selects (-sfe= with all their patterns), so a new test file is always run
# even before it is given a shard.
#
# SLOW and SLOW_REST do the same for the test suites skipped by default
# (TEST_SUITE("<stage>-slow" * doctest::skip())). A SLOW entry is
#   <target> -ts=<suite> --no-skip -sf=<patterns>
# and SLOW_REST, the catch-all, is
#   <target> -ts=<suite> --no-skip -sfe=<all SLOW patterns>
# so a new file with slow cases is run before it is given a SLOW entry.
# <suite> may be a doctest wildcard ("*-slow"). These entries carry the CTest
# label `slow`; the default test presets exclude that label and the
# <preset>-slow test presets select only it. They are not registered when
# UNMAPPED_SANITIZE is set. SLOW_REST is required whenever SLOW is given.
#
# A further entry, <target>-partition, checks with --count (and again with
# --count --no-skip, for the suites skipped by default) that the shards and
# the rest together hold exactly the executable's test cases, none twice,
# and that no shard is empty (a stale pattern after a file was renamed).
# With slow entries it also checks that they hold every test case of the
# SLOW_REST suite exactly once (the slow entries' counts add up to both the
# count of the union of their suites and the count of the SLOW_REST suite)
# and that no SLOW entry is empty. Skipped suites outside the SLOW_REST
# suite (benchmarks, dumps) are run by hand only.
#
# <cost> is the CTest COST property: the shard's approximate time in
# seconds under a release preset. CTest starts the most expensive entries
# first; it only matters for a fresh build tree (later runs use measured
# times from Testing/Temporary/CTestCostData.txt), which every CI job is.
#
# Shards must be safe to run at the same time. The test scratch directories
# are per process (process id, or a directory created atomically); in-memory
# caches are per process too. A shard that needs something exclusive gets a
# RESOURCE_LOCK property next to the call.

set(UNMAPPED_TEST_SHARDS_CHECK_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/UnmappedCheckTestShards.cmake")

function(unmapped_add_doctest_shards target)
    set(names)
    set(costs)
    set(filters)
    set(rest_name "")
    set(rest_cost "")
    set(timeout "")
    set(slow_names)
    set(slow_costs)
    set(slow_suites)
    set(slow_filters)
    set(slow_rest_name "")
    set(slow_rest_cost "")
    set(slow_rest_suite "")
    set(state "")
    foreach(arg IN LISTS ARGN)
        if(arg STREQUAL "SHARD" OR arg STREQUAL "REST" OR arg STREQUAL "TIMEOUT"
           OR arg STREQUAL "SLOW" OR arg STREQUAL "SLOW_REST")
            set(state "${arg}")
            set(pos 0)
            continue()
        endif()
        if(state STREQUAL "TIMEOUT")
            set(timeout "${arg}")
            set(state "")
        elseif(state STREQUAL "SHARD")
            if(pos EQUAL 0)
                list(APPEND names "${arg}")
            elseif(pos EQUAL 1)
                list(APPEND costs "${arg}")
            elseif(pos EQUAL 2)
                if(arg MATCHES "[/\\\\;|]")
                    message(FATAL_ERROR "unmapped_add_doctest_shards(${target}): pattern list '${arg}' "
                        "must not contain a path separator, ';' or '|'")
                endif()
                list(APPEND filters "${arg}")
                set(state "")
            endif()
            math(EXPR pos "${pos} + 1")
        elseif(state STREQUAL "REST")
            if(pos EQUAL 0)
                set(rest_name "${arg}")
            elseif(pos EQUAL 1)
                set(rest_cost "${arg}")
                set(state "")
            endif()
            math(EXPR pos "${pos} + 1")
        elseif(state STREQUAL "SLOW")
            if(pos EQUAL 0)
                list(APPEND slow_names "${arg}")
            elseif(pos EQUAL 1)
                list(APPEND slow_costs "${arg}")
            elseif(pos EQUAL 2)
                list(APPEND slow_suites "${arg}")
            elseif(pos EQUAL 3)
                if(arg MATCHES "[/\\\\;|]")
                    message(FATAL_ERROR "unmapped_add_doctest_shards(${target}): pattern list '${arg}' "
                        "must not contain a path separator, ';' or '|'")
                endif()
                list(APPEND slow_filters "${arg}")
                set(state "")
            endif()
            math(EXPR pos "${pos} + 1")
        elseif(state STREQUAL "SLOW_REST")
            if(pos EQUAL 0)
                set(slow_rest_name "${arg}")
            elseif(pos EQUAL 1)
                set(slow_rest_cost "${arg}")
            elseif(pos EQUAL 2)
                set(slow_rest_suite "${arg}")
                set(state "")
            endif()
            math(EXPR pos "${pos} + 1")
        else()
            message(FATAL_ERROR "unmapped_add_doctest_shards(${target}): unexpected argument '${arg}'")
        endif()
    endforeach()
    list(LENGTH names count)
    list(LENGTH filters filter_count)
    if(count EQUAL 0 OR NOT count EQUAL filter_count OR rest_name STREQUAL "" OR rest_cost STREQUAL ""
       OR timeout STREQUAL "")
        message(FATAL_ERROR "unmapped_add_doctest_shards(${target}): needs TIMEOUT, at least one complete "
            "SHARD <name> <cost> <patterns> and REST <name> <cost>")
    endif()

    list(LENGTH slow_names slow_count)
    list(LENGTH slow_filters slow_filter_count)
    if(NOT slow_count EQUAL slow_filter_count
       OR (slow_count GREATER 0 AND (slow_rest_name STREQUAL "" OR slow_rest_suite STREQUAL ""))
       OR (slow_count EQUAL 0 AND NOT slow_rest_name STREQUAL ""))
        message(FATAL_ERROR "unmapped_add_doctest_shards(${target}): SLOW needs complete "
            "SLOW <name> <cost> <suite> <patterns> entries and one SLOW_REST <name> <cost> <suite>")
    endif()
    # Sanitized builds run the default entries only (spec P3-10 4 item 1).
    if(UNMAPPED_SANITIZE)
        set(slow_count 0)
    endif()

    string(JOIN "," all_patterns ${filters})
    math(EXPR last "${count} - 1")
    foreach(i RANGE ${last})
        list(GET names ${i} name)
        list(GET costs ${i} cost)
        list(GET filters ${i} filter)
        add_test(NAME ${target}-${name} COMMAND ${target} "-sf=${filter}")
        set_tests_properties(${target}-${name} PROPERTIES TIMEOUT ${timeout} COST ${cost})
    endforeach()
    add_test(NAME ${target}-${rest_name} COMMAND ${target} "-sfe=${all_patterns}")
    set_tests_properties(${target}-${rest_name} PROPERTIES TIMEOUT ${timeout} COST ${rest_cost})

    set(slow_args)
    if(slow_count GREATER 0)
        string(JOIN "," all_slow_patterns ${slow_filters})
        math(EXPR slow_last "${slow_count} - 1")
        foreach(i RANGE ${slow_last})
            list(GET slow_names ${i} name)
            list(GET slow_costs ${i} cost)
            list(GET slow_suites ${i} suite)
            list(GET slow_filters ${i} filter)
            add_test(NAME ${target}-${name} COMMAND ${target} "-ts=${suite}" --no-skip "-sf=${filter}")
            set_tests_properties(${target}-${name} PROPERTIES TIMEOUT ${timeout} COST ${cost} LABELS slow)
        endforeach()
        add_test(NAME ${target}-${slow_rest_name}
            COMMAND ${target} "-ts=${slow_rest_suite}" --no-skip "-sfe=${all_slow_patterns}")
        set_tests_properties(${target}-${slow_rest_name} PROPERTIES
            TIMEOUT ${timeout} COST ${slow_rest_cost} LABELS slow)
        string(JOIN "|" slow_name_arg ${slow_names})
        string(JOIN "|" slow_suite_arg ${slow_suites})
        string(JOIN "|" slow_filter_arg ${slow_filters})
        set(slow_args
            "-DSLOW_NAMES=${slow_name_arg}"
            "-DSLOW_SUITES=${slow_suite_arg}"
            "-DSLOW_FILTERS=${slow_filter_arg}"
            "-DSLOW_REST_NAME=${slow_rest_name}"
            "-DSLOW_REST_SUITE=${slow_rest_suite}")
    endif()

    # The shard filters are passed '|'-separated (no pattern contains '|').
    string(JOIN "|" shard_arg ${filters})
    string(JOIN "|" name_arg ${names})
    add_test(NAME ${target}-partition
        COMMAND ${CMAKE_COMMAND}
            "-DEXE=$<TARGET_FILE:${target}>"
            "-DSHARD_NAMES=${name_arg}"
            "-DSHARD_FILTERS=${shard_arg}"
            "-DREST_NAME=${rest_name}"
            ${slow_args}
            -P "${UNMAPPED_TEST_SHARDS_CHECK_SCRIPT}")
    set_tests_properties(${target}-partition PROPERTIES TIMEOUT 120)
endfunction()
