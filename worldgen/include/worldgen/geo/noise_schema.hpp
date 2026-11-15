#pragma once

// TOML schemas of the shared noise records (spec P1-01 section 4.1, P1-00
// section 6.2), as reusable schema-building helpers for the stage schemas
// of the `worldgen` kind, and readers from a loaded record.
//
//   fbm_map = { cycles = "2", octaves = 4, gain = "0.5" }
//   fbm_m   = { wavelength_m = 3000, octaves = 3, gain = "0.5" }
//
// Every field is required (P1-00 6.1). The canonical encoding of a record
// field is that of P0-03 section 8 (value 0x06 followed by the fields in the
// declaration order below).

#include "worldgen/data/schema.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/geo/noise.hpp"

namespace worldgen::geo {

// Schema of record `fbm_map`, fields in this declaration order:
// cycles fx(0.125, 64), octaves int(1, 10), gain fx(0.0625, 1), all
// required (P1-01 4.1).
[[nodiscard]] data::Schema fbm_map_schema();
// record(fbm_map_schema()), the type of an fbm_map field.
[[nodiscard]] data::Type fbm_map_type();

// Schema of record `fbm_m`, fields in this declaration order:
// wavelength_m int(64, 65536), octaves int(1, 10), gain fx(0.0625, 1), all
// required (P1-01 4.1).
[[nodiscard]] data::Schema fbm_m_schema();
// record(fbm_m_schema()), the type of an fbm_m field.
[[nodiscard]] data::Type fbm_m_type();

// The FbmMap held by a validated fbm_map record value. Fatal (FatalError)
// if the value is not a record with those fields.
[[nodiscard]] FbmMap fbm_map_from(const data::Value& value);
// The FbmM held by a validated fbm_m record value. Fatal as above.
[[nodiscard]] FbmM fbm_m_from(const data::Value& value);

}  // namespace worldgen::geo
