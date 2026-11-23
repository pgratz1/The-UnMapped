// Spec P3-02 r19 section 16 (P5-12, generator 0.22.0; the P5-01 world critic's F1), tests 16.4:
//   16.1  the fused-suffix rule: an attempt whose form fuses a tail x after {root} is invalid when the root ends
//         with x or fs(x) (item 1), with another tail of its kind (item 2), or with x's first letter (item 3):
//         sudstead + stead, galwick + wick, ston + ton, dabur + bury, canasham + ford, stead + ham, deas + stead,
//         bant + ton invalid; canasham with the bare form, sud + stead, gal + wick, bea + ford valid; an elven root
//         ending nd + dell invalid; a form with a space after {root} ({root} Ford) not affected;
//   16.2  human forbidden ass rejects beasstont;
//   4.3   a rejected attempt is followed by attempt t + 1 (golden lines hreject and hresult of p3-02-names.txt,
//         written by reference/p3_names_reference.py: every human settlement attempt before the first valid one
//         that 16.1 or 16.2 rejects, entities 1 to 400 at seeds 0 and 42, and make_name's result).
// Invariant 10 (no human settlement name holding ass) on the hresult names. Written from the spec before the
// implementation (test-writer rule).

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"

using names_api::Kind;
using names_api::Naming;
using names_test::dec_u;
using names_test::golden_lines;
using names_test::shipped_naming;
using names_test::split;

namespace {

// A culture that always draws `root` (pattern O over one onset), with the given settlement forms and no
// length bound or forbidden entry: the attempt's validity is the rule under test.
Naming one_root(const Naming& base, const std::string& root, std::optional<std::vector<std::string>> forms = {}) {
    Naming n = base;
    n.onsets = {root};
    n.nuclei = {"a"};
    n.codas = {""};
    n.patterns = {"O"};
    n.god_patterns = std::vector<std::string>{"O"};
    n.min_letters = 1;
    n.max_letters = 32;
    n.forbidden = {};
    if (forms.has_value()) n.settlement_forms = *forms;
    return n;
}

std::string cap(const std::string& root) { return names_api::capitalize_root(root); }

// The validity of the first attempt (t in 0..63, entity 1, seed 0) whose name is `want`.
std::optional<bool> valid_of(const Naming& n, const std::string& want) {
    for (std::uint64_t t = 0; t < 64; ++t) {
        const names_api::Attempt a = names_api::attempt(n, Kind::settlement, 0, 5, 1, t);
        if (a.name == want) return a.valid;
    }
    return std::nullopt;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-02 r19 16.1, 16.4: the fused-suffix rule on the human tails (items 1, 2, 3) and the valid cases") {
    const Naming human = shipped_naming("human");
    struct C {
        const char* root;
        const char* tail;
        bool valid;
    };
    for (const C& c : {C{"sudstead", "stead", false}, C{"galwick", "wick", false}, C{"ston", "ton", false},
                       C{"dabur", "bury", false}, C{"canasham", "ford", false}, C{"stead", "ham", false},
                       C{"deantton", "wick", false}, C{"deas", "stead", false}, C{"bant", "ton", false},
                       C{"sud", "stead", true}, C{"gal", "wick", true}, C{"bea", "ford", true}}) {
        CAPTURE(c.root);
        CAPTURE(c.tail);
        const std::optional<bool> v = valid_of(one_root(human, c.root), cap(c.root) + c.tail);
        REQUIRE(v.has_value());
        CHECK(*v == c.valid);
    }
    // the bare form {root} is not affected: Canasham stays valid
    const std::optional<bool> bare = valid_of(one_root(human, "canasham"), "Canasham");
    REQUIRE(bare.has_value());
    CHECK(*bare);
}

TEST_CASE("P3-02 r19 16.1, 16.4: elven dell; a form with a space after {root}; fs of the tails") {
    const Naming elven = shipped_naming("elven");
    const std::optional<bool> lend = valid_of(one_root(elven, "lend"), "Lenddell");
    REQUIRE(lend.has_value());
    CHECK_FALSE(*lend);                     // item 3: nd + dell
    const std::optional<bool> lin = valid_of(one_root(elven, "lin"), "Lindell");
    REQUIRE(lin.has_value());
    CHECK(*lin);
    // a fixture culture with {root}ford and {root} Ford (B' empty for the second): the rule applies to the first
    const Naming human = shipped_naming("human");
    const std::vector<std::string> fords = {"{root}ford", "{root} Ford"};
    const std::optional<bool> fused = valid_of(one_root(human, "caford", fords), "Cafordford");
    const std::optional<bool> spaced = valid_of(one_root(human, "caford", fords), "Caford Ford");
    REQUIRE(fused.has_value());
    REQUIRE(spaced.has_value());
    CHECK_FALSE(*fused);
    CHECK(*spaced);
    // fs(bury) = bur: a root ending bur + bury (item 1 by the first syllable); fs(stead) = stead, fs(ton) = ton:
    // a root ending tea + stead or to + ton is not item 1
    const std::optional<bool> bur = valid_of(one_root(human, "dabur"), "Daburbury");
    REQUIRE(bur.has_value());
    CHECK_FALSE(*bur);
    const std::optional<bool> tea = valid_of(one_root(human, "rotea"), "Roteastead");
    REQUIRE(tea.has_value());
    CHECK(*tea);
    const std::optional<bool> to = valid_of(one_root(human, "brato"), "Bratoton");
    REQUIRE(to.has_value());
    CHECK(*to);   // ton: fs(ton) = ton, and o is not t (no item applies)
}

TEST_CASE("P3-02 r19 16.2: the human forbidden list gains ass; beasstont is rejected") {
    const Naming human = shipped_naming("human");
    REQUIRE_FALSE(human.forbidden.empty());
    CHECK(human.forbidden.back() == "ass");
    Naming n = one_root(human, "beasstont", std::vector<std::string>{"{root}"});
    n.forbidden = human.forbidden;
    const std::optional<bool> v = valid_of(n, "Beasstont");
    REQUIRE(v.has_value());
    CHECK_FALSE(*v);
}

TEST_CASE("P3-02 r19 16.1, 4.3: every rejected human attempt is followed by t + 1 (golden hreject, hresult)") {
    const Naming human = shipped_naming("human");
    const std::vector<std::string>& rej = golden_lines("hreject");
    REQUIRE(rej.size() >= 10U);
    std::size_t by_item[4] = {0, 0, 0, 0};
    for (const std::string& line : rej) {
        // hreject <seed> human settlement <c> <e> <t> <root> <item 1|2|3|ass> <name>
        const std::vector<std::string> w = split(line, 10);
        CAPTURE(line);
        const names_api::Attempt a = names_api::attempt(human, Kind::settlement, dec_u(w[1]), dec_u(w[4]),
                                                        dec_u(w[5]), dec_u(w[6]), names_test::shipped_filter());
        CHECK(a.name == w[9]);
        CHECK_FALSE(a.valid);
        if (w[8] == "1" || w[8] == "2" || w[8] == "3") ++by_item[std::stoi(w[8])];
    }
    CHECK(by_item[3] >= 1U);   // the join of s + stead or nt + ton is the common case (16.3)
    const std::vector<std::string>& res = golden_lines("hresult");
    REQUIRE_FALSE(res.empty());
    for (const std::string& line : res) {
        // hresult <seed> human settlement <c> <e> <attempt|F> <name> <attempt at r17|F> <name at r17>
        const std::vector<std::string> w = split(line, 9);
        CAPTURE(line);
        names_api::Registry reg;
        const names_api::Choice c = names_api::choose_name(human, "human", Kind::settlement, dec_u(w[1]), dec_u(w[4]),
                                                           dec_u(w[5]), reg, names_test::shipped_filter());
        const std::string& at = w[6];
        const std::string& name = w[7];
        CHECK(c.name == name);
        CHECK(c.name != split(w[8], 2)[1]);   // the r17 name was a rejected attempt
        if (at != "F") CHECK(c.attempt == dec_u(at));
        CHECK(names_test::lower(c.name).find("ass") == std::string::npos);   // invariant 10 (16.2)
    }
}

}  // TEST_SUITE
