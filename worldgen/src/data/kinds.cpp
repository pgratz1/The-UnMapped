// The production kind registry (P0-03 section 7.1): the one source file that
// lists every kind schema the generator loads from the data directory.
//
// P0 registers no kinds (P0-03 section 10). Each later stage spec that needs
// a kind adds it here with its schema, for example in P1:
//
//   registry.add(Kind{"biome", "biomes", true, Schema({...fields...}, {...checks...})});

#include "worldgen/data/schema.hpp"

namespace worldgen::data {

KindRegistry builtin_kinds() {
    KindRegistry registry;
    return registry;
}

}  // namespace worldgen::data
