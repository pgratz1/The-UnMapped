#pragma once

// Helpers shared by conflict.cpp and conflict_war.cpp (spec P3-06); not a
// public header.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/history/conflict.hpp"
#include "worldgen/history/engine.hpp"

namespace worldgen::history::conflict_detail {

[[noreturn]] void fail(const std::string& message);
[[nodiscard]] std::string int_text(std::int64_t v);
[[nodiscard]] bool standing(const Settlement& s) noexcept;
[[nodiscard]] std::uint64_t word(std::int64_t v) noexcept;
// chance_pm of P3-05 3.3.
[[nodiscard]] bool chance_pm(std::uint64_t u, std::int64_t p);
[[nodiscard]] EventSpec point_event(const StepContext& ctx, std::string_view kind, std::int64_t region,
                                    std::int64_t settlement);
[[nodiscard]] Participant civ_role(std::string_view role, std::int64_t civ);
[[nodiscard]] std::int64_t settlement_count(const State& state) noexcept;
[[nodiscard]] std::int64_t civ_count(const State& state) noexcept;
// r13 20.27: the ids of every standing settlement of civ in region, visible,
// hidden or created earlier in the step, ascending (ownership is a fact).
[[nodiscard]] std::vector<std::int64_t> standing_settlements_in(const State& state, std::int64_t civ,
                                                               std::int64_t region);
[[nodiscard]] ConflictSubsystem::Pair ordered(std::int64_t a, std::int64_t b) noexcept;
// A relation between two civilizations, as given; the symmetric form
// orders the pair (lower id first, 8.3 item 6).
[[nodiscard]] RelationKey civ_relation(std::int64_t a, std::int64_t b, std::string_view kind);
[[nodiscard]] RelationKey symmetric_relation(std::int64_t a, std::int64_t b, std::string_view kind);

// The streams of 3.3 (and r10's search), index into ConflictSubsystem's keys.
enum ConflictStream : std::size_t { DECLARE = 0, BATTLE, BATTLE_WIN, SIEGE, SACK, SEARCH };

}  // namespace worldgen::history::conflict_detail
