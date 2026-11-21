#pragma once

// Interim seams of P3-02 r10 12.2 (generator 0.19.0) for the tests of the core rule, in the pattern of
// p3_09_balance_api.hpp. The spec names no C++ function for the core of a root, the near test or the
// history stage's set K; the names below are the test writer's. With UNMAPPED_P3_10_API 0 (the library
// of 0.18.0) they are not compiled; at integration the lead maps them to the engineer's signatures.

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "p3_02_names_api.hpp"
#include "../p3_10_switch.hpp"

#if UNMAPPED_P3_10_API
#include "worldgen/history/names.hpp"

namespace names_api_r10 {

// 12.2: the core of a root (its letters without ' and -).
inline std::string name_core(std::string_view root) { return std::string(worldgen::history::name_core(root)); }

// 12.2: k is near K when some core of K equals k or is one letter (inserted, deleted, replaced) apart.
inline bool core_near(std::string_view core, const std::set<std::string>& cores) {
    return worldgen::history::core_near(core, cores);
}

// 4.3 as amended by 12.2 for civilizations and settlements in the history stage: the registry and K.
inline names_api::Choice choose_name_k(const names_api::Naming& n, const names_api::Filter& f,
                                       std::string_view culture_id, names_api::Kind k, std::uint64_t seed,
                                       std::uint64_t culture, std::uint64_t entity, names_api::Registry& registry,
                                       std::set<std::string>& cores) {
    const worldgen::history::NameChoice c = worldgen::history::choose_name(
        names_api::to_lib(n), names_api::to_lib(f), culture_id, names_api::to_lib(k), seed, culture, entity,
        registry.lib(), cores);
    return names_api::Choice{std::string(c.name), static_cast<std::uint64_t>(c.attempt), c.fallback};
}

}  // namespace names_api_r10
#endif
