# Self-test snippet for cmake-fp-flag. Not part of any build. Comments are
# ignored: -ffast-math -Ofast /fp:fast here is not reported.
#[[ A bracket comment with -ffast-math is ignored too. ]]
add_compile_options(-ffast-math)  # expect: cmake-fp-flag
target_compile_options(t PRIVATE -Ofast)  # expect: cmake-fp-flag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funsafe-math-optimizations")  # expect: cmake-fp-flag
add_compile_options(-ffinite-math-only -fassociative-math)  # expect: cmake-fp-flag
add_compile_options(-ffp-contract=fast)  # expect: cmake-fp-flag
add_compile_options(/fp:fast)  # expect: cmake-fp-flag
add_compile_options(/fp:contract)  # expect: cmake-fp-flag
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:-fp:fast>)  # expect: cmake-fp-flag
add_compile_options(-fno-trapping-math -freciprocal-math)  # expect: cmake-fp-flag

# Near misses: not reported.
add_compile_options(-fno-fast-math -ffp-contract=off)
add_compile_options(/fp:precise)
set(NOTE "Ofast is a word, not a flag")
message(STATUS "fast-math is off")
