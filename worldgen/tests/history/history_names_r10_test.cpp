// P3-02 r10 section 12 (P3-10, generator 0.19.0): the filter entries of 12.1 (data) and the core rule of
// 12.2 (no two civilization or settlement names of a world whose cores are equal or one letter apart),
// with invariant 6 of 12.3.
//
// Expected values are written from the spec (12.1's table, 12.3's units) or read from
// worldgen/tests/golden/p3-02-names.txt (lines corename, corenear, corefallback, core, coresha256), written
// by the test writer's reference p3_names_reference.py at 0.19.0. The test's own core rule (below) runs on
// names_api::attempt, which exists at 0.18.0; the library's seams of 12.2 (interim names in
// p3_10_names_api.hpp) are mapped to the engineer's API. The data tests and the golden lists fail
// until the 0.19.0 filter files land (game/data/name_filters and both fixture copies). The critics' stems
// are written as concatenations (P3-02 11.7 item 10).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "p3_10_names_api.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

using names_api::Kind;
using names_api::Naming;
using names_api::Registry;
using names_test::culture_number;
using names_test::dec_u;
using names_test::golden_lines;
using names_test::kind_of;
using names_test::shipped_naming;
using names_test::split;

namespace {

namespace wd = worldgen::data;

// 12.2, the test's own: the root's letters without ' and - (the root is lower case).
std::string own_core(std::string_view root) {
    std::string out;
    for (const char c : root) {
        if (c != '\'' && c != '-') out.push_back(c);
    }
    return names_test::lower(out);
}

// Levenshtein distance at most 1.
bool lev1(const std::string& a0, const std::string& b0) {
    if (a0 == b0) return true;
    const std::string& a = a0.size() <= b0.size() ? a0 : b0;
    const std::string& b = a0.size() <= b0.size() ? b0 : a0;
    if (b.size() - a.size() > 1) return false;
    if (a.size() == b.size()) {
        std::size_t diff = 0;
        for (std::size_t i = 0; i < a.size(); ++i) diff += a[i] != b[i] ? 1U : 0U;
        return diff == 1;
    }
    std::size_t i = 0;
    while (i < a.size() && a[i] == b[i]) ++i;
    return a.substr(i) == b.substr(i + 1);
}

bool own_near(const std::string& core, const std::set<std::string>& cores) {
    return std::any_of(cores.begin(), cores.end(), [&](const std::string& k) { return lev1(core, k); });
}

struct OwnChoice {
    std::string name;
    std::string token;  // attempt number or "F"
};

// 4.3 with 12.2's K, on names_api::attempt.
OwnChoice own_choose(const Naming& n, Kind k, std::uint64_t seed, std::uint64_t c, std::uint64_t e,
                     Registry& reg, std::set<std::string>* cores, const names_api::Filter& f) {
    std::string first, first_core;
    bool have = false;
    for (std::uint64_t t = 0; t < 64; ++t) {
        const names_api::Attempt a = names_api::attempt(n, k, seed, c, e, t, f);
        if (!a.valid) continue;
        const std::string core = own_core(a.root);
        if (!have) {
            first = a.name;
            first_core = core;
            have = true;
        }
        if (!reg.contains(a.name) && (cores == nullptr || !own_near(core, *cores))) {
            reg.add(a.name);
            if (cores != nullptr) cores->insert(core);
            return {a.name, std::to_string(t)};
        }
    }
    REQUIRE(have);
    std::uint64_t kk = 2;
    while (reg.contains(first + " " + names_test::roman_ref(kk))) ++kk;
    const std::string nm = first + " " + names_test::roman_ref(kk);
    reg.add(nm);
    if (cores != nullptr) cores->insert(first_core);
    return {nm, "F"};
}

std::set<std::string> set_of(const std::string& joined) {
    std::set<std::string> out;
    if (joined == "-") return out;
    std::size_t p = 0;
    while (true) {
        const std::size_t q = joined.find(',', p);
        out.insert(joined.substr(p, q == std::string::npos ? std::string::npos : q - p));
        if (q == std::string::npos) break;
        p = q + 1;
    }
    return out;
}

wd::ContentSet shipped_content() {
    const auto r = wd::load_content(wd::builtin_kinds(), std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / "game" / "data");
    REQUIRE(r.ok());
    return *r.value;
}

}  // namespace

TEST_SUITE("history-names") {

TEST_CASE("P3-02 r10 12.1, 12.3: the eight filter entries reject the names of 12.1's table; Gramm and the culture syllables pass") {
    const wd::ContentSet content = shipped_content();  // load_content runs name_filter_not_redundant (11.2)
    const names_api::Filter lib = names_api::filter_from_content(content);
    const std::string su = std::string("su") + "ck";
    const std::string sh = std::string("shi") + "et";
    // root words as 11.3 forms them (lower case, the whole root word)
    const std::vector<std::string> rejected = {
        su + "ford", sh + "y", "real" + std::string("de") + "adstead", "tim" + std::string("ha") + "ck",
        std::string("wh") + "op", std::string("fl") + "opfia", std::string("go") + "rdman", std::string("gram") + "mam",
        su + "bury", "gul" + su + "ton", sh + "wo", std::string("de") + "adstead", std::string("ha") + "ckford",
        "as" + std::string("ha") + "ckstead", std::string("wh") + "opia", "lee" + std::string("fl") + "op",
        std::string("go") + "rdford", std::string("gram") + "maik"};
    for (const std::string& w : rejected) {
        CAPTURE(w);
        CHECK(names_api::name_filtered(lib, w));
        CHECK(names_api::name_filtered(names_test::shipped_filter(), w));
    }
    for (const char* w : {"gramm", "mammga", "twiwhiwou", "timmthorin", "kaidinmamm"}) {
        CAPTURE(std::string_view(w));
        CHECK_FALSE(names_api::name_filtered(lib, w));
    }
    // 12.1 sizes after the edit: crude 70 / 16 / 14; words 5 / 1 / 389; after P3-02 r13 13.1 (the second
    // round): crude 72 / 12 / 14, words 5 / 1 / 387; after P3-02 r15 14.3 (P3-11): crude 71 / 12 / 14, words
    // 5 / 1 / 391; after P3-02 r17 15.1 (P5-01): crude 71 / 12 / 15 (`fannie`).
    const wd::KindContent* kind = content.find_kind("name_filter");
    REQUIRE(kind != nullptr);
    for (const auto& d : kind->definitions) {
        CAPTURE(d.id);
        if (d.id == "crude") {
            CHECK(d.record.get_array("anywhere").size() == 71U);
            CHECK(d.record.get_array("at_start").size() == 12U);
            CHECK(d.record.get_array("whole").size() == 15U);
        }
        if (d.id == "words") {
            CHECK(d.record.get_array("anywhere").size() == 5U);
            CHECK(d.record.get_array("at_start").size() == 1U);
            CHECK(d.record.get_array("whole").size() == 391U);
        }
    }
}

TEST_CASE("P3-02 r10 12.2, 12.3: the core and the near test (skat, sligg, kuzz, kaz, kuz near; kazz not)") {
    CHECK(own_core("skat") == "skat");
    CHECK(own_core("ogh'krak") == "oghkrak");
    CHECK(own_core("gnink-snunk") == "gninksnunk");
    const std::set<std::string> skat = {"skat"}, slig = {"slig"}, kuz = {"kuz"};
    CHECK(own_near("skat", skat));
    CHECK(own_near("sligg", slig));
    for (const char* k : {"kuzz", "kaz", "kuz"}) CHECK_MESSAGE(own_near(k, kuz), k);
    CHECK_FALSE(own_near("kazz", kuz));
    CHECK_FALSE(own_near("nili", {"muli"}));
    for (const std::string& line : golden_lines("corename")) {
        CAPTURE(line);
        const auto t = split(line, 3);
        CHECK(own_core(t[1]) == t[2]);
        CHECK(names_api_r10::name_core(t[1]) == t[2]);
    }
    const auto& near = golden_lines("corenear");
    CHECK(near.size() >= 10U);
    for (const std::string& line : near) {
        CAPTURE(line);
        const auto t = split(line, 4);
        CHECK(own_near(t[1], set_of(t[2])) == (t[3] == "1"));
        CHECK(names_api_r10::core_near(t[1], set_of(t[2])) == (t[3] == "1"));
    }
}

TEST_CASE("P3-02 r10 12.3: with skat in K an attempt with root skat is skipped and the next attempt accepted") {
    // A goblin settlement whose first valid attempt has a root R: with own_core(R) in K the name is the next
    // valid attempt that is neither in the registry nor near K.
    const Naming g = shipped_naming("goblin");
    const names_api::Filter& f = names_test::shipped_filter();
    const names_api::Attempt a0 = [&] {
        for (std::uint64_t t = 0; t < 64; ++t) {
            const names_api::Attempt a = names_api::attempt(g, Kind::settlement, 11, 4, 1, t, f);
            if (a.valid) return a;
        }
        FAIL("no valid attempt");
        return names_api::Attempt{};
    }();
    Registry reg;
    std::set<std::string> cores = {own_core(a0.root)};
    const OwnChoice c = own_choose(g, Kind::settlement, 11, 4, 1, reg, &cores, f);
    CHECK(c.name != a0.name);
    CHECK(c.token != "F");
    Registry reg2;
    std::set<std::string> k2 = {own_core(a0.root)};
    const names_api::Choice lc = names_api_r10::choose_name_k(g, f, "goblin", Kind::settlement, 11, 4, 1, reg2, k2);
    CHECK(lc.name == c.name);
    CHECK(k2 == cores);
}

TEST_CASE("P3-02 r10 12.3: every attempt near K falls back to the first valid name with a numeral; its core joins K") {
    const auto& lines = golden_lines("corefallback");
    REQUIRE(lines.size() == 1U);
    const auto t = split(lines[0], 9);
    // corefallback tiny settlement 0 1 1 <attempt|F> <name> <K>; the name holds one space
    const std::string& line = lines[0];
    CAPTURE(line);
    CHECK(t[6] == "F");
    const std::string name = t[7] + " " + t[8].substr(0, t[8].find(' '));
    const std::string after = line.substr(line.rfind(' ') + 1);
    Registry reg;
    std::set<std::string> cores = {"ba"};
    const OwnChoice c = own_choose(shipped_naming("tiny"), Kind::settlement, 0, 1, 1, reg, &cores, names_api::Filter{});
    CHECK(c.token == "F");
    CHECK(c.name == name);
    CHECK(cores == set_of(after));
    Registry r2;
    std::set<std::string> k2 = {"ba"};
    const names_api::Choice lc =
        names_api_r10::choose_name_k(shipped_naming("tiny"), names_api::Filter{}, "tiny", Kind::settlement, 0, 1, 1,
                                     r2, k2);
    CHECK(lc.fallback);
    CHECK(lc.name == name);
    CHECK(k2 == set_of(after));
}

TEST_CASE("P3-02 r10 12.2, 12.3: the golden core lists (seeds 0 and 42; gods without K, as before) and invariant 6") {
    const auto& expected = golden_lines("core");
    REQUIRE(expected.size() == 2 * 6 * 3 * 50);
    const auto& plain = golden_lines("name");
    std::size_t next = 0;
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        Registry reg;
        std::set<std::string> cores;
        std::vector<std::pair<std::string, std::string>> named;  // (name, core) of accepted non-fallback names
        std::string joined;
        for (const std::string_view cid : names_test::CULTURE_IDS) {
            const Naming n = shipped_naming(cid);
            const names_api::Filter& f = names_test::shipped_filter();
            for (const Kind k : names_test::KINDS) {
                for (std::uint64_t e = 1; e <= 50; ++e) {
                    REQUIRE(next < expected.size());
                    const std::string& line = expected[next++];
                    CAPTURE(line);
                    const auto t = split(line, 7);
                    REQUIRE(dec_u(t[1]) == seed);
                    REQUIRE(t[2] == cid);
                    REQUIRE(kind_of(t[3]) == k);
                    const OwnChoice c =
                        own_choose(n, k, seed, culture_number(cid), e, reg, k == Kind::god ? nullptr : &cores, f);
                    CHECK(c.name == t[6]);
                    CHECK(c.token == t[5]);
                    if (k != Kind::god && c.token != "F") {
                        const names_api::Attempt a =
                            names_api::attempt(n, k, seed, culture_number(cid), e, dec_u(c.token), f);
                        named.emplace_back(c.name, own_core(a.root));
                    }
                    if (!joined.empty()) joined += "\n";
                    joined += line;
                }
            }
        }
        for (const std::string& line : golden_lines("coresha256")) {
            const auto t = split(line, 3);
            if (dec_u(t[1]) == seed) CHECK(worldgen::sha256_hex(joined) == t[2]);
        }
        // Invariant 6 (12.3): no two cores equal or one letter apart (no fallback occurs in these lists).
        std::size_t pairs = 0;
        for (std::size_t i = 0; i < named.size(); ++i)
            for (std::size_t j = i + 1; j < named.size(); ++j) pairs += lev1(named[i].second, named[j].second) ? 1U : 0U;
        CHECK(pairs == 0U);
    }
    // Gods are drawn as before (P3-03 keeps 4.3): the god lines equal those of the plain lists.
    std::vector<std::string> g1, g2;
    for (const std::string& l : expected)
        if (l.find(" god ") != std::string::npos) g1.push_back(l.substr(5));
    for (const std::string& l : plain)
        if (l.find(" god ") != std::string::npos) g2.push_back(l);
    CHECK(g1 == g2);
}

}  // TEST_SUITE
