// Self-test fixture for toml-config (paired with CMakeLists.txt here). Not
// compiled, and not scanned as a C++ snippet. The guard requires three
// macros; the CMake fixture pins one correctly, one with the wrong value and
// omits the third.
#if !defined(TOML_EXCEPTIONS) || TOML_EXCEPTIONS != 1 || !defined(TOML_ENABLE_SIMD) || TOML_ENABLE_SIMD != 0 \
    || !defined(TOML_FLOAT_CHARCONV) || TOML_FLOAT_CHARCONV != 1
#error "toml++ must be configured by unmapped::tomlplusplus (see extern/CMakeLists.txt)"
#endif

#include <toml.hpp>
