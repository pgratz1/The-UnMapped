# Compiler flags for The Unmapped: the single place where they are set.
#
# 1. Arithmetic flags apply to every target, vendored code included, because
#    vendored code (SQLite in particular) can sit in the output path.
#      - No fast-math or any of its parts, on any compiler.
#      - Floating-point contraction (fused multiply-add) off.
#    Never add -ffast-math, -Ofast, -funsafe-math-optimizations,
#    -ffinite-math-only, /fp:fast or /fp:contract anywhere in the build.
# 2. Warnings, as errors, apply to project targets only, through
#    unmapped_project_target().
# 3. Vendored targets are built with warnings suppressed, through
#    unmapped_vendored_target().

option(UNMAPPED_WARNINGS_AS_ERRORS "Treat warnings in project code as errors" ON)

# --------------------------------------------------------------------------
# Arithmetic flags, all targets, C and C++
# --------------------------------------------------------------------------
if(MSVC)
    # /fp:precise is the default; state it so nothing can silently change it.
    # Since Visual Studio 2022, /fp:precise does not contract a*b+c into an
    # FMA unless /fp:contract is given, so contraction is off.
    add_compile_options(/fp:precise)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC defaults to -ffp-contract=fast in GNU mode and Clang to "on";
    # both would allow FMA contraction that differs between compilers.
    add_compile_options(-fno-fast-math -ffp-contract=off)
else()
    message(FATAL_ERROR "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}': "
        "the determinism flags in cmake/UnmappedCompilerFlags.cmake must be "
        "defined for it first.")
endif()

# Optional sanitizers for debugging builds, e.g. "address,undefined".
# Applied to every target so that vendored code is instrumented consistently.
set(UNMAPPED_SANITIZE "" CACHE STRING "Comma-separated -fsanitize= list (GCC/Clang only); empty for none")
if(UNMAPPED_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "UNMAPPED_SANITIZE is only supported with GCC and Clang")
    endif()
    add_compile_options(-fsanitize=${UNMAPPED_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${UNMAPPED_SANITIZE})
endif()

if(MSVC)
    # Source files are UTF-8; report the real __cplusplus value.
    add_compile_options(/utf-8 $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus> $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
endif()

# --------------------------------------------------------------------------
# Project targets: full warnings, as errors
# --------------------------------------------------------------------------
function(unmapped_project_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(UNMAPPED_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
            $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual>)
        if(UNMAPPED_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# --------------------------------------------------------------------------
# Vendored targets: warnings suppressed
# --------------------------------------------------------------------------
function(unmapped_vendored_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W0)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endfunction()
