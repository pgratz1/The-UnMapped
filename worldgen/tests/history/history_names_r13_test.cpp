// P3-02 r13 section 13 (P3-10 second round, generator stays 0.19.0): the filter entries of 13.1 (cum, fass, pee,
// poo, tit inside a word, data) and the triple-letter rule of 13.2 (an attempt whose rendered name holds the same
// letter a-z at three consecutive positions, case-insensitive, is invalid; space, ' and - break a run; the
// fallback's Roman numeral is not tested), with invariant 7 of 13.3 on the golden lists.
//
// Expected values are written from the spec (13.1's names, 13.2's examples) or read from
// worldgen/tests/golden/p3-02-names.txt (lines triple, tattempt, tresult; the lists of 9.3 and r10's core lists),
// written by the test writer's reference p3_names_reference.py. The test cultures trip and trip_hollow are defined
// in the reference and in p3_02_names_support.hpp. The critics' stems are written as concatenations (P3-02 11.7
// item 10). The filter cases fail until the r13 filter files land (game/data/name_filters/ and both fixture
// copies); the attempt and list cases fail until the library has the triple rule.

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"

using names_api::Kind;
using names_api::Registry;
using names_test::dec_u;
using names_test::golden_lines;
using names_test::kind_of;
using names_test::shipped_naming;
using names_test::split;

namespace {

// 13.2, the test's own: lower(n) holds one letter a-z at three consecutive positions.
bool own_triple(std::string_view n) {
    const std::string l = names_test::lower(n);
    for (std::size_t i = 0; i + 2 < l.size(); ++i) {
        if (l[i] >= 'a' && l[i] <= 'z' && l[i] == l[i + 1] && l[i] == l[i + 2]) return true;
    }
    return false;
}

// The name of a list line "<seed> <culture> <kind> <entity> <attempt|F> <name>" without the fallback's numeral.
std::string attempt_name_of(const std::string& line, std::size_t fields) {
    const std::vector<std::string> w = split(line, fields);
    std::string n = w.back();
    if (w[w.size() - 2] == "F") n = n.substr(0, n.rfind(' '));
    return n;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-02 r13 13.1, 13.3: the root words of the critics' names are filtered; benign words pass") {
    const names_api::Filter& f = names_test::shipped_filter();
    // Twipee, Twypoo, Twotitwee, Fassbou Hollow (W fassbou), Beamcumstead, and validation's Teefass
    for (const std::string& w : {std::string("twi") + "pee", std::string("twy") + "poo", std::string("twoti") + "twee",
                                std::string("fass") + "bou", std::string("beamcum") + "stead",
                                std::string("tee") + "fass", std::string("my") + "pee", std::string("din") + "cum"}) {
        CAPTURE(w);
        CHECK(names_api::name_filtered(f, w));
    }
    for (const char* w : {"billa", "biggnizz", "gramm", "mammga", "spon", "dincer", "aazaam"}) {
        CAPTURE(std::string_view(w));
        CHECK_FALSE(names_api::name_filtered(f, w));
    }
}

TEST_CASE("P3-02 r13 13.2: the triple test (own) on 13.2's examples and the golden triple lines") {
    for (const char* n : {"Wasssie", "Floullly Hollow", "Fiasssie", "The Bosssie Ring", "AAAb"}) CHECK(own_triple(n));
    for (const char* n : {"Bigg-Gnizz", "Aa'azaam", "Billa Hollow", "Aa a", "Kaa-aar", "Mammga"}) CHECK_FALSE(own_triple(n));
    const std::vector<std::string>& lines = golden_lines("triple");
    REQUIRE(lines.size() >= 10U);
    for (const std::string& l : lines) {
        const std::vector<std::string> w = split(l, 3);
        CAPTURE(l);
        CHECK(own_triple(w[2]) == (w[1] == "1"));
    }
}

TEST_CASE("P3-02 r13 13.2: attempts of the test cultures: a triple-letter name is an invalid attempt (golden tattempt)") {
    const std::vector<std::string>& lines = golden_lines("tattempt");
    REQUIRE(lines.size() == 288U);  // 2 cultures x 3 kinds x 2 seeds x 3 entities x 8 attempts
    std::set<std::pair<bool, bool>> seen;
    for (const std::string& l : lines) {
        // tattempt <culture> <kind> <seed> <c> <e> <t> <valid> <triple> <name>
        const std::vector<std::string> w = split(l, 10);
        CAPTURE(l);
        const names_api::Attempt a = names_api::attempt(shipped_naming(w[1]), kind_of(w[2]), dec_u(w[3]), dec_u(w[4]),
                                                        dec_u(w[5]), dec_u(w[6]), names_test::shipped_filter());
        const bool valid = w[7] == "1", triple = w[8] == "1";
        CHECK(a.valid == valid);
        if (valid) CHECK(a.name == w[9]);
        CHECK(own_triple(w[9]) == triple);  // every attempt of these cultures passes the r4 rules and the filter
        CHECK(valid == !triple);
        seen.insert({valid, triple});
    }
    CHECK(seen.size() == 2U);
}

TEST_CASE("P3-02 r13 13.2: a name skips triple-letter attempts and takes the next attempt (golden tresult)") {
    const std::vector<std::string>& lines = golden_lines("tresult");
    REQUIRE(lines.size() == 36U);
    bool skipped = false;
    for (const std::string& l : lines) {
        // tresult <culture> <kind> <seed> <c> <e> <attempt|F> <name>
        const std::vector<std::string> w = split(l, 8);
        CAPTURE(l);
        Registry reg;
        const names_api::Choice c = names_api::choose_name(shipped_naming(w[1]), w[1], kind_of(w[2]), dec_u(w[3]),
                                                           dec_u(w[4]), dec_u(w[5]), reg, names_test::shipped_filter());
        CHECK(c.name == w[7]);
        CHECK_FALSE(own_triple(c.name));
        if (w[6] != "F") {
            CHECK(c.attempt == dec_u(w[6]));
            skipped = skipped || dec_u(w[6]) > 0;
        }
    }
    CHECK(skipped);  // some name took a later attempt for a triple
}

TEST_CASE("P3-02 r13 13.2: Wasssie, Fiasssie (a god) and Floullly Hollow are invalid; Bigg-Gnizz, Aa'azaam, Billa Hollow valid") {
    // every attempt of trip and trip_hollow renders one onset; over 24 attempts per kind each onset is drawn, and
    // only the names without a run of three equal letters are ever valid
    const names_api::Filter& f = names_test::shipped_filter();
    std::set<std::string> valid;
    std::size_t invalid = 0;
    for (const char* cid : {"trip", "trip_hollow"}) {
        for (const Kind k : {Kind::god, Kind::civilization, Kind::settlement}) {
            for (std::uint64_t t = 0; t < 24; ++t) {
                const names_api::Attempt a = names_api::attempt(shipped_naming(cid), k, 0, 1, 1, t, f);
                if (a.valid) {
                    valid.insert(a.name);
                } else {
                    ++invalid;
                }
            }
        }
    }
    for (const char* n : {"Bigg-Gnizz", "Aa'azaam", "Billa Hollow", "Billa"}) {
        CAPTURE(std::string_view(n));
        CHECK(valid.count(n) == 1U);
    }
    for (const char* n : {"Wasssie", "Fiasssie", "Floullly Hollow", "Floullly"}) CHECK(valid.count(n) == 0U);
    CHECK(invalid > 0U);
}

TEST_CASE("P3-02 r13 13.3: the fallback's numeral is not tested (Da III with Ba, Da and Da II taken)") {
    Registry reg;
    for (const char* n : {"Ba", "Da", "Da II"}) reg.add(n);
    const names_api::Choice c =
        names_api::choose_name(shipped_naming("tiny"), "tiny", Kind::settlement, 0, 1, 1, reg, names_test::shipped_filter());
    CHECK(c.fallback);
    CHECK(c.name == "Da III");
    CHECK(own_triple(c.name));  // the string holds III; the rule tests attempts only
}

TEST_CASE("P3-02 r13 13.3 invariant 7: no name of the golden lists holds three equal letters (numerals aside)") {
    std::size_t n = 0;
    for (const std::string& l : golden_lines("name")) {
        CAPTURE(l);
        CHECK_FALSE(own_triple(attempt_name_of(l, 6)));
        ++n;
    }
    for (const std::string& l : golden_lines("core")) {
        CAPTURE(l);
        CHECK_FALSE(own_triple(attempt_name_of(l, 7)));
        ++n;
    }
    CHECK(n >= 1800U);
}

}  // TEST_SUITE
