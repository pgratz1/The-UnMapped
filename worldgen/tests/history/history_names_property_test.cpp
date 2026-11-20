// Properties of names (spec P3-02 4.3, 4.4 and section 6; 9.2 items 8 and
// 9): 2,000 names per shipped culture and kind for seeds 0 and 1 (unique,
// invariant 1, the fallback share reported), uniqueness across kinds and
// cultures in one registry, region names pre-loaded, invariant 3 (purity and
// order independence), invariant 4 (empty registry: the first valid
// attempt), the 64-attempt bound, the numeral fallback, and the 64-byte hard
// limit. The library is reached only through p3_02_names_api.hpp.
//
// The reference (reference/p3_names_reference.py sweep 2000) measured 0
// fallbacks for every shipped culture, kind and seed 0 and 1; the largest
// attempt number used was 26 (human gods, seed 0).

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "worldgen/core/error.hpp"

using names_api::Kind;
using names_api::Naming;
using names_api::Registry;
using names_test::culture_number;
using names_test::kind_name;
using names_test::shipped_naming;

namespace {

// The first valid attempt's name, or "" if none of the 64 is valid.
std::string first_valid(const names_api::Namer& namer, Kind k, std::uint64_t seed, std::uint64_t c, std::uint64_t e) {
    for (std::uint64_t t = 0; t < 64; ++t) {
        names_api::Attempt a = namer.attempt(k, seed, c, e, t);
        if (a.valid) return a.name;
    }
    return {};
}

}  // namespace

TEST_SUITE("history-names-property") {

TEST_CASE("P3-02 9.2 test 8, 11.7 item 9: 2,000 names per shipped culture and kind, seeds 0 and 1: unique, invariants 1 and 5") {
    // r5 (generator 0.17.0): drawn with god_patterns and the shipped filter; no root word is
    // filtered (invariant 5, by the test's own code of 11.3), every god root parses as one of its
    // culture's god_patterns, no civilization name is a bare root. A filtered name is reported by
    // its entity number only (the text could be offensive).
    std::ostringstream report;  // the fallback share, reported once (9.2 test 8)
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{1}}) {
        for (const std::string_view cid : names_test::CULTURE_IDS) {
            const Naming n = shipped_naming(cid);
            const names_api::Namer namer(n, names_test::shipped_filter());
            for (const Kind k : names_test::KINDS) {
                CAPTURE(seed);
                CAPTURE(cid);
                CAPTURE(kind_name(k));
                Registry reg;
                std::set<std::string> seen;
                int numbered_count = 0;
                bool all_ok = true;
                for (std::uint64_t e = 1; e <= 2000; ++e) {
                    const std::string name = namer.make(cid, k, seed, culture_number(cid), e, reg);
                    bool numbered = false;
                    const bool ok = names_test::shape_ok(n, k, name, &numbered) && seen.insert(name).second &&
                                    reg.contains(name);
                    if (!ok) {
                        all_ok = false;
                        FAIL_CHECK("bad or repeated name for entity " << e << ": '" << name << "'");
                        break;
                    }
                    numbered_count += numbered ? 1 : 0;
                    if (names_test::name_root_filtered(n, k, name, names_test::shipped_filter())) {
                        FAIL_CHECK("invariant 5: the root word of entity " << e << " is filtered");
                        all_ok = false;
                        break;
                    }
                    if (k == Kind::god && !numbered && !names_test::god_root_parses(n, name)) {
                        FAIL_CHECK("11.7 item 9: god " << e << " '" << name << "' is not of the god_patterns");
                    }
                    if (k == Kind::civilization && name.find(' ') == std::string::npos) {
                        FAIL_CHECK("11.7 item 9: civilization " << e << " '" << name << "' is a bare root");
                    }
                }
                CHECK(all_ok);
                CHECK(seen.size() == 2000);
                report << " " << cid << "/" << kind_name(k) << "/" << seed << "=" << numbered_count;
                // The reference measured none for the shipped data.
                CHECK(numbered_count == 0);
            }
        }
    }
    MESSAGE("names needing the Roman fallback, of 2000 (culture/kind/seed=count):" << report.str());
}

TEST_CASE("P3-02 4.4, invariant 2: one registry across kinds and cultures never holds a name twice") {
    // Spec 10 question 3: names are unique across kinds. The tiny culture's
    // god, civilization and settlement draw from the same two roots, so
    // sharing the registry forces numerals across kinds.
    {
        const Naming tiny = shipped_naming("tiny");
        Registry reg;
        std::set<std::string> seen;
        for (std::uint64_t e = 1; e <= 10; ++e) {
            for (const Kind k : names_test::KINDS) {
                const std::string name = names_api::make_name(tiny, "tiny", k, 3, 1, e, reg);
                CHECK(seen.insert(name).second);
                CHECK(names_test::shape_ok(tiny, k, name));
            }
        }
        CHECK(seen.size() == 30);
    }
    {
        Registry reg;
        std::set<std::string> seen;
        for (const std::string_view cid : names_test::CULTURE_IDS) {
            const names_api::Namer namer(shipped_naming(cid));
            for (const Kind k : names_test::KINDS) {
                for (std::uint64_t e = 1; e <= 150; ++e) {
                    const std::string name = namer.make(cid, k, 7, culture_number(cid), e, reg);
                    CHECK_MESSAGE(seen.insert(name).second, name);
                }
            }
        }
        CHECK(seen.size() == 6 * 3 * 150);
    }
}

TEST_CASE("P3-02 4.4: region names pre-loaded in the registry are never generated") {
    // Spec 4.4: the caller fills the registry first with the region names of
    // table region, in id order. A pre-loaded name that a culture would make
    // first is skipped (seed 0 dwarven settlement 1 is Moumka by 5.1).
    const Naming dwarven = shipped_naming("dwarven");
    const std::vector<std::string> regions = {"Central Downs", "East Wood", "East Wood II", "Moumka", "Northwest Peaks"};
    Registry reg;
    for (const std::string& r : regions) reg.add(r);
    std::set<std::string> generated;
    for (std::uint64_t e = 1; e <= 200; ++e) {
        const std::string name = names_api::make_name(dwarven, "dwarven", Kind::settlement, 0, 1, e, reg);
        generated.insert(name);
        for (const std::string& r : regions) CHECK(name != r);
    }
    CHECK(generated.size() == 200);
    // Entity 1 takes its next valid attempt instead of Moumka.
    Registry only;
    only.add("Moumka");
    const std::string e1 = names_api::make_name(dwarven, "dwarven", Kind::settlement, 0, 1, 1, only);
    CHECK(e1 != "Moumka");
    CHECK(generated.count(e1) == 1);
    for (const std::string& r : regions) CHECK(reg.contains(r));
}

TEST_CASE("P3-02 invariant 4: with an empty registry the result is the first valid attempt") {
    for (const std::string_view cid : names_test::CULTURE_IDS) {
        const names_api::Namer namer(shipped_naming(cid));
        for (const Kind k : names_test::KINDS) {
            for (std::uint64_t e = 1; e <= 40; ++e) {
                Registry reg;
                const std::string first = first_valid(namer, k, 11, culture_number(cid), e);
                REQUIRE_FALSE(first.empty());
                CHECK(namer.make(cid, k, 11, culture_number(cid), e, reg) == first);
            }
        }
    }
}

TEST_CASE("P3-02 invariant 3: name_attempt is a pure function, whatever was computed before") {
    // The same 2 x 6 x 3 x 10 x 8 attempts, forwards and backwards, agree.
    struct Key {
        std::uint64_t seed, c, e, t;
        Kind k;
    };
    std::vector<Key> keys;
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        for (std::uint64_t c = 1; c <= 6; ++c) {
            for (const Kind k : names_test::KINDS) {
                for (std::uint64_t e = 1; e <= 10; ++e) {
                    for (std::uint64_t t = 0; t < 8; ++t) keys.push_back(Key{seed, c, e, t, k});
                }
            }
        }
    }
    std::vector<names_api::Attempt> fwd;
    for (const Key& key : keys) {
        fwd.push_back(names_api::attempt(shipped_naming(names_test::CULTURE_IDS[key.c - 1]), key.k, key.seed, key.c,
                                         key.e, key.t));
    }
    for (std::size_t i = keys.size(); i-- > 0;) {
        const Key& key = keys[i];
        const names_api::Attempt a = names_api::attempt(shipped_naming(names_test::CULTURE_IDS[key.c - 1]), key.k,
                                                        key.seed, key.c, key.e, key.t);
        CHECK(a.valid == fwd[i].valid);
        if (a.valid) CHECK(a.name == fwd[i].name);
    }
}

TEST_CASE("P3-02 9.2 test 9: culture A interleaved with culture B gives A's names alone, when B never collides") {
    struct Pair {
        std::string_view a, b;
    };
    for (const Pair p : {Pair{"elven", "orcish"}, Pair{"human", "goblin"}, Pair{"dwarven", "faerie"}}) {
        CAPTURE(p.a);
        CAPTURE(p.b);
        const names_api::Namer na(shipped_naming(p.a));
        const names_api::Namer nb(shipped_naming(p.b));
        const std::uint64_t ca = culture_number(p.a);
        const std::uint64_t cb = culture_number(p.b);
        for (const Kind k : names_test::KINDS) {
            std::vector<std::string> alone;
            {
                Registry reg;
                for (std::uint64_t e = 1; e <= 100; ++e) alone.push_back(na.make(p.a, k, 0, ca, e, reg));
            }
            std::vector<std::string> mixed_a;
            std::set<std::string> mixed_b;
            {
                Registry reg;
                for (std::uint64_t e = 1; e <= 100; ++e) {
                    // B names first and between, different entity ids too.
                    mixed_b.insert(nb.make(p.b, k, 0, cb, 1000 + e, reg));
                    mixed_a.push_back(na.make(p.a, k, 0, ca, e, reg));
                    mixed_b.insert(nb.make(p.b, Kind::settlement, 0, cb, 5000 + e, reg));
                }
            }
            // The precondition, checked: no B name is one A would draw in its
            // first 64 attempts for any of these entities.
            bool collides = false;
            for (std::uint64_t e = 1; e <= 100 && !collides; ++e) {
                for (std::uint64_t t = 0; t < 64; ++t) {
                    const names_api::Attempt at = na.attempt(k, 0, ca, e, t);
                    if (at.valid && mixed_b.count(at.name) != 0) {
                        collides = true;
                        break;
                    }
                }
            }
            REQUIRE_FALSE(collides);
            CHECK(mixed_a == alone);
        }
    }
}

TEST_CASE("P3-02 4.3, 4.4: two runs give the same names; a name does not depend on draws made elsewhere") {
    std::vector<std::string> run1;
    std::vector<std::string> run2;
    for (int run = 0; run < 2; ++run) {
        Registry reg;
        std::vector<std::string>& out = run == 0 ? run1 : run2;
        for (const std::string_view cid : names_test::CULTURE_IDS) {
            const names_api::Namer namer(shipped_naming(cid));
            for (const Kind k : names_test::KINDS) {
                for (std::uint64_t e = 1; e <= 30; ++e) {
                    if (run == 1) {
                        // Unrelated keyed draws in between change nothing (H4).
                        for (std::uint64_t j = 0; j < 5; ++j) (void)names_api::draw(e, j, 1, 2, 3);
                    }
                    out.push_back(namer.make(cid, k, 3, culture_number(cid), e, reg));
                }
            }
        }
    }
    CHECK(run1 == run2);
}

TEST_CASE("P3-02 4.3: at most 64 attempts: a fresh valid name at attempt 64 or later is not used") {
    // Find an entity whose attempts 64 .. 127 hold a valid name that none of
    // attempts 0 .. 63 makes; take all of 0 .. 63's valid names: 4.3 must
    // fall back to a numeral instead of trying attempt 64.
    const Naming human = shipped_naming("human");
    const names_api::Namer namer(human);
    bool tested = false;
    for (std::uint64_t e = 1; e <= 50 && !tested; ++e) {
        std::set<std::string> within;
        std::string first;
        for (std::uint64_t t = 0; t < 64; ++t) {
            const names_api::Attempt a = namer.attempt(Kind::settlement, 0, 5, e, t);
            if (a.valid) {
                if (first.empty()) first = a.name;
                within.insert(a.name);
            }
        }
        bool fresh_later = false;
        for (std::uint64_t t = 64; t < 128 && !fresh_later; ++t) {
            const names_api::Attempt a = namer.attempt(Kind::settlement, 0, 5, e, t);
            fresh_later = a.valid && within.count(a.name) == 0;
        }
        if (!fresh_later || first.empty()) continue;
        Registry reg;
        for (const std::string& s : within) reg.add(s);
        CHECK(namer.make("human", Kind::settlement, 0, 5, e, reg) == first + " II");
        tested = true;
    }
    CHECK(tested);
}

TEST_CASE("P3-02 4.3: the numeral fallback takes the smallest free numeral, from II") {
    const Naming tiny = shipped_naming("tiny");
    const names_api::Namer namer(tiny);
    // Entity 1's first valid attempt decides its base.
    const std::string base = first_valid(namer, Kind::god, 0, 1, 1);
    REQUIRE((base == "Ba" || base == "Da"));
    Registry reg;
    reg.add("Ba");
    reg.add("Da");
    reg.add(base + " II");
    reg.add(base + " IV");
    CHECK(namer.make("tiny", Kind::god, 0, 1, 1, reg) == base + " III");
    CHECK(namer.make("tiny", Kind::god, 0, 1, 1, reg) == base + " V");
    CHECK(namer.make("tiny", Kind::god, 0, 1, 1, reg) == base + " VI");
    // A name of the other base or a gap elsewhere does not matter.
    CHECK_FALSE(reg.contains((base == "Ba" ? std::string("Da") : std::string("Ba")) + " II"));
}

TEST_CASE("P3-02 4.4: a numbered name may reach 64 bytes; above 64 is fatal") {
    // A 48-byte name: root "Aaaaaa" in a 48-byte form; the numerals of k up
    // to 3887 taken, the next is MMMDCCCLXXXVIII (15 bytes): 48 + 1 + 15 = 64.
    Naming n;
    n.onsets = {""};
    n.nuclei = {"aaaaaa"};
    n.codas = {""};
    n.patterns = {"N"};
    n.min_letters = 1;
    n.max_letters = 32;
    n.god_forms = {"{root}" + std::string(42, 'x')};
    n.civilization_forms = {"{root}"};
    n.settlement_forms = {"{root}"};
    const names_api::Namer namer(n);
    const std::string base = "Aaaaaa" + std::string(42, 'x');
    REQUIRE(base.size() == 48);
    {
        Registry reg;
        reg.add(base);
        for (std::uint64_t k = 2; k <= 3887; ++k) reg.add(base + " " + names_test::roman_ref(k));
        const std::string name = namer.make("long", Kind::god, 0, 1, 1, reg);
        CHECK(name == base + " MMMDCCCLXXXVIII");
        CHECK(name.size() == 64);
    }
    {
        // k = 4888 gives MMMMDCCCLXXXVIII (16 bytes): 65 bytes, fatal.
        Registry reg;
        reg.add(base);
        for (std::uint64_t k = 2; k <= 4887; ++k) reg.add(base + " " + names_test::roman_ref(k));
        CHECK_THROWS_WITH_AS((void)namer.make("long", Kind::god, 0, 1, 1, reg), "names: a name longer than 64 bytes",
                             worldgen::FatalError);
    }
}

}  // TEST_SUITE
