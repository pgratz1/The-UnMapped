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
                        const Registry& before, const std::string& tok, const std::string& name) {
    CAPTURE(tok);
    CAPTURE(name);
    std::string first;
    bool have_first = false;
    for (std::uint64_t t = 0; t < 64; ++t) {
        const names_api::Attempt a = names_api::attempt(n, k, seed, c, e, t);
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
        const names_api::Attempt a = names_api::attempt(n, k, dec_u(t[1]), dec_u(t[4]), dec_u(t[5]), dec_u(t[6]));
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
        CHECK(valid.size() == 61);
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

TEST_CASE("P3-02 4.6: the seed-42 vectors in one registry, in order") {
    Registry r;
    CHECK(names_api::make_name(shipped_naming("dwarven"), "dwarven", Kind::god, 42, 1, 1, r) == "Khork");
    CHECK_FALSE(names_api::attempt(shipped_naming("goblin"), Kind::civilization, 42, 4, 3, 0).valid);
    CHECK(names_api::make_name(shipped_naming("goblin"), "goblin", Kind::civilization, 42, 4, 3, r) == "The Grork Horde");
    CHECK(names_api::make_name(shipped_naming("human"), "human", Kind::settlement, 42, 5, 17, r) == "Deantwick");
    CHECK(names_api::make_name(shipped_naming("orcish"), "orcish", Kind::settlement, 42, 6, 200, r) == "Ol'uraakh Fort");
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
    const std::string tail(42, 'x');  // a 48-byte form "{root}xxx...x"
    Naming n = fixed("", "aaaaaa", "", "N", 1, 32);
    n.god_forms = {"{root}" + tail};
    REQUIRE(n.god_forms[0].size() == 48);
    names_api::Attempt a = names_api::attempt(n, Kind::god, 0, 1, 1, 0);
    CHECK(a.valid);  // 6 + 42 = 48 bytes
    CHECK(a.name.size() == 48);
    n.nuclei = {"aaaaaaa"};
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
            check_attempt_rule(n, k, seed, c, e, copy_of(mirror), t[6], t[7]);
            CHECK_FALSE(reg.contains(t[7]));
            CHECK(names_api::make_name(n, t[2], k, seed, c, e, reg) == t[7]);
            CHECK(reg.contains(t[7]));
            // choose_name: same name, the attempt token, and for a fallback the first valid attempt.
            const names_api::Choice ch = names_api::choose_name(n, t[2], k, seed, c, e, reg2);
            CHECK(ch.name == t[7]);
            CHECK(reg2.contains(t[7]));
            CHECK(ch.fallback == (t[6] == "F"));
            if (t[6] != "F") {
                CHECK(ch.attempt == dec_u(t[6]));
            } else {
                std::uint64_t first = 64;
                for (std::uint64_t a = 0; a < 64 && first == 64; ++a) {
                    if (names_api::attempt(n, k, seed, c, e, a).valid) first = a;
                }
                CHECK(ch.attempt == first);
            }
            mirror.insert(t[7]);
            ++results;
            fallbacks += t[6] == "F" ? 1 : 0;
            retries += (t[6] != "F" && t[6] != "0") ? 1 : 0;
        }
    }
    CHECK(scenarios == 8);
    CHECK(results == 24);
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
                    const names_api::Choice ch = names_api::choose_name(n, cid, k, seed, culture_number(cid), e, reg);
                    const std::string& got = ch.name;
                    CHECK(got == t[5]);
                    CHECK(ch.fallback == (t[4] == "F"));
                    if (t[4] != "F") CHECK(ch.attempt == dec_u(t[4]));
                    // The attempt token: checked against a second registry that
                    // holds exactly the names before this one.
                    check_attempt_rule(n, k, seed, culture_number(cid), e, before, t[4], t[5]);
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

TEST_CASE("P3-02 5.1: the sample table (seed 0, one registry per culture: settlements 1-20, civilizations 1-5, gods 1-3)") {
    struct Sample {
        std::string_view culture;
        std::vector<std::string> names;
    };
    const std::vector<Sample> table = {
        {"dwarven", {"Moumka", "Thoumdaili", "Makmoudin Deep", "Brummthu", "Gromka", "Kaird Deep", "Khaili Hold", "Kili",
                     "Thoum Hold", "Nodin", "Gondhark", "Turnbruk Deep", "Kidinkorn", "Tharndramm Hold", "Grangrak",
                     "Houmm Deep", "Doknirk", "Gali", "Nailimim Deep", "Tairdbra Hold", "The Kourdbrar Throne",
                     "The Hold of Didin", "Clan Durgarin", "Clan Drourd", "Clan Drailimai", "Brommgrai", "Groumtoun",
                     "Nurnthaimm"}},
        {"elven", {"Iensath", "Onaeme", "Ierrael", "Elonoma", "Fievos", "Mielae", "Asvathdell", "Lafin", "Laevo",
                   "Leithiedell", "Orith", "Voglael", "Mievife", "Iefann", "Cellve", "Fimo", "Londcae", "Rienel",
                   "Nathol", "Siendnie", "Nife", "The Court of Nothind", "Aesoth", "Riefifie",
                   "The Court of Thiesaell", "Saene", "Fiendse", "Ithonnfa"}},
        {"faerie", {"Liapa", "Wiatwiesk Glade", "Twamae Glade", "Lip", "Biafloss Glade", "Pieshaep Hollow", "Teelan",
                    "Meeie Hollow", "Sywysh Hollow", "Iatysk", "Wiepysk", "Miemo Hollow", "Mapoufee", "Feetop Hollow",
                    "Showhie Hollow", "Mawhiap Hollow", "Athfli Hollow", "Whywo", "Fliaposk Hollow", "Whosy Hollow",
                    "The Lyp Ring", "The Watiafia Court", "The Bouss Ring", "The Wosk Ring", "The Twiwhiwou Ring",
                    "Fiwhall", "Fiabo", "Biensho"}},
        {"goblin", {"Krot Pit", "Slograb Pit", "Gnink-Snunk", "Zux", "Grunkgnuzz Pit", "Grizz-Skeeb", "Slank-Skrox",
                    "Drobskeet", "Bag Hole", "Sluggatch Hole", "Bux-Kreeb", "Sniz", "Slak", "Zug-Gok", "Gunitch",
                    "Slagreeg Hole", "Droggkazz Hole", "Slobeerk", "Snotgab", "Kruskreezz Hole", "The Gnazz Horde",
                    "The Warrens of Bonk-Gitch", "The Grodratch Horde", "The Krunkgirk Horde", "The Warrens of Gnonk",
                    "Zabskreeb", "Kruskux", "Bazz"}},
        {"human", {"Deanham", "Alantton", "Cickham", "Gackwick", "Brir", "Tackbury", "Gurdton", "Studford", "Ceambury",
                   "Buld", "Silstudstead", "Tirdwick", "Dack", "Stastint", "Moldstead", "Edumford", "Tontstead",
                   "Ceanbir", "Durdton", "Durdbury", "The Duchy of Ashem", "The Kingdom of Ashos", "The Duchy of Hick",
                   "The Duchy of Wick", "The Duchy of Circon", "Simbear", "Histold", "Edostold"}},
        {"orcish", {"Kaak Fort", "Zaarz Camp", "Grush", "Zaakugaag", "Uruld'duuld", "Uu'mold Fort", "Kark Fort",
                    "Azol'urash Fort", "Karzthogh", "Uromgha", "Maag", "Uul'krakh Camp", "Og'buurk Camp",
                    "Zaalduruz Camp", "Mak", "Duld'sha Fort", "Shuum Fort", "Ghorz Fort", "Duuldkrosh Fort",
                    "Aag'urog Fort", "Clan Kram'krorz", "The Azakhdam Horde", "The Urogh'krorz Horde",
                    "The Burk'zork Horde", "The Warband of Maakurokh", "Ugh'kaal", "Og'uraag", "Uruuk"}},
    };
    for (const Sample& s : table) {
        CAPTURE(s.culture);
        REQUIRE(s.names.size() == 28);
        const Naming n = shipped_naming(s.culture);
        const std::uint64_t c = culture_number(s.culture);
        Registry reg;
        std::size_t i = 0;
        for (std::uint64_t e = 1; e <= 20; ++e) CHECK(names_api::make_name(n, s.culture, Kind::settlement, 0, c, e, reg) == s.names[i++]);
        for (std::uint64_t e = 1; e <= 5; ++e) CHECK(names_api::make_name(n, s.culture, Kind::civilization, 0, c, e, reg) == s.names[i++]);
        for (std::uint64_t e = 1; e <= 3; ++e) CHECK(names_api::make_name(n, s.culture, Kind::god, 0, c, e, reg) == s.names[i++]);
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

}  // TEST_SUITE
