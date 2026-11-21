#pragma once

// Interim seams of P3-02 r10 12.2 (generator 0.19.0) for the tests of the core rule, in the pattern of
// p3_09_balance_api.hpp. The spec names no C++ function for the core of a root, the near test or the
// history stage's set K; the names below are the test writer's, mapped at the P3-10 integration to the
// engineer's name_core, NameCores and choose_name.

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "p3_02_names_api.hpp"

#include "worldgen/history/names.hpp"

namespace names_api_r10 {

// 12.2: the core of a root (its letters without ' and -): worldgen::history::name_core.
inline std::string name_core(std::string_view root) { return worldgen::history::name_core(root); }

// 12.2: k is near K: the library's NameCores::near on a copy of K.
inline worldgen::history::NameCores lib_cores(const std::set<std::string>& cores) {
    worldgen::history::NameCores k;
    for (const std::string& c : cores) k.add(c);
    return k;
}
inline bool core_near(std::string_view core, const std::set<std::string>& cores) {
    return lib_cores(cores).near(core);
}

// 4.3 as amended by 12.2: the library's choose_name with a NameCores; the core it adds (the accepted attempt's,
// or the fallback's first valid attempt's, which NameChoice::attempt names) is mirrored into `cores`.
inline names_api::Choice choose_name_k(const names_api::Naming& n, const names_api::Filter& f,
                                       std::string_view culture_id, names_api::Kind k, std::uint64_t seed,
                                       std::uint64_t culture, std::uint64_t entity, names_api::Registry& registry,
                                       std::set<std::string>& cores) {
    worldgen::history::NameCores lk = lib_cores(cores);
    const worldgen::history::NameChoice c = worldgen::history::choose_name(
        names_api::to_lib(n), names_api::to_lib(f), culture_id, names_api::to_lib(k), seed, culture, entity,
        registry.lib(), lk);
    const names_api::Attempt a = names_api::attempt(n, k, seed, culture, entity, c.attempt, f);
    cores.insert(name_core(a.root));
    return names_api::Choice{c.name, c.attempt, c.fallback};
}

}  // namespace names_api_r10
