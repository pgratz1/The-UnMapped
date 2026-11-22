// P3-02 r15 section 14 (P3-11, generator 0.20.0): the faerie syllables of 14.1, the rule of 14.2 (no syllable
// repeated twice in a row, for every culture), the filter entries of 14.3, and invariant 8 of 14.4.
//
// Expected values are written from the spec (14.2's table, 14.3's names) or read from
// worldgen/tests/golden/p3-02-names.txt (lines rrow, rattempt, rresult, sattempt; the lists of 9.3 and r10's core
// lists), written by the test writer's reference p3_names_reference.py. The syllable split and the repeated test
// are the test's own (14.2), applied to the strings the test draws itself with the keyed draws of 4.1 and 4.2
// (names_api::draw, pick), so the library is compared with an independent reading. The critics' stems are written
// as concatenations (P3-02 11.7 item 10). Written before the implementation (test-writer rule): the attempt and
// list cases fail until the library has the rule, the filter cases until the r15 filter files land.

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "worldgen/core/hash.hpp"

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

using Drawn = std::vector<std::pair<char, std::string>>;  // (symbol, string drawn) per pattern position

// 14.2, the test's own: the syllables (s_i, b_i) of an attempt. A boundary at the first symbol, before every O,
// before every N not directly preceded by O, and at every separator (which belongs to no syllable).
std::vector<std::pair<std::string, std::string>> syllables(const Drawn& d) {
    std::vector<std::pair<std::string, std::string>> out;
    bool open = false;
    std::pair<std::string, std::string> cur;
    char prev = 0;
    for (const auto& [sym, piece] : d) {
        if (sym == '\'' || sym == '-') {
            if (open) out.push_back(cur);
            open = false;
            prev = sym;
            continue;
        }
        if (open && (sym == 'O' || (sym == 'N' && prev != 'O'))) {
            out.push_back(cur);
            open = false;
        }
        if (!open) {
            cur = {};
            open = true;
        }
        cur.first += piece;
        if (sym != 'C') cur.second += piece;
        prev = sym;
    }
    if (open) out.push_back(cur);
    return out;
}

bool own_repeated(const Drawn& d) {
    const auto sy = syllables(d);
    for (std::size_t i = 0; i + 1 < sy.size(); ++i) {
        if (!sy[i].first.empty() && (sy[i].first == sy[i + 1].first || sy[i].first == sy[i + 1].second)) return true;
    }
    return false;
}

Drawn row(std::string_view pattern, const std::vector<std::string>& pieces) {
    Drawn d;
    std::size_t k = 0;
    for (const char s : pattern) {
        if (s == 'O' || s == 'N' || s == 'C') {
            d.emplace_back(s, pieces.at(k++));
        } else {
            d.emplace_back(s, std::string(1, s));
        }
    }
    return d;
}

// 4.2, the test's own: the pattern and the strings an attempt draws (u(j) = draw(S_K, t, c, e, j)).
Drawn own_drawn(const Naming& n, Kind k, std::uint64_t seed, std::uint64_t c, std::uint64_t e, std::uint64_t t) {
    const std::uint64_t key = worldgen::stream_key(seed, names_test::stream_of(k));
    const std::vector<std::string>& pats = (k == Kind::god && n.god_patterns) ? *n.god_patterns : n.patterns;
    (void)names_api::pick(names_api::draw(key, t, c, e, 0), names_test::forms_of(n, k).size());
    const std::string& pat = pats.at(names_api::pick(names_api::draw(key, t, c, e, 1), pats.size()));
    Drawn d;
    std::uint64_t j = 2;
    for (const char s : pat) {
        const std::vector<std::string>* arr = s == 'O' ? &n.onsets : s == 'N' ? &n.nuclei : s == 'C' ? &n.codas : nullptr;
        if (arr == nullptr) {
            d.emplace_back(s, std::string(1, s));
            continue;
        }
        d.emplace_back(s, arr->at(names_api::pick(names_api::draw(key, t, c, e, j++), arr->size())));
    }
    return d;
}

std::vector<std::string> undot(const std::string& s) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (true) {
        const std::size_t c = s.find(',', at);
        const std::string x = s.substr(at, c == std::string::npos ? std::string::npos : c - at);
        out.push_back(x == "." ? std::string() : x);
        if (c == std::string::npos) break;
        at = c + 1;
    }
    return out;
}

Naming row_culture(const std::string& pattern, const std::string& on, const std::string& nu, const std::string& co) {
    Naming n;
    n.onsets = undot(on);
    n.nuclei = undot(nu);
    n.codas = undot(co);
    n.patterns = {pattern};
    n.god_patterns = n.patterns;
    n.min_letters = 1;
    n.max_letters = 32;
    n.god_forms = {"{root}"};
    n.civilization_forms = {"{root}"};
    n.settlement_forms = {"{root}"};
    return n;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-02 r15 14.2: the test's own syllables and repeated test on 14.2's table and 14.4's OONC case") {
    struct Row {
        const char* pattern;
        std::vector<std::string> pieces;
        bool repeated;
    };
    const std::vector<Row> rows = {
        {"ONONON", {"w", "i", "fl", "ee", "fl", "ee"}, true},   // wifleeflee
        {"ONONC", {"l", "i", "l", "i", "n"}, true},             // lilin
        {"ONON", {"f", "i", "f", "i"}, true},                   // fifi
        {"ONON", {"f", "ee", "f", "ee"}, true},                 // feefee
        {"ONON", {"n", "o", "n", "o"}, true},                   // nono
        {"ONCONC", {"d", "a", "ck", "d", "a", "ck"}, true},     // dackdack
        {"ONC-ONC", {"kr", "a", "k", "kr", "a", "k"}, true},    // krak-krak
        {"ONCONC", {"gr", "a", "n", "gr", "a", "k"}, false},    // grangrak
        {"ONON", {"f", "ee", "f", "i"}, false},                 // feefi
        {"ONONON", {"tw", "i", "wh", "i", "w", "ou"}, false},   // twiwhiwou
        {"ONCON", {"l", "i", "ll", "", "ia"}, false},           // lillia
        {"OONC", {"", "l", "i", "n"}, false},                   // 14.4: syllables "" and "lin"
    };
    for (const Row& r : rows) {
        CAPTURE(std::string_view(r.pattern));
        CHECK(own_repeated(row(r.pattern, r.pieces)) == r.repeated);
    }
    const auto lillia = syllables(row("ONCON", {"l", "i", "ll", "", "ia"}));
    REQUIRE(lillia.size() == 2U);
    CHECK(lillia[0] == std::pair<std::string, std::string>{"lill", "li"});
    CHECK(lillia[1] == std::pair<std::string, std::string>{"ia", "ia"});
    const auto oonc = syllables(row("OONC", {"", "l", "i", "n"}));
    REQUIRE(oonc.size() == 2U);
    CHECK(oonc[0].first.empty());
    CHECK(oonc[1] == std::pair<std::string, std::string>{"lin", "li"});
    // NC'ONC: ug | krak (the separator is a boundary and belongs to no syllable)
    const auto orc = syllables(row("NC'ONC", {"u", "g", "kr", "a", "k"}));
    REQUIRE(orc.size() == 2U);
    CHECK(orc[0].first == "ug");
    CHECK(orc[1].first == "krak");
}

TEST_CASE("P3-02 r15 14.2: the library rejects exactly the repeated rows (golden rrow, test cultures from the line)") {
    // the spec's expected result per row (14.2's table, 14.4's OONC), by the row's root
    const std::vector<std::pair<std::string, bool>> want = {
        {"wifleeflee", false}, {"lilin", false}, {"fifi", false},  {"feefee", false}, {"nono", false},
        {"dackdack", false},   {"krak-krak", false}, {"grangrak", true}, {"feefi", true}, {"twiwhiwou", true},
        {"lillia", true},      {"lin", true}};
    const std::vector<std::string>& lines = golden_lines("rrow");
    REQUIRE(lines.size() == want.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        // rrow <i> <pattern> <onsets> <nuclei> <codas> <pieces> <e> <t> <valid> <root>
        const std::vector<std::string> w = split(lines[i], 11);
        CAPTURE(lines[i]);
        CHECK(w[10] == want[i].first);
        CHECK((w[9] == "1") == want[i].second);
        const Naming n = row_culture(w[2], w[3], w[4], w[5]);
        const Drawn mine = own_drawn(n, Kind::settlement, 0, 1, dec_u(w[7]), dec_u(w[8]));
        CHECK(mine == row(w[2], undot(w[6])));  // the line's attempt draws exactly the row's strings
        CHECK(own_repeated(mine) == !want[i].second);
        const names_api::Attempt a =
            names_api::attempt(n, Kind::settlement, 0, 1, dec_u(w[7]), dec_u(w[8]), names_test::shipped_filter());
        CHECK(a.valid == want[i].second);
        if (a.valid) CHECK(a.root == w[10]);
    }
}

TEST_CASE("P3-02 r15 14.2: every attempt of the row cultures; a repeated attempt is never valid (golden rattempt)") {
    const std::vector<std::string>& rows = golden_lines("rrow");
    const std::vector<std::string>& lines = golden_lines("rattempt");
    REQUIRE(lines.size() == rows.size() * 48U);  // e 1-3, t 0-15 per row
    std::set<std::pair<bool, bool>> seen;
    for (const std::string& l : lines) {
        // rattempt <i> <e> <t> <valid> <repeated> <pieces> <name>
        const std::vector<std::string> w = split(l, 8);
        CAPTURE(l);
        const std::vector<std::string> r = split(rows.at(dec_u(w[1]) - 1), 11);
        const Naming n = row_culture(r[2], r[3], r[4], r[5]);
        const Drawn mine = own_drawn(n, Kind::settlement, 0, 1, dec_u(w[2]), dec_u(w[3]));
        CHECK(mine == row(r[2], undot(w[6])));
        const bool valid = w[4] == "1", rep = w[5] == "1";
        CHECK(own_repeated(mine) == rep);
        if (rep) CHECK_FALSE(valid);  // a repeated attempt is invalid (some others are filtered or triple, 13.2)
        const names_api::Attempt a =
            names_api::attempt(n, Kind::settlement, 0, 1, dec_u(w[2]), dec_u(w[3]), names_test::shipped_filter());
        CHECK(a.valid == valid);
        if (valid) CHECK(a.name == w[7]);
        seen.insert({valid, rep});
    }
    CHECK(seen.count({true, false}) == 1U);
    CHECK(seen.count({false, true}) == 1U);
}

TEST_CASE("P3-02 r15 14.4: a repeated attempt is skipped for attempt t + 1 and is never the fallback's first name") {
    const std::vector<std::string>& lines = golden_lines("rresult");
    REQUIRE(lines.size() == 4U);
    const Naming rep = shipped_naming("rep");
    const names_api::Filter& f = names_test::shipped_filter();
    for (const std::string& l : lines) {
        // rresult rep <kind> <seed> 1 <e> <attempt|F> <name> [<registry>]
        const std::vector<std::string> w = split(l, 9);
        CAPTURE(l);
        Registry reg;
        const bool with_ba = w.size() == 9U;
        if (with_ba) reg.add("Ba");
        const names_api::Choice c = names_api::choose_name(rep, "rep", kind_of(w[2]), dec_u(w[3]), dec_u(w[4]),
                                                           dec_u(w[5]), reg, f);
        CHECK(c.name == (with_ba ? std::string("Ba II") : w[7]));
        CHECK(c.fallback == (w[6] == "F"));
        if (w[6] != "F") CHECK(c.attempt == dec_u(w[6]));
    }
    // the settlement entity of the lines: its attempt 0 draws b a b a (Baba), and the name is the next valid attempt
    const std::vector<std::string> w = split(lines[2], 8);
    const std::uint64_t e = dec_u(w[5]);
    CHECK(own_drawn(rep, Kind::settlement, 0, 1, e, 0) == row("ONON", {"b", "a", "b", "a"}));
    CHECK_FALSE(names_api::attempt(rep, Kind::settlement, 0, 1, e, 0, f).valid);
    CHECK(dec_u(w[6]) > 0U);
    Registry reg;
    reg.add("Ba");
    const names_api::Choice fb = names_api::choose_name(rep, "rep", Kind::settlement, 0, 1, e, reg, f);
    CHECK(fb.fallback);
    CHECK(fb.name == "Ba II");  // never "Baba II": a repeated attempt is invalid for every purpose of 4.3
}

TEST_CASE("P3-02 r15 14.2: shipped cultures at seed 42, the first attempt rejected by 14.2 alone and the next (golden sattempt)") {
    const std::vector<std::string>& lines = golden_lines("sattempt");
    REQUIRE(lines.size() >= 20U);
    for (const std::string& l : lines) {
        // sattempt <seed> <culture> <kind> <c> <e> <t> <valid> <repeated> <name>
        const std::vector<std::string> w = split(l, 10);
        CAPTURE(l);
        const Naming n = shipped_naming(w[2]);
        const Kind k = kind_of(w[3]);
        const names_api::Attempt a =
            names_api::attempt(n, k, dec_u(w[1]), dec_u(w[4]), dec_u(w[5]), dec_u(w[6]), names_test::shipped_filter());
        CHECK(a.valid == (w[7] == "1"));
        if (a.valid) CHECK(a.name == w[9]);
        CHECK(own_repeated(own_drawn(n, k, dec_u(w[1]), dec_u(w[4]), dec_u(w[5]), dec_u(w[6]))) == (w[8] == "1"));
    }
}

TEST_CASE("P3-02 r15 14.1: faerie attempts: no p or b, a root with a + ss is invalid (forbidden ass), Lyss is valid") {
    const Naming fa = shipped_naming("faerie");
    const names_api::Filter& f = names_test::shipped_filter();
    const std::uint64_t c = culture_number("faerie");
    bool lyss = false, ass_seen = false;
    for (std::uint64_t e = 1; e <= 3000 && !(lyss && ass_seen); ++e) {
        for (std::uint64_t t = 0; t < 4; ++t) {
            const Drawn d = own_drawn(fa, Kind::settlement, 0, c, e, t);
            std::string root;
            for (const auto& [s, p] : d) root += p;
            const names_api::Attempt a = names_api::attempt(fa, Kind::settlement, 0, c, e, t, f);
            if (a.valid) {
                CHECK(a.name.find_first_of("pbPB") == std::string::npos);
                CHECK(a.root.find("ass") == std::string::npos);
            }
            if (root.find("ass") != std::string::npos) {
                ass_seen = true;
                CHECK_FALSE(a.valid);
            }
            if (root == "lyss") {
                lyss = true;
                CHECK(a.valid);
            }
        }
    }
    CHECK(lyss);
    CHECK(ass_seen);
}

TEST_CASE("P3-02 r15 14.3: Tardham, Tardwuldwick, The Horktard Throne and Stard are filtered; Standard is not") {
    const names_api::Filter& f = names_test::shipped_filter();
    const std::string frag = std::string("ta") + "rd";
    for (const std::string& w : {frag + "ham", frag + "wuldwick", "hork" + frag, "s" + frag}) {
        CAPTURE(w.size());
        CHECK(names_api::name_filtered(f, w));
        CHECK(names_test::ref_filtered(f, w));
    }
    CHECK(names_api::root_word("The {root} Throne", "hork" + frag) == "hork" + frag);
    for (const char* w : {"standard", "tarn", "dart", "stad"}) {
        CAPTURE(std::string_view(w));
        CHECK_FALSE(names_api::name_filtered(f, w));
    }
    // the four English words of 14.3 as a whole root word; inside a longer word they pass
    for (const char* w : {"via", "van", "von", "ova"}) {
        CAPTURE(std::string_view(w));
        CHECK(names_api::name_filtered(f, w));
        CHECK_FALSE(names_api::name_filtered(f, std::string("x") + w + "x"));
    }
}

TEST_CASE("P3-02 r15 14.4 invariant 8: no name of the golden lists comes from a repeated attempt (by replay)") {
    std::size_t n = 0;
    auto check_line = [&](const std::string& l, std::size_t off) {
        const std::vector<std::string> w = split(l, 6 + off);
        const std::uint64_t seed = dec_u(w[off]);
        const std::string& cid = w[1 + off];
        const Kind k = kind_of(w[2 + off]);
        const std::uint64_t e = dec_u(w[3 + off]);
        const Naming nm = shipped_naming(cid);
        std::uint64_t t = 0;
        if (w[4 + off] == "F") {
            while (t < 64 && !names_api::attempt(nm, k, seed, culture_number(cid), e, t, names_test::shipped_filter()).valid) {
                ++t;
            }
        } else {
            t = dec_u(w[4 + off]);
        }
        CAPTURE(l);
        CHECK_FALSE(own_repeated(own_drawn(nm, k, seed, culture_number(cid), e, t)));
        ++n;
    };
    for (const std::string& l : golden_lines("name")) check_line(l, 0);
    for (const std::string& l : golden_lines("core")) check_line(l, 1);
    CHECK(n >= 1800U);
}

}  // TEST_SUITE
