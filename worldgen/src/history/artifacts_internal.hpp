#pragma once

// Helpers shared by artifacts.cpp and artifacts_moves.cpp (spec P5-11); not
// a public header.

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "worldgen/history/engine.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::history::artifacts_detail {

// The streams of 3 (121 to 126), index into ArtifactsSubsystem's keys.
enum ArtifactStream : std::size_t { MAKE = 0, CLASS, KIND, TAKE, BURY, RAID };

// Entity type codes of the holder columns (6.2).
inline constexpr std::int64_t CIV_TYPE = static_cast<std::int64_t>(EntityType::civilization);
inline constexpr std::int64_t SETTLEMENT_TYPE = static_cast<std::int64_t>(EntityType::settlement);
inline constexpr std::int64_t REGION_TYPE = static_cast<std::int64_t>(EntityType::region);
inline constexpr std::int64_t FIGURE_TYPE = static_cast<std::int64_t>(EntityType::figure);
inline constexpr std::int64_t MONSTER_TYPE = static_cast<std::int64_t>(EntityType::monster);
inline constexpr std::int64_t SITE_TYPE = static_cast<std::int64_t>(EntityType::site);

[[nodiscard]] inline Participant artifact_role(std::int64_t artifact) {
    return Participant{"artifact", EntityType::artifact, artifact};
}

[[nodiscard]] inline Participant figure_role(std::string_view role, std::int64_t figure) {
    return Participant{role, EntityType::figure, figure};
}

[[nodiscard]] inline Participant monster_role(std::int64_t monster) {
    return Participant{"monster", EntityType::monster, monster};
}

}  // namespace worldgen::history::artifacts_detail
