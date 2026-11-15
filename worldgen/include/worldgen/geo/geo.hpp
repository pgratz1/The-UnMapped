#pragma once

// Umbrella header for the pieces the geography stages share (spec P1-00).

#include "worldgen/geo/blocks.hpp"        // G6 row blocks, for_each_row_block, map_row_blocks (7.6, 8)
#include "worldgen/geo/components.hpp"    // G2 label_components, bfs_region (7.2)
#include "worldgen/geo/flow.hpp"          // G3 routing_flood, G4 receivers, G5 accumulate (7.3 to 7.5)
#include "worldgen/geo/grid.hpp"          // Grid, D8 codes, neighbour orders (2.1, 2.3)
#include "worldgen/geo/noise.hpp"         // fbm_map, fbm_m, ridged_m (P1-01 4.1)
#include "worldgen/geo/noise_schema.hpp"  // fbm_map / fbm_m TOML schemas (P1-01 4.1)
#include "worldgen/geo/select.hpp"        // G1 kth, quantile (7.1)
