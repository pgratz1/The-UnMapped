// Engineer-side checks of the P3-11 change (generator 0.20.0), written by the
// generator engineer while implementing; the change's tests proper are the
// test writer's (docs/specs/p3-11-followups.md section 6).

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "worldgen/history/names.hpp"

namespace wh = worldgen::history;

TEST_CASE("P3-11 engineer: P3-02 r15 14.2's table of repeated syllables") {
    using V = std::vector<std::string>;
    CHECK(wh::repeated_syllable("ONONON", V{"w", "i", "fl", "ee", "fl", "ee"}));   // wifleeflee
    CHECK(wh::repeated_syllable("ONONC", V{"l", "i", "l", "i", "n"}));             // lilin
    CHECK(wh::repeated_syllable("ONON", V{"f", "i", "f", "i"}));                   // fifi
    CHECK(wh::repeated_syllable("ONON", V{"f", "ee", "f", "ee"}));                 // feefee
    CHECK(wh::repeated_syllable("ONON", V{"n", "o", "n", "o"}));                   // nono
    CHECK(wh::repeated_syllable("ONCONC", V{"d", "a", "ck", "d", "a", "ck"}));     // dackdack
    CHECK(wh::repeated_syllable("ONC-ONC", V{"kr", "a", "k", "kr", "a", "k"}));    // krak-krak
    CHECK_FALSE(wh::repeated_syllable("ONCONC", V{"gr", "a", "n", "gr", "a", "k"}));  // grangrak
    CHECK_FALSE(wh::repeated_syllable("ONON", V{"f", "ee", "f", "i"}));              // feefi
    CHECK_FALSE(wh::repeated_syllable("ONONON", V{"tw", "i", "wh", "i", "w", "ou"}));  // twiwhiwou
    CHECK_FALSE(wh::repeated_syllable("ONCON", V{"l", "i", "ll", "", "ia"}));        // lillia
    // OONC: syllables "" and "lin"; s_1 empty, so not repeated.
    CHECK_FALSE(wh::repeated_syllable("OONC", V{"", "l", "i", "n"}));
    // NCONC (shipped shape): syllables NC and ONC.
    CHECK(wh::repeated_syllable("NCONC", V{"a", "", "", "a", "n"}));
    CHECK(wh::repeated_syllable("NCONC", V{"a", "n", "", "a", "n"}));  // "an", "an"
}
