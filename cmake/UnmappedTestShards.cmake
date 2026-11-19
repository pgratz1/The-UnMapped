# CTest entries that each run a disjoint part of one doctest executable, so
# that `ctest -j` runs them in parallel. The executable itself is unchanged:
# run it directly for doctest options such as -tc= or -ts=.
#
#   unmapped_add_doctest_shards(<target>
#       TIMEOUT <seconds>
#       SHARD <name> <cost> <patterns>
#       [SHARD <name> <cost> <patterns> ...]
#       REST <name> <cost>)
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
# A further entry, <target>-partition, checks with --count (and again with
# --count --no-skip, for the suites skipped by default) that the shards and
# the rest together hold exactly the executable's test cases, none twice,
# and that no shard is empty (a stale pattern after a file was renamed).
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
    set(state "")
    foreach(arg IN LISTS ARGN)
        if(arg STREQUAL "SHARD" OR arg STREQUAL "REST" OR arg STREQUAL "TIMEOUT")
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

    # The shard filters are passed '|'-separated (no pattern contains '|').
    string(JOIN "|" shard_arg ${filters})
    string(JOIN "|" name_arg ${names})
    add_test(NAME ${target}-partition
        COMMAND ${CMAKE_COMMAND}
            "-DEXE=$<TARGET_FILE:${target}>"
            "-DSHARD_NAMES=${name_arg}"
            "-DSHARD_FILTERS=${shard_arg}"
            "-DREST_NAME=${rest_name}"
            -P "${UNMAPPED_TEST_SHARDS_CHECK_SCRIPT}")
    set_tests_properties(${target}-partition PROPERTIES TIMEOUT 120)
endfunction()
