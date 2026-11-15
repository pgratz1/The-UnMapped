#pragma once

// Umbrella header for the world database layer (spec P0-02). The test
// hooks in worldgen/db/testing.hpp are deliberately not included.

#include "worldgen/db/layer_blob.hpp"  // ElementType, LayerGrid, blob codec (5.6, 7)
#include "worldgen/db/reader.hpp"      // WorldReader (9)
#include "worldgen/db/result.hpp"      // Error, ErrorCode, Result, Status (9)
#include "worldgen/db/schema.hpp"      // SCHEMA_VERSION, DDL, meta keys (4.2, 5)
#include "worldgen/db/utf8_path.hpp"   // UTF-8 path conversion (P0-04 4.1)
#include "worldgen/db/writer.hpp"      // WorldWriter, StageWriter (6)
