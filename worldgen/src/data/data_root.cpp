// Data root resolution (P0-03 7.6 and amendment 15, P1-00 6.3).
//
// The configure-time path comes from data_root_config.hpp, which CMake
// writes into the build tree (worldgen/CMakeLists.txt, configure_file of
// src/data/data_root_config.hpp.in). Only this file includes it, and the
// path it holds is used only to locate data files: it never reaches a world
// file, a hash or a report.

#include "worldgen/data/data_root.hpp"

#include "data_root_config.hpp"

namespace worldgen::data {

std::string_view configured_data_root() noexcept {
    return detail::CONFIGURED_DATA_ROOT;
}

DataRoot resolve_data_root(std::optional<std::string_view> flag_utf8, std::optional<std::string_view> env_utf8) {
    if (flag_utf8) {
        if (flag_utf8->empty()) {
            return DataRoot{};  // an explicit empty flag names no root; never falls back
        }
        return DataRoot{std::string(*flag_utf8), DataRootSource::flag};
    }
    if (env_utf8 && !env_utf8->empty()) {
        return DataRoot{std::string(*env_utf8), DataRootSource::environment};
    }
    const std::string_view configured = configured_data_root();
    if (!configured.empty()) {
        return DataRoot{std::string(configured), DataRootSource::configured};
    }
    return DataRoot{};
}

}  // namespace worldgen::data
