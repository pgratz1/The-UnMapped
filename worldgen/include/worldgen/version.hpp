#pragma once

#include <string_view>

namespace worldgen {

// The generator version (spec P0-02 section 5.5), recorded as
// meta.generator_version in every world.db. Format MAJOR.MINOR.PATCH,
// decimal, no leading zeros. Changed by hand: any merged change that alters
// output bytes for any input bumps it, and the golden hashes are updated in
// the same branch. Never derived from git, the build date or the compiler.
// 0.1.0 until seed phrases were scrubbed (P0-02 amendment 12); 0.2.0 until
// the first geography stage (elevation, P1-01) and the `worldgen` data kind
// (P1-00 6.4 rule 4); 0.3.0 until the hydrology stage (P1-02) joined the
// default pipeline; 0.4.0 until the hydrology tie-break on flats (P1-02 H2,
// stream geo.hydrology.flat); 0.5.0 until the elevation passes and final
// talus (P1-01 fourth version, schema 2); 0.6.0 until the climate stage
// (P1-03) joined the default pipeline; 0.7.0 until the biome stage (P1-04)
// joined it with schema 3 (tables biome and climate); 0.8.0 until the biome
// variety revision (P1-04 third version, P1-03 C2b, schema 4); 0.9.0 until
// the resources stage (P1-05) joined it; 0.10.0 until the regions stage
// (P1-06) joined it with schema 5 (tables region, region_border and
// region_biome); 0.11.0 until the history data kinds race, culture and
// history (P3-01) entered every world's content hash; 0.12.0 until the
// chronology stage (P3-03) joined it with schema 6 (tables race,
// race_biome, culture, god and age, eight history meta keys); 0.13.0 until
// the history stage (P3-04, the engine with stub subsystems) joined it with
// schema 7 (thirteen history tables, meta key history_end_year); 0.14.0
// until the peoples subsystem (P3-05: awakenings, growth, foundings,
// pressures, secession; event kinds 6 to 19, tuning history.peoples, race
// field density_permille) replaced the stubs; now 0.15.0.
inline constexpr std::string_view GENERATOR_VERSION = "0.15.0";

// Returns GENERATOR_VERSION (P0-02 5.5). A world is only guaranteed
// reproducible under the same generator version (design doc section 4).
[[nodiscard]] std::string_view generator_version() noexcept;

}  // namespace worldgen
