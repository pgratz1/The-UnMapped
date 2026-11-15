#pragma once
// Umbrella header for the TOML loader (spec P0-03).
#include "worldgen/data/content.hpp"     // ContentSet, load_content*, canonical_encoding, content_hash (7, 8)
#include "worldgen/data/data_root.hpp"   // resolve_data_root, configured_data_root (7.6, amendment 15)
#include "worldgen/data/load_error.hpp"  // LoadError, LoadResult, sort_errors (9)
#include "worldgen/data/schema.hpp"      // Type, Field, Check, Schema, Kind, KindRegistry (4, 7.1)
#include "worldgen/data/settings.hpp"    // Settings, load_settings_file, parse_settings (6)
#include "worldgen/data/value.hpp"       // Value, Record (4)
