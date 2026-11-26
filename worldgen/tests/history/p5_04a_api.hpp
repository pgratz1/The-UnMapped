#pragma once

// The one place of the P5-04a tests' names and values that the spec (docs/specs/p5-04a-relations-alliances-
// followups.md r2) gives but that the 0.27.0 library does not have yet, and the switch that turns the cases that need
// the 0.28.0 engine on.
//
// The P5-04a tests are written from the spec before the implementation (on 0.27.0). The spec fixes every name the
// tests reach (field names, value keys, kinds, messages, the header `# worldgen history v9`, the constant
// SUMMARY_MILESTONE_SALIENCE of digest.hpp), so no library call is guessed here; what waits is behaviour: a 0.27.0
// world has none of the new values, and the 0.27.0 loader refuses the new fields.
//
// Integration: define UNMAPPED_P504A_API (one line below, or -DUNMAPPED_P504A_API in worldgen/tests/CMakeLists.txt)
// once the implementation is merged. Until then INTEGRATED is false and every case that needs 0.28.0 starts with
// P504A_REQUIRE_INTEGRATED(), which returns with a MESSAGE; the cases that check the spec's arithmetic by hand
// (history_alliances_p504a_unit_test.cpp) and the browser's figure search (browser_support.hpp) run either way.

#include <cstdint>
#include <string>

#include <doctest/doctest.h>

#include "worldgen/data/content.hpp"

#define UNMAPPED_P504A_API 1

namespace p504a_api {

namespace wd = worldgen::data;

inline constexpr const char* GENERATOR_SPEC = "0.28.0";            // header
inline constexpr std::int64_t SCHEMA_SPEC = 12;                    // header: no schema change
inline constexpr std::int64_t STREAMS_SPEC = 98;                   // 2: hist.alliances.renew, the 98th
inline constexpr std::int64_t KINDS_SPEC = 92;                     // no kind added
inline constexpr const char* REPORT_HEADER_SPEC = "# worldgen history v9";   // 12.1
inline constexpr std::int64_t SUMMARY_MILESTONE_SALIENCE_SPEC = 40;           // 12.2 (digest.hpp)

#if defined(UNMAPPED_P504A_API)
inline constexpr bool INTEGRATED = true;
#else
inline constexpr bool INTEGRATED = false;
#endif

// A history tuning field, or `fallback` when the definition or the field is absent (a 0.27.0 content set).
inline std::int64_t tuning_or(const wd::ContentSet& c, const std::string& id, const std::string& field,
                              std::int64_t fallback) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != id) continue;
            const wd::Value* v = d.record.find(field);
            return v == nullptr ? fallback : v->as_int();
        }
    }
    return fallback;
}

}  // namespace p504a_api

// Every test case that needs the 0.28.0 engine starts with this: before integration it returns (a MESSAGE, no check).
#define P504A_REQUIRE_INTEGRATED()                                                          \
    do {                                                                                    \
        if (!p504a_api::INTEGRATED) {                                                       \
            MESSAGE("P5-04a not integrated (define UNMAPPED_P504A_API): case not run");     \
            return;                                                                         \
        }                                                                                   \
    } while (false)
