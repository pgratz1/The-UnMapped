#pragma once

// Runs shared by the P5-03 tests: the fixtures on the conflict stress geographies (p3-06-conflict.txt's
// stress-<seed> cases, the reference's grid) with a content set and a configuration, and generated worlds with the
// shipped data (cached per process). No library name beyond the adapter's.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>

#include "p5_03_relations_api.hpp"
#include "p5_03_relations_checks.hpp"

namespace p503run {

namespace wd = worldgen::data;
namespace db = worldgen::db;

inline eng::HistoryInputs stress_inputs(std::uint64_t seed, const wd::ContentSet& content) {
    const conflict_test::CCase& c = conflict_test::conflict_case(("stress-" + std::to_string(seed)).c_str());
    return conflict_test::inputs_of(conflict_test::make_synthetic(seed), seed, conflict_test::settings_of(c), content);
}

// The history stage (the full configuration of 6.6) on stress geography `seed`.
inline p503check::World stage_fixture(std::uint64_t seed, const wd::ContentSet& content) {
    const eng::HistoryInputs in = stress_inputs(seed, content);
    return p503check::world_of_run(p503api::history_stage(content, in), in, content);
}

// A configuration of the adapter (relations, ledger and view each optional) on stress geography `seed`.
inline p503check::World config_fixture(std::uint64_t seed, const wd::ContentSet& content,
                                       p503api::RelConfig::Options o) {
    const eng::HistoryInputs in = stress_inputs(seed, content);
    const p503api::RelConfig cfg(content, o);
    return p503check::world_of_run(cfg.run(content, in), in, content);
}

// A generated world with the shipped data: seed, map size, monster pressure (cached per process).
inline std::string world_path(std::uint64_t seed, std::int64_t size, const std::string& pressure = "normal",
                              std::int64_t span = 6000, const std::string& races = "all",
                              const std::string& until = "", std::uint32_t threads = 1) {
    if (pressure == "normal") {
        return chrono_test::world(chrono_test::WorldKey{seed, "", size, span, races, until, threads});
    }
    using Key = std::tuple<std::uint64_t, std::int64_t, std::string, std::int64_t, std::string, std::string>;
    static std::map<Key, std::string> cache;
    const Key k{seed, size, pressure, span, races, until};
    if (const auto it = cache.find(k); it != cache.end()) return it->second;
    chrono_api::Params p = chrono_test::params_with(span, chrono_test::races_of_text(races));
    p.pressure = pressure;
    const std::string path = chrono_test::world_dir().file("rel" + std::to_string(cache.size()) + ".db");
    const worldgen::WorldParams wp = chrono_test::world_params(
        seed, size, p, conflict_test::shipped(), until.empty() ? std::nullopt : std::optional<std::string>(until),
        threads);
    REQUIRE(worldgen::generate_world(wp, conflict_test::shipped(), path, db::Overwrite::refuse).ok());
    cache[k] = path;
    return path;
}

inline p503check::World world_rows(const std::string& path) {
    const db::WorldReader r = chrono_test::open_world(path);
    return p503check::world_of_file(r, conflict_test::shipped());
}

}  // namespace p503run
