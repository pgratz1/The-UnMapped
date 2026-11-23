// Names (spec P3-02 4.2 to 4.6): the worked example attempt by attempt, the
// collision and fallback cases, the seed-42 vectors, the validity rules,
// capitalization, the fatal case, Roman numerals, the registry scenarios
// and the golden name lists of 9.3 item 10 with their SHA-256, and the
// sample table of 5.1. Spec 9.1 items 3 to 6 and 9.3 item 10.
//
// Golden values come from worldgen/tests/golden/p3-02-names.txt, written by
// worldgen/tests/reference/p3_names_reference.py (an independent reference
// written from the spec). Where 4.6's tuples fall in it: its seed-0 elven
// settlement 1 is the list line "0 elven settlement 1 0 Iensath" (by then
// the seed-0 registry holds the 250 names before it: dwarven 150, elven god
// and civilization 100, none of them Iensath); its empty-registry, collision and
// fallback cases are the scenarios "empty", "collide" and "full"; its
// seed-42 vectors are the scenario "seed42". The library is reached only
// through p3_02_names_api.hpp.
//
// From generator 0.17.0 (P3-08) the golden file is r5's (P3-02 11): the
// attempt, step, scenario and list lines are drawn with god_patterns and the
// shipped name filter (read here independently from game/data/name_filters/
// with toml++), and the file adds the filter lines of 11.7 (fattempt,
// rootword, filter, the scenarios filtered, redraw_alone and redraw_after,
// r4sha256). Tests of r5 behaviour fail until the library implements it;
// the seams of 11.5 (root_word, name_filtered, the filter argument) are used
// only with UNMAPPED_P3_08_API 1 (p3_08_switch.hpp) and otherwise throw
// names_api::ApiMissing. The critics' stems are written as concatenations of
// their parts, never whole (P3-02 11.7 item 10).

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/sha256.hpp"

using names_api::Kind;
using names_api::Naming;
using names_api::Registry;
using names_test::culture_number;
using names_test::dec_u;
using names_test::golden_lines;
using names_test::hex;
using names_test::kind_of;
using names_test::shipped_naming;
using names_test::split;

namespace {

// Checks 4.3's loop independently of make_name, before it runs: the result
// `name` with attempt token `tok` ("F" or a number) is what 4.3 gives on the
// registry `before`, judged with name_attempt alone.
void check_attempt_rule(const Naming& n, Kind k, std::uint64_t seed, std::uint64_t c, std::uint64_t e,
                        const Registry& before, const std::string& tok, const std::string& name,
                        const names_api::Filter& f = names_api::Filter{}) {
    CAPTURE(tok);
    CAPTURE(name);
    std::string first;
    bool have_first = false;
    for (std::uint64_t t = 0; t < 64; ++t) {
        const names_api::Attempt a = names_api::attempt(n, k, seed, c, e, t, f);
        if (!a.valid) continue;
        if (!have_first) {
            first = a.name;
            have_first = true;
        }
        if (tok != "F" && t == dec_u(tok)) {
            CHECK(a.name == name);
            CHECK_FALSE(before.contains(a.name));
            return;
        }
        // Every earlier valid attempt collided.
        CHECK(before.contains(a.name));
    }
    REQUIRE(tok == "F");
    REQUIRE(have_first);
    std::uint64_t kk = 2;
    while (before.contains(first + " " + names_test::roman_ref(kk))) ++kk;
    CHECK(name == first + " " + names_test::roman_ref(kk));
}

Registry copy_of(const std::set<std::string>& names) {
    Registry r;
    for (const std::string& s : names) r.add(s);
    return r;
}

}  // namespace

TEST_SUITE("history-names") {

TEST_CASE("P3-02 9.1 test 3: the worked example of 4.6, attempt 0, draw by draw") {
    const Naming elven = shipped_naming("elven");
    const std::uint64_t s = worldgen::stream_key(0, "hist.names.settlement");
    REQUIRE(s == 0x969C418DDF3C5EDCULL);
    struct Row {
        std::uint64_t j, u, index, n;
        std::string piece;
    };
    const std::vector<Row> rows = {{0, 0xAB9A949FA699954CULL, 2, 4, "{root}"}, {1, 0xF77E9ED830C1A5E0ULL, 4, 5, "NCONC"},
                                   {2, 0xAF071A40E08E8E51ULL, 4, 6, "ie"},     {3, 0x5EAF3E5C2245C62DULL, 3, 10, "n"},
                                   {4, 0x42F8D74C06F27F04ULL, 3, 13, "s"},     {5, 0x109285B9E64CA453ULL, 0, 6, "a"},
                                   {6, 0x7347EA42EAED2BB2ULL, 4, 10, "th"}};
    const std::vector<const std::vector<std::string>*> arrays = {&elven.settlement_forms, &elven.patterns,
                                                                 &elven.nuclei,          &elven.codas,
                                                                 &elven.onsets,          &elven.nuclei,
                                                                 &elven.codas};
    std::string root;
    for (const Row& r : rows) {
        CAPTURE(r.j);
        const std::uint64_t u = names_api::draw(s, 0, 2, 1, r.j);
        CHECK(u == r.u);
        CHECK(arrays[r.j]->size() == r.n);
        CHECK(names_api::pick(u, r.n) == r.index);
        CHECK((*arrays[r.j])[r.index] == r.piece);
        if (r.j >= 2) root += r.piece;
    }
    CHECK(root == "iensath");
    const names_api::Attempt a = names_api::attempt(elven, Kind::settlement, 0, 2, 1, 0);
    CHECK(a.valid);
    CHECK(a.name == "Iensath");
}

TEST_CASE("P3-02 4.6: the golden attempt and step lines (4.6 and the seed-42 tuples), through the seams") {
    const auto& attempts = golden_lines("attempt");
    const auto& steps = golden_lines("step");
    REQUIRE(attempts.size() == 9);
    REQUIRE(steps.size() >= 9 * 5);
    for (const std::string& line : attempts) {
        CAPTURE(line);
        const auto t = split(line, 10);
        const Naming n = shipped_naming(t[2]);
        const Kind k = kind_of(t[3]);
        const names_api::Attempt a = names_api::attempt(n, k, dec_u(t[1]), dec_u(t[4]), dec_u(t[5]), dec_u(t[6]),
                                                        names_test::shipped_filter());
        CHECK(a.valid == (t[7] == "1"));
        // An invalid attempt's name and root are unspecified (4.2): compared only when valid.
        if (a.valid) {
            CHECK(a.name == t[9]);
            CHECK(a.root == t[8]);
        }
    }
    for (const std::string& line : steps) {
        CAPTURE(line);
        const auto t = split(line, 13);
        const std::uint64_t key = worldgen::stream_key(dec_u(t[1]), names_test::stream_of(kind_of(t[3])));
        const std::uint64_t u = names_api::draw(key, dec_u(t[6]), dec_u(t[4]), dec_u(t[5]), dec_u(t[7]));
        CHECK(u == hex(t[8]));
        CHECK(names_api::pick(u, dec_u(t[9])) == dec_u(t[10]));
    }
}

TEST_CASE("P3-02 4.6, 9.1 test 3: collision gives Focoth (attempt 1); 61 names taken give Iensath II") {
    const Naming elven = shipped_naming("elven");
    {
        Registry r;
        CHECK(names_api::make_name(elven, "elven", Kind::settlement, 0, 2, 1, r) == "Iensath");
        CHECK(r.contains("Iensath"));
    }
    {
        Registry r;
        r.add("Iensath");
        CHECK(names_api::make_name(elven, "elven", Kind::settlement, 0, 2, 1, r) == "Focoth");
        CHECK(r.contains("Focoth"));
        CHECK(names_api::attempt(elven, Kind::settlement, 0, 2, 1, 1).name == "Focoth");
    }
    {
        std::set<std::string> valid;
        for (std::uint64_t t = 0; t < 64; ++t) {
            const names_api::Attempt a = names_api::attempt(elven, Kind::settlement, 0, 2, 1, t);
            if (a.valid) valid.insert(a.name);
        }
        // 4.6 counts 61 distinct valid names; from 0.20.0 (P3-02 r15 14.2) one of them comes from an attempt whose
        // syllables repeat, so 60 remain (the reference's check "4.6 distinct valid names")
        CHECK(valid.size() == 60);
        Registry r = copy_of(valid);
        CHECK(names_api::make_name(elven, "elven", Kind::settlement, 0, 2, 1, r) == "Iensath II");
        CHECK(r.contains("Iensath II"));
        // Again, with Iensath II taken: the numeral counts on.
        CHECK(names_api::make_name(elven, "elven", Kind::settlement, 0, 2, 1, r) == "Iensath III");
    }
    {
        // The registry compares exact bytes: a lower-case "iensath" does not collide.
        Registry r;
        r.add("iensath");
        r.add("Iensath ");
        CHECK(names_api::make_name(elven, "elven", Kind::settlement, 0, 2, 1, r) == "Iensath");
    }
}

TEST_CASE("P3-02 4.6, 11.6 (r5): the seed-42 vectors in one registry, in order (god Khorkhound, r4 Khork)") {
    const names_api::Filter& f = names_test::shipped_filter();
    Registry r;
    CHECK(names_api::make_name(shipped_naming("dwarven"), "dwarven", Kind::god, 42, 1, 1, r, f) == "Khorkhound");
    CHECK_FALSE(names_api::attempt(shipped_naming("goblin"), Kind::civilization, 42, 4, 3, 0, f).valid);
    CHECK(names_api::make_name(shipped_naming("goblin"), "goblin", Kind::civilization, 42, 4, 3, r, f) == "The Grork Horde");
    CHECK(names_api::make_name(shipped_naming("human"), "human", Kind::settlement, 42, 5, 17, r, f) == "Deantwick");
    CHECK(names_api::make_name(shipped_naming("orcish"), "orcish", Kind::settlement, 42, 6, 200, r, f) == "Ol'uraakh Fort");
    // r4 (11.7 item 7): with the empty filter and god_patterns = patterns, the r4 vector.
    Registry r4;
    CHECK(names_api::make_name(names_test::r4_equivalent("dwarven"), "dwarven", Kind::god, 42, 1, 1, r4) == "Khork");
}

// --- 9.1 test 4: validity rules on hand-made data ------------------------------------------------------

namespace {

// One element per array, so every attempt makes the same root.
Naming fixed(std::string onset, std::string nucleus, std::string coda, std::string pattern, std::int64_t lo,
             std::int64_t hi) {
    Naming n;
    n.onsets = {std::move(onset)};
    n.nuclei = {std::move(nucleus)};
    n.codas = {std::move(coda)};
    n.patterns = {std::move(pattern)};
    n.min_letters = lo;
    n.max_letters = hi;
    n.god_forms = {"{root}"};
    n.civilization_forms = {"{root}"};
    n.settlement_forms = {"{root}"};
    return n;
}

bool valid0(const Naming& n) {
    bool v = names_api::attempt(n, Kind::god, 0, 1, 1, 0).valid;
    // The root does not depend on the draws here, so every attempt agrees.
    for (std::uint64_t t = 1; t < 4; ++t) CHECK(names_api::attempt(n, Kind::god, 9, 3, 7, t).valid == v);
    return v;
}

}  // namespace

TEST_CASE("P3-02 9.1 test 4: letter bounds, inclusive, separators not counted") {
    CHECK(valid0(fixed("b", "a", "c", "ONC", 3, 3)));         // "bac": 3 letters
    CHECK_FALSE(valid0(fixed("b", "a", "c", "ONC", 4, 10)));  // below min_letters
    CHECK_FALSE(valid0(fixed("b", "a", "c", "ONC", 1, 2)));   // above max_letters
    CHECK(valid0(fixed("b", "a", "c", "ONC", 1, 3)));
    CHECK(valid0(fixed("b", "a", "c", "ON'NC", 4, 4)));       // "ba'ac": 4 letters, 5 bytes
    CHECK_FALSE(valid0(fixed("b", "a", "c", "ON'NC", 5, 5)));
    CHECK(valid0(fixed("b", "a", "c", "ON-NC", 1, 4)));
    CHECK(names_api::attempt(fixed("b", "a", "c", "ON-NC", 1, 4), Kind::god, 0, 1, 1, 0).name == "Ba-Ac");
}

TEST_CASE("P3-02 9.1 test 4: a forbidden substring makes the attempt invalid, across syllables too") {
    Naming n = fixed("b", "a", "c", "ONC", 1, 10);
    n.forbidden = {"ac"};
    CHECK_FALSE(valid0(n));
    n.forbidden = {"bac"};
    CHECK_FALSE(valid0(n));
    n.forbidden = {"ca", "bb", "x"};
    CHECK(valid0(n));
    // Across a separator: "ba-ac" contains "a-a"? only letters are syllables; "aa" is not a substring.
    Naming m = fixed("b", "a", "c", "ON-NC", 1, 10);
    m.forbidden = {"aa"};
    CHECK(valid0(m));
    m.forbidden = {"ac"};
    CHECK_FALSE(valid0(m));
}

TEST_CASE("P3-02 9.1 test 4: a root starting or ending with ' or -, or with two separators adjacent, is invalid") {
    CHECK_FALSE(valid0(fixed("", "a", "", "O'N", 1, 10)));    // "'a"
    CHECK_FALSE(valid0(fixed("", "a", "", "O-N", 1, 10)));    // "-a"
    CHECK_FALSE(valid0(fixed("", "a", "", "N-C", 1, 10)));    // "a-"
    CHECK_FALSE(valid0(fixed("", "a", "", "N'C", 1, 10)));    // "a'"
    CHECK_FALSE(valid0(fixed("", "a", "", "N'C-N", 1, 10)));  // "a'-a"
    CHECK_FALSE(valid0(fixed("", "a", "", "N-O-N", 1, 10)));  // "a--a"
    CHECK_FALSE(valid0(fixed("", "a", "", "N'O'N", 1, 10)));  // "a''a"
    CHECK(valid0(fixed("x", "a", "", "N'ON", 1, 10)));        // "a'xa"
    CHECK(valid0(fixed("x", "a", "y", "NC-ON", 1, 10)));      // "ay-xa"
}

TEST_CASE("P3-02 9.1 test 4: a name over 48 bytes is invalid; 48 bytes is valid") {
    // a 48-byte form "{root}xyxy...xy"; P3-02 r13 13.2: no three equal letters in a row in the fixture
    std::string tail;
    for (int i = 0; i < 21; ++i) tail += "xy";
    Naming n = fixed("", "abcabc", "", "N", 1, 32);
    n.god_forms = {"{root}" + tail};
    REQUIRE(n.god_forms[0].size() == 48);
    names_api::Attempt a = names_api::attempt(n, Kind::god, 0, 1, 1, 0);
    CHECK(a.valid);  // 6 + 42 = 48 bytes
    CHECK(a.name.size() == 48);
    n.nuclei = {"abcabca"};
    a = names_api::attempt(n, Kind::god, 0, 1, 1, 0);
    CHECK_FALSE(a.valid);  // 49 bytes
    // The bound is on the name, not on the root: the same root in a short form is valid.
    n.god_forms = {"{root}"};
    CHECK(names_api::attempt(n, Kind::god, 0, 1, 1, 0).valid);
}

TEST_CASE("P3-02 4.2: each kind uses its own forms and its own stream") {
    Naming n = fixed("b", "a", "", "ON", 1, 10);
    n.god_forms = {"God {root}"};
    n.civilization_forms = {"The {root} Realm"};
    n.settlement_forms = {"{root}ford"};
    CHECK(names_api::attempt(n, Kind::god, 0, 1, 1, 0).name == "God Ba");
    CHECK(names_api::attempt(n, Kind::civilization, 0, 1, 1, 0).name == "The Ba Realm");
    CHECK(names_api::attempt(n, Kind::settlement, 0, 1, 1, 0).name == "Baford");
    // The forms are picked by u(0) of the kind's stream: with two forms, the choice follows pick(u(0), 2).
    n.settlement_forms = {"{root}ford", "{root}ham"};
    for (std::uint64_t e = 1; e <= 20; ++e) {
        const std::uint64_t u0 = names_api::draw(worldgen::stream_key(5, "hist.names.settlement"), 0, 3, e, 0);
        const std::string expect = names_api::pick(u0, 2) == 0 ? "Baford" : "Baham";
        CHECK(names_api::attempt(n, Kind::settlement, 5, 3, e, 0).name == expect);
    }
}

// --- 9.1 test 5: capitalization -------------------------------------------------------------------

TEST_CASE("P3-02 9.1 test 5: Cap upper-cases the first letter and every letter after '-', not after '\''") {
    CHECK(names_api::attempt(fixed("gn", "i", "nk", "ONC-ONC", 1, 20), Kind::god, 0, 1, 1, 0).name == "Gnink-Gnink");
    CHECK(names_api::attempt(fixed("ur", "u", "ld", "ONC'ONC", 1, 20), Kind::god, 0, 1, 1, 0).name == "Uruld'uruld");
    CHECK(names_api::attempt(fixed("", "iensath", "", "N", 1, 20), Kind::god, 0, 1, 1, 0).name == "Iensath");
    // Inside a form the fixed text is unchanged; only the root is capitalized.
    Naming n = fixed("sn", "u", "nk", "ONC-ONC", 1, 20);
    n.god_forms = {"the {root} of x-y"};
    CHECK(names_api::attempt(n, Kind::god, 0, 1, 1, 0).name == "the Snunk-Snunk of x-y");
    // The spec's own examples, from the 5.1 sample protocol (seed 0, goblin
    // settlement 3, orcish settlement 5, elven settlement 1).
    CHECK(names_test::cap("gnink-snunk") == "Gnink-Snunk");
    CHECK(names_test::cap("uruld'duuld") == "Uruld'duuld");
    for (const std::string& line : golden_lines("cap")) {
        const auto t = split(line, 3);
        CHECK(names_test::cap(t[1]) == t[2]);  // the test's Cap agrees with the reference's
    }
}

// --- 9.1 test 6: fatal ----------------------------------------------------------------------------

TEST_CASE("P3-02 9.1 test 6: data that can never make a valid root is fatal with the message of 4.3") {
    const Naming never = shipped_naming("never");
    for (const Kind k : names_test::KINDS) {
        Registry r;
        const std::string expect = "names: culture 'never' produced no valid " + std::string(names_test::kind_name(k)) +
                                   " name in 64 attempts";
        CHECK_THROWS_WITH_AS((void)names_api::make_name(never, "never", k, 0, 1, 1, r), expect.c_str(),
                             worldgen::FatalError);
    }
    // The golden fatal lines carry the same messages.
    const auto& lines = golden_lines("fatal");
    REQUIRE(lines.size() == 3);
    for (const std::string& line : lines) {
        const auto t = split(line, 7);
        Registry r;
        CHECK_THROWS_WITH_AS((void)names_api::make_name(never, "never", kind_of(t[2]), dec_u(t[3]), dec_u(t[4]),
                                                        dec_u(t[5]), r),
                             t[6].c_str(), worldgen::FatalError);
    }
    // The culture id in the message is the caller's.
    Registry r;
    CHECK_THROWS_WITH_AS((void)names_api::make_name(never, "elven", Kind::god, 0, 1, 1, r),
                         "names: culture 'elven' produced no valid god name in 64 attempts", worldgen::FatalError);
}

TEST_CASE("P3-02 4.3: a culture whose only valid names are taken falls back to numerals, never fatal") {
    const Naming tiny = shipped_naming("tiny");
    Registry r;
    std::set<std::string> got;
    for (std::uint64_t e = 1; e <= 40; ++e) {
        const std::string n = names_api::make_name(tiny, "tiny", Kind::settlement, 0, 1, e, r);
        CHECK(got.insert(n).second);
        CHECK(r.contains(n));
    }
    // Two plain names; every other name is "Ba <numeral>" or "Da <numeral>",
    // and each base's numerals run II, III, ... without a gap (4.3: the
    // smallest free k >= 2).
    CHECK(got.count("Ba") == 1);
    CHECK(got.count("Da") == 1);
    for (const std::string base : {"Ba", "Da"}) {
        std::uint64_t count = 0;
        for (const std::string& n : got) count += n.rfind(base, 0) == 0 ? 1U : 0U;
        for (std::uint64_t k = 2; k <= count; ++k) CHECK(got.count(base + " " + names_test::roman_ref(k)) == 1);
    }
    for (const std::string& n : got) {
        bool numbered = false;
        CHECK(names_test::shape_ok(tiny, Kind::settlement, n, &numbered));
        CHECK(numbered == (n.size() > 2));
    }
}

// --- Roman numerals (P1-06 RG11 4) ----------------------------------------------------------------

TEST_CASE("P3-02 4.3: roman(k) is P1-06 RG11's numeral") {
    const auto& lines = golden_lines("roman");
    REQUIRE(lines.size() >= 10);
    for (const std::string& line : lines) {
        const auto t = split(line, 3);
        CAPTURE(line);
        if (t[2] == "FATAL") {
            CHECK_THROWS_WITH_AS((void)names_api::roman(dec_u(t[1])), "roman: k outside [1, 100000]", worldgen::FatalError);
        } else {
            CHECK(names_api::roman(dec_u(t[1])) == t[2]);
        }
    }
    CHECK(names_api::roman(100000) == std::string(100, 'M'));
    CHECK_THROWS_AS((void)names_api::roman(0), worldgen::FatalError);
    CHECK_THROWS_AS((void)names_api::roman(100001), worldgen::FatalError);
    for (std::uint64_t k = 1; k <= 4000; ++k) {
        REQUIRE(names_api::roman(k) == names_test::roman_ref(k));
    }
}

// --- Golden registry scenarios ---------------------------------------------------------------------

TEST_CASE("P3-02 4.3, 4.4: the registry scenarios of the golden file (retries, numerals, pre-loaded names)") {
    // One registry per scenario: its preload lines first, then its result
    // lines in naming order. Each result is checked twice: by make_name, and
    // by 4.3's rule rebuilt from name_attempt on the registry before the call.
    int scenarios = 0;
    int results = 0;
    int fallbacks = 0;
    int retries = 0;
    Registry reg;
    Registry reg2;  // the same scenario through choose_name
    std::set<std::string> mirror;
    std::string label;
    std::uint64_t seed = 0;
    for (const std::string& line : names_test::golden().lines) {
        const auto head = split(line, 2);
        if (head[0] == "scenario") {
            const auto t = split(line, 4);
            label = t[1];
            seed = dec_u(t[2]);
            reg = Registry();
            reg2 = Registry();
            mirror.clear();
            ++scenarios;
        } else if (head[0] == "preload") {
            const auto t = split(line, 3);
            REQUIRE(t[1] == label);
            reg.add(t[2]);
            reg2.add(t[2]);
            mirror.insert(t[2]);
        } else if (head[0] == "result") {
            const auto t = split(line, 8);
            CAPTURE(line);
            REQUIRE(t[1] == label);
            const Naming n = shipped_naming(t[2]);
            const Kind k = kind_of(t[3]);
            const std::uint64_t c = dec_u(t[4]);
            const std::uint64_t e = dec_u(t[5]);
            // r5: the shipped filter, except culture filt with the test filter (11.7 item 8).
            const names_api::Filter f = t[2] == "filt" ? names_test::test_filter() : names_test::shipped_filter();
            check_attempt_rule(n, k, seed, c, e, copy_of(mirror), t[6], t[7], f);
            CHECK_FALSE(reg.contains(t[7]));
            CHECK(names_api::make_name(n, t[2], k, seed, c, e, reg, f) == t[7]);
            CHECK(reg.contains(t[7]));
            // choose_name: same name, the attempt token, and for a fallback the first valid attempt.
            const names_api::Choice ch = names_api::choose_name(n, t[2], k, seed, c, e, reg2, f);
            CHECK(ch.name == t[7]);
            CHECK(reg2.contains(t[7]));
            CHECK(ch.fallback == (t[6] == "F"));
            if (t[6] != "F") {
                CHECK(ch.attempt == dec_u(t[6]));
            } else {
                std::uint64_t first = 64;
                for (std::uint64_t a = 0; a < 64 && first == 64; ++a) {
                    if (names_api::attempt(n, k, seed, c, e, a, f).valid) first = a;
                }
                CHECK(ch.attempt == first);
            }
            mirror.insert(t[7]);
            ++results;
            fallbacks += t[6] == "F" ? 1 : 0;
            retries += (t[6] != "F" && t[6] != "0") ? 1 : 0;
        }
    }
    CHECK(scenarios == 11);  // the 8 of r4 and filtered, redraw_alone, redraw_after (r5, 11.7 item 8)
    CHECK(results == 37);
    CHECK(fallbacks >= 8);
    CHECK(retries >= 4);
}

// --- 9.3 item 10: the golden name lists -------------------------------------------------------------

TEST_CASE("P3-02 9.3 test 10: seeds 0 and 42, every shipped culture and kind, entities 1-50, one registry per seed") {
    const auto& expected = golden_lines("name");
    REQUIRE(expected.size() == 2 * 6 * 3 * 50);
    std::size_t next = 0;
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        Registry reg;
        Registry before;
        std::string joined;
        for (const std::string_view cid : names_test::CULTURE_IDS) {
            const Naming n = shipped_naming(cid);
            const names_api::Filter& f = names_test::shipped_filter();
            for (const Kind k : names_test::KINDS) {
                for (std::uint64_t e = 1; e <= 50; ++e) {
                    REQUIRE(next < expected.size());
                    const std::string& line = expected[next++];
                    CAPTURE(line);
                    const auto t = split(line, 6);
                    REQUIRE(dec_u(t[0]) == seed);
                    REQUIRE(t[1] == cid);
                    REQUIRE(kind_of(t[2]) == k);
                    REQUIRE(dec_u(t[3]) == e);
                    // choose_name gives the name and the attempt token of 9.3 directly.
                    const names_api::Choice ch =
                        names_api::choose_name(n, cid, k, seed, culture_number(cid), e, reg, f);
                    const std::string& got = ch.name;
                    CHECK(got == t[5]);
                    CHECK(ch.fallback == (t[4] == "F"));
                    if (t[4] != "F") CHECK(ch.attempt == dec_u(t[4]));
                    // The attempt token: checked against a second registry that
                    // holds exactly the names before this one.
                    check_attempt_rule(n, k, seed, culture_number(cid), e, before, t[4], t[5], f);
                    before.add(got);
                    const std::string mine = std::to_string(seed) + " " + std::string(cid) + " " +
                                             std::string(names_test::kind_name(k)) + " " + std::to_string(e) + " " +
                                             t[4] + " " + got;
                    if (!joined.empty()) joined += "\n";
                    joined += mine;
                }
            }
        }
        // The SHA-256 of the seed's lines joined by LF.
        bool found = false;
        for (const std::string& line : golden_lines("sha256")) {
            const auto t = split(line, 3);
            if (dec_u(t[1]) == seed) {
                CHECK(worldgen::sha256_hex(joined) == t[2]);
                found = true;
            }
        }
        CHECK(found);
    }
    // 4.6's tuple, at its place in the seed-0 list.
    bool iensath = false;
    for (const std::string& line : expected) iensath = iensath || line == "0 elven settlement 1 0 Iensath";
    CHECK(iensath);
}

// --- 5.1 sample names --------------------------------------------------------------------------------

TEST_CASE("P3-02 11.6 (r5): the sample table under r5 (seed 0, one registry per culture: settlements 1-20, civilizations 1-5, gods 1-3)") {
    // The 5.1 protocol with r5's rules; the table of 11.6 "Sample names of 5.1 under r5".
    struct Sample {
        std::string_view culture;
        std::vector<std::string> names;
    };
    const std::vector<Sample> table = {
        {"dwarven", {"Moumka", "Thoumdaili", "Makmoudin Deep", "Brummthu", "Gromka", "Kaird Deep", "Khaili Hold", "Kili",
                     "Thoum Hold", "Nodin", "Gondhark", "Turnbruk Deep", "Kidinkorn", "Tharndramm Hold", "Grangrak",
                     "Houmm Deep", "Doknirk", "Gali", "Nailimim Deep", "Tairdbra Hold", "The Kourdbrar Throne",
                     // P3-02 r17 15.2 (0.21.0): `didin` is forbidden; the 9.2 table's "The Hold of Didin" is
                     // replaced by the next attempt (the reference's make_name; spec table to be corrected)
                     "The Khandthra Throne", "Clan Durgarin", "Clan Drourd", "Clan Drailimai", "Brommgrai", "Groumtoun",
                     "Nurnthaimm"}},
        {"elven", {"Iensath", "Onaeme", "Ierrael", "Elonoma", "Fievos", "Mielae", "Asvathdell", "Lafin", "Laevo",
                   "Leithiedell", "Orith", "Voglael", "Mievife", "Iefann", "Cellve", "Fimo", "Londcae", "Rienel",
                   "Nathol", "Siendnie", "The House of Nife", "The Court of Nothind", "The House of Aesoth", "The House of Riefifie",
                   "The Court of Thiesaell", "Saene", "Fiendse", "Ithonnfa"}},
        // r15 (P3-02 14.1, 14.4, generator 0.20.0): the faerie row of 5.1's protocol with 14.1's lists (the
        // reference's SAMPLES_R15; 11.6's row is the record of the 0.17.0 to 0.19.0 lists)
        {"faerie", {"Liava", "Wiatwiesk Glade", "Twamae Glade", "Lith", "Gliafloss Glade", "Viashaeth Hollow", "Teelan",
                    "Meeie Hollow", "Sywyn Hollow", "Iatysk", "Wievysk", "Miamo Hollow", "Mavofie", "Fitoth Hollow",
                    "Showhia Hollow", "Mawhiath Hollow", "Ashfli Hollow", "Whywo", "Fliavosk Hollow", "Whosy Hollow",
                    "The Lysk Ring", "The Watiafia Court", "The Gloss Ring", "The Wosk Ring", "The Twiwhiwae Ring",
                    "Fiwhall", "Fiaglo", "Gliallsho"}},
        {"goblin", {"Krot Pit", "Slograb Pit", "Gnink-Snunk", "Zux", "Grunkgnuzz Pit", "Grizz-Skeeb", "Slank-Skrox",
                    "Drobskeet", "Bozz Pit", "Sluggatch Hole", "Bux-Kreeb", "Sniz", "Slak", "Zug-Gok", "Gunitch",
                    "Skrizuz", "Droggkazz Hole", "Slobeerk", "Snok", "Kruskreezz Hole", "The Gnazz Horde",
                    "The Baskeeb Mob", "The Grodratch Horde", "The Krunkgirk Horde", "The Warrens of Gnonk",
                    "Zab-Skreeb", "Kruskux", "Nugnug"}},
        {"human", {"Deanham", "Alantton", "Cickham", "Gackwick", "Brir", "Tackbury", "Gurdton", "Studford", "Ceambury",
                   "Buld", "Silstudstead", "Tirdwick", "Dack", "Stastint", "Moldstead", "Edumford", "Tontstead",
                   "Ceanbir", "Durdton", "Durdbury", "The Duchy of Ashem", "The Kingdom of Ashos", "The Kingdom of Edes",
                   "The Duchy of Wick", "The Duchy of Circon", "Wasbrid", "Histold", "Edostold"}},
        {"orcish", {"Kaak Fort", "Zaarz Camp", "Grush", "Zaakugaag", "Uruld'duuld", "Shaazazuug", "Kark Fort",
                    "Azol'urash Fort", "Karzthogh", "Uromgha", "Maag", "Uul'krakh Camp", "Og'buurk Camp",
                    "Zaalduruz Camp", "Mak", "Duld'sha Fort", "Shuum Fort", "Ghorz Fort", "Duuldkrosh Fort",
                    "Aag'urog Fort", "Clan Kram'krorz", "The Azakhdam Horde", "The Urogh'krorz Horde",
                    "The Burk'zork Horde", "The Warband of Maakurokh", "Ug'krarz", "Og'uraag", "Uruuk'krush"}},
    };
    for (const Sample& s : table) {
        CAPTURE(s.culture);
        REQUIRE(s.names.size() == 28);
        const Naming n = shipped_naming(s.culture);
        const std::uint64_t c = culture_number(s.culture);
        const names_api::Filter& f = names_test::shipped_filter();
        Registry reg;
        std::size_t i = 0;
        for (std::uint64_t e = 1; e <= 20; ++e) CHECK(names_api::make_name(n, s.culture, Kind::settlement, 0, c, e, reg, f) == s.names[i++]);
        for (std::uint64_t e = 1; e <= 5; ++e) CHECK(names_api::make_name(n, s.culture, Kind::civilization, 0, c, e, reg, f) == s.names[i++]);
        for (std::uint64_t e = 1; e <= 3; ++e) CHECK(names_api::make_name(n, s.culture, Kind::god, 0, c, e, reg, f) == s.names[i++]);
    }
}

// --- Revised spec (2026-11-19): registry, seams, malformed data ----------------------------------------

TEST_CASE("P3-02 4.3 (revised): adding a name the registry already holds changes nothing") {
    Registry r;
    CHECK(r.size() == 0);
    r.add("Iensath");
    r.add("Iensath");
    CHECK(r.size() == 1);
    CHECK(r.contains("Iensath"));
    r.add("iensath");
    CHECK(r.size() == 2);
    // make_name adds exactly one name; a fallback adds the numbered name only.
    const Naming tiny = shipped_naming("tiny");
    r.add("Ba");
    r.add("Da");
    const std::size_t before = r.size();
    const std::string n = names_api::make_name(tiny, "tiny", Kind::god, 0, 1, 1, r);
    CHECK(r.size() == before + 1);
    CHECK(r.contains(n));
    CHECK(n.size() == 5);  // "Ba II" or "Da II"
}

TEST_CASE("P3-02 9 (revised): the kind seams, the constants and capitalize_root") {
    CHECK(names_api::kind_text(Kind::god) == "god");
    CHECK(names_api::kind_text(Kind::civilization) == "civilization");
    CHECK(names_api::kind_text(Kind::settlement) == "settlement");
    for (const Kind k : names_test::KINDS) CHECK(names_api::kind_stream(k) == names_test::stream_of(k));
    const Naming elven = shipped_naming("elven");
    for (const Kind k : names_test::KINDS) CHECK(names_api::kind_forms(elven, k) == names_test::forms_of(elven, k));
    CHECK(names_api::attempt_max_bytes() == 48);
    CHECK(names_api::name_max_bytes() == 64);
    CHECK(names_api::name_attempts() == 64);
    for (const std::string& line : golden_lines("cap")) {
        const auto t = split(line, 3);
        CHECK(names_api::capitalize_root(t[1]) == t[2]);
    }
    // NameAttempt::root is the root before Cap (4.6: iensath).
    CHECK(names_api::attempt(elven, Kind::settlement, 0, 2, 1, 0).root == "iensath");
}

TEST_CASE("P3-02 9 (revised): make_name equals choose_name(...).name, registry for registry") {
    for (const std::string_view cid : {std::string_view("human"), std::string_view("tiny")}) {
        const Naming n = shipped_naming(cid);
        Registry a;
        Registry b;
        for (std::uint64_t e = 1; e <= 60; ++e) {
            for (const Kind k : names_test::KINDS) {
                const std::string m = names_api::make_name(n, cid, k, 9, culture_number(cid), e, a);
                const names_api::Choice c = names_api::choose_name(n, cid, k, 9, culture_number(cid), e, b);
                CHECK(m == c.name);
            }
        }
        CHECK(a.size() == b.size());
    }
}

TEST_CASE("P3-02 4.2 (revised): malformed naming data is fatal in the library, with its message") {
    Naming n = shipped_naming("tiny");
    n.god_forms = {"Lord"};
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::god, 0, 1, 1, 0), "names: a name form must hold exactly one {root}",
                         worldgen::FatalError);
    n.god_forms = {"{root} {root}"};
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::god, 0, 1, 1, 0), "names: a name form must hold exactly one {root}",
                         worldgen::FatalError);
    n = shipped_naming("tiny");
    n.patterns = {"OX"};
    n.god_patterns.reset();  // r5: the god draws from god_patterns; unset, it is patterns
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::god, 0, 1, 1, 0),
                         "names: a name pattern holds a symbol other than O, N, C, ' and -", worldgen::FatalError);
    n = shipped_naming("tiny");
    n.onsets.clear();
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::god, 0, 1, 1, 0), "names: a naming list is empty",
                         worldgen::FatalError);
    n = shipped_naming("tiny");
    n.settlement_forms.clear();
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::settlement, 0, 1, 1, 0), "names: a naming list is empty",
                         worldgen::FatalError);
    n = shipped_naming("tiny");
    n.patterns.clear();
    n.god_patterns.reset();
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::god, 0, 1, 1, 0), "names: a naming list is empty",
                         worldgen::FatalError);
    // make_name passes it on.
    Registry r;
    CHECK_THROWS_WITH_AS((void)names_api::make_name(n, "tiny", Kind::god, 0, 1, 1, r), "names: a naming list is empty",
                         worldgen::FatalError);
    // A list is checked only when picked from: no C in the pattern, an empty codas list is fine.
    n = shipped_naming("tiny");
    n.codas.clear();
    const names_api::Attempt a = names_api::attempt(n, Kind::god, 0, 1, 1, 0);
    CHECK(a.valid);
    CHECK((a.name == "Ba" || a.name == "Da"));
}


// --- r5 (P3-02 11, generator 0.17.0): the name filter, god_patterns, realm forms ----------------------

TEST_CASE("P3-02 11.7 item 5: root_word is the root with the form's letters fused to it, lower-cased") {
    CHECK(names_api::root_word("The Hold of {root}", "dur") == "dur");
    CHECK(names_api::root_word("{root}ford", "ced") == "cedford");
    CHECK(names_api::root_word("Ny{root} Pit", "ash") == "nyash");
    CHECK(names_api::root_word("{root}", "ug'kaal") == "ug'kaal");
    for (const std::string& line : golden_lines("rootword")) {
        CAPTURE(line);
        const auto t = split(line, 4);
        std::string form = t[1];
        for (char& c : form) {
            if (c == '_') c = ' ';
        }
        CHECK(names_api::root_word(form, t[2]) == t[3]);
        CHECK(names_test::ref_root_word(form, t[2]) == t[3]);
    }
}

TEST_CASE("P3-02 11.7 item 6: name_filtered on a hand-made filter (anywhere ab, at_start cd, whole ef)") {
    const names_api::Filter f{{"ab"}, {"cd"}, {"ef"}};
    struct Case {
        const char* w;
        bool filtered;
    };
    for (const Case& c : std::vector<Case>{{"xaby", true}, {"a-by", true}, {"xcdy", false}, {"xx-cdy", true},
                                           {"cd", true},   {"ef", true},   {"efx", false},  {"x'ef", true}}) {
        CAPTURE(c.w);
        CHECK(names_api::name_filtered(f, c.w) == c.filtered);
        CHECK(names_test::ref_filtered(f, c.w) == c.filtered);
    }
    // "XEF (after lower-casing in root_word)": root_word lower-cases the form's fused capitals.
    // ASSUMPTION: as written, W of form "X{root}F" with root "e" is "xef", which none of the three
    // entries matches; the lower-casing is shown by form "E{root}", root "f": W "ef", whole "ef".
    CHECK(names_api::root_word("X{root}F", "e") == "xef");
    CHECK_FALSE(names_api::name_filtered(f, names_api::root_word("X{root}F", "e")));
    CHECK(names_api::name_filtered(f, names_api::root_word("E{root}", "f")));
    // The empty filter rejects nothing.
    for (const char* w : {"xaby", "cd", "ef", "x'ef"}) CHECK_FALSE(names_api::name_filtered(names_api::Filter{}, w));
}

TEST_CASE("P3-02 11.3, 11.7 item 10: the critics' examples are rejected as root words of their forms (stems as concatenations)") {
    const names_api::Filter& f = names_test::shipped_filter();
    // Root words W of 11.3, each written as a concatenation of its parts.
    const std::vector<std::string> rejected = {
        std::string("co") + "ck" + "ford", std::string("co") + "ck" + "ton", std::string("ca") + "ck" + "ford",
        std::string("bu") + "m" + "dou",   std::string("but") + "-" + "skrotch", std::string("sn") + "ot" + "gab",
        "bag", "dog", std::string("do") + "rk", "baa", "bald", "buzz", "gord", "vanir",
        std::string("n") + "igg" + "snib", std::string("sn") + "ig", std::string("drin") + "igg",
        // r6: P3-02 11.7 item 10's additions (the masked letters restored only here, as concatenations).
        std::string("tu") + "rd" + "stead", std::string("krob-") + "sn" + "igg", std::string("guz-") + "gn" + "igg",
        std::string("nik-") + "gn" + "igg", std::string("gn") + "ig" + "sneet", std::string("kreets") + "l" + "ut",
        std::string("s") + "l" + "ut" + "keeb", std::string("gn") + "azi" + "tch",
        // 11.3's examples
        std::string("ugh") + "'kaal", std::string("cu") + "m" + "brin",
        // P3-02 r13 13.1: the fragment of 11.3's example now matches anywhere, so its "not filtered" example
        // (Ascumbrin) is filtered from 0.19.0 r13 on (11.3's sentence is outdated by 13.1)
        std::string("ascu") + "m" + "brin",
        // P3-02 r15 14.3 (0.20.0): tard is an anywhere entry, so 11.3's passing example Stard is filtered
        std::string("s") + "tard"};
    for (std::size_t i = 0; i < rejected.size(); ++i) {
        CAPTURE(i);  // the index, not the word
        CHECK(names_api::name_filtered(f, rejected[i]));
        CHECK(names_test::ref_filtered(f, rejected[i]));
    }
    for (const std::string& w : std::vector<std::string>{"iensath", "grork"}) {
        CAPTURE(w);
        CHECK_FALSE(names_api::name_filtered(f, w));
        CHECK_FALSE(names_test::ref_filtered(f, w));
    }
    // r6: the per-word guard: The Realm of Arthos is not filtered (its W is "arthos").
    CHECK(names_api::root_word("The Realm of {root}", "arthos") == "arthos");
    CHECK_FALSE(names_api::name_filtered(f, names_api::root_word("The Realm of {root}", "arthos")));
    // The golden filter lines (benign words only).
    for (const std::string& line : golden_lines("filter")) {
        CAPTURE(line);
        const auto t = split(line, 3);
        CHECK(names_api::name_filtered(f, t[1]) == (t[2] == "1"));
        CHECK(names_test::ref_filtered(f, t[1]) == (t[2] == "1"));
    }
}

TEST_CASE("P3-02 11.7 item 8: an attempt rejected only by the filter has valid false and filtered true (golden fattempt lines)") {
    const auto& lines = golden_lines("fattempt");
    REQUIRE(lines.size() >= 4);
    int filtered = 0;
    for (const std::string& line : lines) {
        CAPTURE(line);
        const auto t = split(line, 10);
        const Naming n = shipped_naming(t[2]);
        const names_api::Filter f = t[2] == "filt" ? names_test::test_filter() : names_test::shipped_filter();
        const names_api::Attempt a =
            names_api::attempt(n, kind_of(t[3]), dec_u(t[1]), dec_u(t[4]), dec_u(t[5]), dec_u(t[6]), f);
        CHECK(a.valid == (t[7] == "1"));
        CHECK(a.filtered == (t[8] == "1"));
        if (a.valid) CHECK(a.name == t[9]);
        if (a.filtered) {
            ++filtered;
            // The same attempt without the filter is valid (the r4 rules pass).
            CHECK(names_api::attempt(n, kind_of(t[3]), dec_u(t[1]), dec_u(t[4]), dec_u(t[5]), dec_u(t[6])).valid);
        }
    }
    CHECK(filtered >= 4);
}

TEST_CASE("P3-02 11.7 item 8 (r6): a redraw depends on (kind, attempt, culture, entity) only") {
    // Culture filt with the test filter: the first entity whose attempts 0 and 1 are filtered and
    // whose attempt 2 is valid (the reference's search; scenario redraw_alone gives it).
    const Naming filt = shipped_naming("filt");
    const names_api::Filter tf = names_test::test_filter();
    std::uint64_t e2 = 0;
    for (std::uint64_t e = 1; e < 100000 && e2 == 0; ++e) {
        if (names_api::attempt(filt, Kind::settlement, 0, 1, e, 0, tf).filtered &&
            names_api::attempt(filt, Kind::settlement, 0, 1, e, 1, tf).filtered &&
            names_api::attempt(filt, Kind::settlement, 0, 1, e, 2, tf).valid) {
            e2 = e;
        }
    }
    REQUIRE(e2 != 0);
    const std::string alone = [&] {
        Registry r;
        return names_api::make_name(filt, "filt", Kind::settlement, 0, 1, e2, r, tf);
    }();
    CHECK(alone == names_api::attempt(filt, Kind::settlement, 0, 1, e2, 2, tf).name);
    // Others named first, with their own filtered attempts: the same name.
    Registry r;
    const names_api::Filter& sf = names_test::shipped_filter();
    for (std::uint64_t e = 1; e <= 5; ++e) {
        (void)names_api::make_name(shipped_naming("human"), "human", Kind::settlement, 0, 5, e, r, sf);
        (void)names_api::make_name(shipped_naming("human"), "human", Kind::civilization, 0, 5, e, r, sf);
    }
    CHECK(names_api::make_name(filt, "filt", Kind::settlement, 0, 1, e2, r, tf) == alone);
    // The fallback numbers the first unfiltered valid name (scenario "filtered" checks the golden case).
    std::uint64_t e1 = 0;
    for (std::uint64_t e = 1; e < 100000 && e1 == 0; ++e) {
        if (names_api::attempt(filt, Kind::settlement, 0, 1, e, 0, tf).filtered) e1 = e;
    }
    REQUIRE(e1 != 0);
    Registry full;
    for (const char* o : {"B", "D", "G", "K", "M"}) {
        for (const char* nu : {"a", "o"}) {
            for (const char* co : {"", "n"}) {
                const std::string nm = std::string(o) + nu + co;
                if (!names_test::ref_filtered(tf, names_test::lower(nm))) full.add(nm);
            }
        }
    }
    CHECK(full.size() == 18);
    std::string first;
    for (std::uint64_t t = 0; t < 64 && first.empty(); ++t) {
        const names_api::Attempt a = names_api::attempt(filt, Kind::settlement, 0, 1, e1, t, tf);
        if (a.valid) first = a.name;
    }
    REQUIRE_FALSE(first.empty());
    const names_api::Choice ch = names_api::choose_name(filt, "filt", Kind::settlement, 0, 1, e1, full, tf);
    CHECK(ch.fallback);
    CHECK(ch.name == first + " II");
}

TEST_CASE("P3-02 11.7 item 7: with the empty filter and god_patterns = patterns every r4 name is reproduced") {
    // The r4sha256 lines hold the 0.16.0 lists' SHA-256 (seed 0 and 42).
    const auto& lines = golden_lines("r4sha256");
    REQUIRE(lines.size() == 2);
    for (const std::string& line : lines) {
        CAPTURE(line);
        const auto t = split(line, 3);
        const std::uint64_t seed = dec_u(t[1]);
        Registry reg;
        std::string joined;
        for (const std::string_view cid : names_test::CULTURE_IDS) {
            const Naming n = names_test::r4_equivalent(cid);
            for (const Kind k : names_test::KINDS) {
                for (std::uint64_t e = 1; e <= 50; ++e) {
                    const names_api::Choice ch = names_api::choose_name(n, cid, k, seed, culture_number(cid), e, reg);
                    if (!joined.empty()) joined += "\n";
                    joined += std::to_string(seed) + " " + std::string(cid) + " " +
                              std::string(names_test::kind_name(k)) + " " + std::to_string(e) + " " +
                              (ch.fallback ? std::string("F") : std::to_string(ch.attempt)) + " " + ch.name;
                }
            }
        }
        CHECK(worldgen::sha256_hex(joined) == t[2]);
    }
}

TEST_CASE("P3-02 11.4, 11.6: gods draw their shape from god_patterns; the elven realms have a realm form") {
    // The 11.6 god table (race order elf, goblin, dwarf, faerie, human, orc; god entity = race order).
    const std::vector<std::pair<std::uint64_t, std::vector<std::string>>> gods = {
        {0, {"Saene", "Kruskux", "Nurnthaimm", "Whaia", "Steldhil", "Uruld'kruum"}},
        {7, {"Onveth", "Buzork", "Mammga", "Ouwheefy", "Balbron", "Maarkzaagh"}},  // r10 12.1: Gordman redrawn (0.19.0)
        {42, {"Fielie", "Krokzit", "Bronhak", "Syfiash", "Wealstuld", "Ogh'krak"}}};
    const std::vector<std::string_view> by_race = {"elven", "goblin", "dwarven", "faerie", "human", "orcish"};
    const names_api::Filter& f = names_test::shipped_filter();
    for (const auto& [seed, want] : gods) {
        CAPTURE(seed);
        for (std::size_t i = 0; i < by_race.size(); ++i) {
            Registry r;
            CHECK(names_api::make_name(shipped_naming(by_race[i]), by_race[i], Kind::god, seed, culture_number(by_race[i]),
                                       i + 1, r, f) == want[i]);
        }
    }
    // The 11.6 realm table, seeds 0 and 3 (civilizations 1 and 2, empty registry).
    for (const auto& [seed, want] : std::vector<std::pair<std::uint64_t, std::pair<std::string, std::string>>>{
             {0, {"The House of Nife", "The Court of Nothind"}}, {3, {"The House of Roelaele", "The House of Vefal"}}}) {
        Registry r1;
        Registry r2;
        CHECK(names_api::make_name(shipped_naming("elven"), "elven", Kind::civilization, seed, 2, 1, r1, f) == want.first);
        CHECK(names_api::make_name(shipped_naming("elven"), "elven", Kind::civilization, seed, 2, 2, r2, f) == want.second);
    }
    // A god drawn from god_patterns, not patterns: a tiny culture whose patterns make one-letter
    // roots and whose god_patterns make two-letter ones.
    Naming n = shipped_naming("tiny");
    n.patterns = {"N"};
    n.min_letters = 1;
    n.god_patterns = std::vector<std::string>{"ON"};
    const names_api::Attempt g = names_api::attempt(n, Kind::god, 0, 1, 1, 0);
    CHECK(g.valid);
    CHECK(g.root.size() == 2);
    CHECK(names_api::attempt(n, Kind::settlement, 0, 1, 1, 0).root == "a");
    // An empty god_patterns is fatal like the other lists (11.4).
    n.god_patterns = std::vector<std::string>{};
    CHECK_THROWS_WITH_AS((void)names_api::attempt(n, Kind::god, 0, 1, 1, 0), "names: a naming list is empty",
                         worldgen::FatalError);
}

TEST_CASE("P3-02 11.2, 11.5: the library's effective filter equals the union of the shipped files") {
    // The test reads game/data/name_filters/ with toml++; the library builds its filter from the
    // loaded content set (name_filter_from). Compared as sets (only membership matters, 11.2).
    const names_api::Filter& mine = names_test::shipped_filter();
    // P3-02 r17 15.1 (0.21.0): 109 / 30 / 514 (`fag` from at_start to anywhere, `fagg` removed from anywhere;
    // `shiva`, `hindi`, `fannie` whole). P3-02 r15 14.3 (0.20.0): 109 / 31 / 511 (r13 13.1: 110 / 31 / 508; r10 12.1: 108 / 36 / 510; 101 / 35 / 514
    // at 0.18.0; 768 whole at 0.17.0)
    CHECK(std::set<std::string>(mine.anywhere.begin(), mine.anywhere.end()).size() == 109);
    CHECK(std::set<std::string>(mine.at_start.begin(), mine.at_start.end()).size() == 30);
    CHECK(std::set<std::string>(mine.whole.begin(), mine.whole.end()).size() == 514);
}

}  // TEST_SUITE
