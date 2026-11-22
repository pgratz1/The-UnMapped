// P3-02 r17 section 15 (P5-01, generator 0.21.0), tests 15.4: the filter entries of 15.1 (fag anywhere, shiva and
// hindi in famous.whole, fannie in crude.whole) and the two dwarven stutters of 15.2 (forbidden didin and ririn).
//
// Expected values are written from the spec (15.1's names, 15.4's roots) or read from
// worldgen/tests/golden/p3-02-names.txt (lines dattempt, dresult, dresult15, dvalid), written by the test writer's
// reference p3_names_reference.py from the spec. The critics' stems are written as concatenations (P3-02 11.7 item
// 10); the golden file stores none of them. Written before the implementation (test-writer rule): the filter cases
// pass once the r17 filter files land, the dwarven cases once the library reads the new forbidden entries.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"

using names_api::Kind;
using names_api::Naming;
using names_test::dec_u;
using names_test::golden_lines;
using names_test::kind_of;
using names_test::shipped_naming;
using names_test::split;

namespace {

// The r15 dwarven naming (0.20.0): the shipped one without the two entries of 15.2.
Naming dwarven_r15() {
    Naming n = shipped_naming("dwarven");
    REQUIRE(n.forbidden.size() == 15U);
    n.forbidden.resize(13);
    return n;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-02 r17 15.1, 15.4: Shyfaglae and Fagwick are filtered; Fagwick also by r15's at_start, Shyfaglae only from r17") {
    const names_api::Filter& f = names_test::shipped_filter();
    const std::string frag = std::string("f") + "ag";
    const std::string inside = "shy" + frag + "lae";
    const std::string start = frag + "wick";
    CHECK(names_api::name_filtered(f, inside));
    CHECK(names_api::name_filtered(f, start));
    CHECK(names_test::ref_filtered(f, inside));
    // r15's lists: fag at_start only, fagg anywhere
    const names_api::Filter r15{{frag + "g"}, {frag}, {}};
    CHECK(names_api::name_filtered(r15, start));
    CHECK_FALSE(names_api::name_filtered(r15, inside));
    // the root word of a faerie settlement {root} and a human one: the whole root
    CHECK(names_api::root_word("{root}", inside) == inside);
    for (const char* w : {"fawick", "flag", "gaff", "frag"}) {   // no fragment, or not the letters f a g in a row
        CAPTURE(std::string_view(w));
        CHECK_FALSE(names_api::name_filtered(f, w));
    }
}

TEST_CASE("P3-02 r17 15.1, 15.4: Shiva, The Hindi Court and Fannie Glade are filtered (whole root words)") {
    const names_api::Filter& f = names_test::shipped_filter();
    const std::string fannie = std::string("f") + "annie";
    CHECK(names_api::name_filtered(f, names_api::root_word("{root}", "shiva")));
    CHECK(names_api::root_word("The {root} Court", "hindi") == "hindi");
    CHECK(names_api::name_filtered(f, names_api::root_word("The {root} Court", "hindi")));
    CHECK(names_api::root_word("{root} Glade", fannie) == fannie);
    CHECK(names_api::name_filtered(f, names_api::root_word("{root} Glade", fannie)));
    // whole entries: inside a longer root word they pass
    for (const std::string& w : {std::string("shivae"), std::string("thindi"), "x" + fannie}) {
        CAPTURE(w);
        CHECK_FALSE(names_api::name_filtered(f, w));
    }
}

TEST_CASE("P3-02 r17 15.2, 15.4: dwarven roots didin and gririnhard are invalid by forbidden; dadin and kadin valid") {
    const Naming d = shipped_naming("dwarven");
    const Naming old = dwarven_r15();
    for (const char* root : {"didin", "gririnhard"}) {
        CAPTURE(std::string_view(root));
        CHECK_FALSE(names_test::root_valid(d, root));
        CHECK(names_test::root_valid(old, root));
    }
    for (const char* root : {"dadin", "kadin", "gromrin", "hodin", "brarin"}) {
        CAPTURE(std::string_view(root));
        CHECK(names_test::root_valid(d, root));
    }
    // the reference's own lines agree (dvalid)
    const std::vector<std::string>& lines = golden_lines("dvalid");
    REQUIRE(lines.size() == 4U);
    for (const std::string& l : lines) {
        // dvalid dwarven <root> <0|1>
        const std::vector<std::string> w = split(l, 4);
        CAPTURE(l);
        CHECK(names_test::root_valid(d, w[2]) == (w[3] == "1"));
    }
}

TEST_CASE("P3-02 r17 15.2, 15.4: the attempt drawing didin or ririn is invalid and attempt t + 1 follows (golden dattempt, dresult)") {
    const Naming d = shipped_naming("dwarven");
    const Naming old = dwarven_r15();
    const std::vector<std::string>& lines = golden_lines("dattempt");
    REQUIRE(lines.size() >= 6U);
    std::size_t stutters = 0;
    for (const std::string& l : lines) {
        // dattempt <seed> dwarven <kind> <c> <e> <t> <drawn root> <valid> <valid at r15> <name>
        const std::vector<std::string> w = split(l, 11);
        CAPTURE(l);
        const Kind k = kind_of(w[3]);
        const names_api::Attempt a =
            names_api::attempt(d, k, dec_u(w[1]), dec_u(w[4]), dec_u(w[5]), dec_u(w[6]), names_test::shipped_filter());
        const names_api::Attempt a15 =
            names_api::attempt(old, k, dec_u(w[1]), dec_u(w[4]), dec_u(w[5]), dec_u(w[6]), names_test::shipped_filter());
        CHECK(a.valid == (w[8] == "1"));
        CHECK(a15.valid == (w[9] == "1"));
        if (a.valid) CHECK(a.name == w[10]);
        if (a15.valid) CHECK(a15.name == w[10]);
        const bool stutter = w[7].find("didin") != std::string::npos || w[7].find("ririn") != std::string::npos;
        if (stutter) {
            ++stutters;
            CHECK_FALSE(a.valid);
        }
    }
    CHECK(stutters >= 4U);
    // make_name with an empty registry (4.3): from r17 a later attempt than r15's
    const std::vector<std::string>& res = golden_lines("dresult");
    const std::vector<std::string>& res15 = golden_lines("dresult15");
    REQUIRE(res.size() == res15.size());
    REQUIRE(res.size() >= 2U);
    for (std::size_t i = 0; i < res.size(); ++i) {
        // dresult <seed> dwarven <kind> <c> <e> <attempt|F> <name>
        const std::vector<std::string> w = split(res[i], 8);
        const std::vector<std::string> w15 = split(res15[i], 8);
        CAPTURE(res[i]);
        names_api::Registry reg, reg15;
        CHECK(names_api::make_name(d, "dwarven", kind_of(w[3]), dec_u(w[1]), dec_u(w[4]), dec_u(w[5]), reg,
                                   names_test::shipped_filter()) == w[7]);
        CHECK(names_api::make_name(old, "dwarven", kind_of(w15[3]), dec_u(w15[1]), dec_u(w15[4]), dec_u(w15[5]), reg15,
                                   names_test::shipped_filter()) == w15[7]);
        const std::string low = names_test::lower(w[7]);
        CHECK(low.find("didin") == std::string::npos);
        CHECK(low.find("ririn") == std::string::npos);
    }
}

}  // TEST_SUITE
