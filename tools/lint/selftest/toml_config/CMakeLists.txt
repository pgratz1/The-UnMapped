# Self-test fixture for toml-config (paired with toml_document.cpp here).
# Not part of any build. A missing macro is reported on the
# target_compile_definitions line, a wrong value on its own line.
add_library(unmapped_tomlplusplus INTERFACE)
# TOML_FLOAT_CHARCONV=1 in a comment does not count as pinned.
target_compile_definitions(unmapped_tomlplusplus INTERFACE  # expect: toml-config (TOML_FLOAT_CHARCONV missing)
    TOML_EXCEPTIONS=1
    TOML_ENABLE_SIMD=1  # expect: toml-config (wrong value)
    TOML_EXTRA_UNGUARDED=0
)
