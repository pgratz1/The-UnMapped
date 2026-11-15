#include "worldgen/core/error.hpp"

namespace worldgen {

FatalError::~FatalError() = default;

void fatal(const char* message) {
    throw FatalError(message);
}

}  // namespace worldgen
