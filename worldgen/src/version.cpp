#include "worldgen/version.hpp"

namespace worldgen {

std::string_view generator_version() noexcept {
    return GENERATOR_VERSION;
}

}  // namespace worldgen
