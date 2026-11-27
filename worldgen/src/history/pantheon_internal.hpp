#pragma once

// Helpers shared by pantheon.cpp and pantheon_cults.cpp (spec P5-08); not a
// public header.

#include <cstddef>

namespace worldgen::history::pantheon_detail {

// The streams of 3 (108 to 118), index into PantheonSubsystem's keys.
enum PantheonStream : std::size_t {
    ACT = 0,
    KIND,
    FLARE,
    MONSTER_KIND,
    SITE_NAME,
    CULT_FOUND,
    CULT_NAME,
    CULT_SPREAD,
    ADOPT,
    SUPPRESS,
    HIDE,
};

}  // namespace worldgen::history::pantheon_detail
