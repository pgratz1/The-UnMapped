#pragma once

// Test-owned data and helpers of the P3-02 names tests: the six shipped
// naming blocks of spec section 5 transcribed (not read from the files), the
// test cultures `tiny` and `never` shared with the reference
// (worldgen/tests/reference/p3_names_reference.py), the golden file reader,
// and an independent shape check of invariant 1 (section 6). No library
// symbol is used here except through p3_02_names_api.hpp.

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_02_names_api.hpp"

namespace names_test {

using names_api::Kind;
using names_api::Naming;

inline constexpr std::uint64_t M64 = ~std::uint64_t{0};
inline constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
inline constexpr std::int64_t HALF_RAW = std::int64_t{1} << 31;

inline constexpr std::array<Kind, 3> KINDS = {Kind::god, Kind::civilization, Kind::settlement};

inline std::string_view kind_name(Kind k) {
    switch (k) {
    case Kind::god: return "god";
    case Kind::civilization: return "civilization";
    case Kind::settlement: return "settlement";
    }
    return "?";
}

inline Kind kind_of(std::string_view s) {
    if (s == "god") return Kind::god;
    if (s == "civilization") return Kind::civilization;
    if (s == "settlement") return Kind::settlement;
    throw std::runtime_error("unknown kind " + std::string(s));
}

inline std::string_view stream_of(Kind k) {
    switch (k) {
    case Kind::god: return "hist.names.god";
    case Kind::civilization: return "hist.names.civilization";
    case Kind::settlement: return "hist.names.settlement";
    }
    return "?";
}

inline const std::vector<std::string>& forms_of(const Naming& n, Kind k) {
    switch (k) {
    case Kind::god: return n.god_forms;
    case Kind::civilization: return n.civilization_forms;
    case Kind::settlement: return n.settlement_forms;
    }
    return n.god_forms;
}

// --- Section 5, transcribed ------------------------------------------------------------------------

// Canonical culture order: culture number c = index + 1 (spec 4.6).
inline constexpr std::array<std::string_view, 6> CULTURE_IDS = {"dwarven", "elven", "faerie", "goblin", "human", "orcish"};

inline std::uint64_t culture_number(std::string_view id) {
    for (std::size_t i = 0; i < CULTURE_IDS.size(); ++i) {
        if (CULTURE_IDS[i] == id) return i + 1;
    }
    return 1;  // test cultures
}

inline Naming shipped_naming(std::string_view id) {
    Naming n;
    if (id == "dwarven") {
        n.onsets = {"d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"};
        n.nuclei = {"a", "u", "o", "i", "ai", "ou"};
        n.codas = {"r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"};
        n.patterns = {"ONC", "ONCONC", "ONCONC", "ONCON"};
        n.min_letters = 4;
        n.max_letters = 10;
        n.forbidden = {"mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk"};
        n.god_forms = {"{root}"};
        n.civilization_forms = {"The Hold of {root}", "Clan {root}", "The {root} Throne"};
        n.settlement_forms = {"{root}", "{root} Deep", "{root} Hold", "{root}"};
    } else if (id == "elven") {
        n.onsets = {"", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"};
        n.nuclei = {"a", "e", "i", "ae", "ie", "o"};
        n.codas = {"", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"};
        n.patterns = {"ONONC", "ONONC", "ONCON", "ONONON", "NCONC"};
        n.min_letters = 4;
        n.max_letters = 11;
        n.forbidden = {"aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"};
        n.god_forms = {"{root}"};
        n.civilization_forms = {"The Court of {root}", "The Realm of {root}", "{root}"};
        n.settlement_forms = {"{root}", "{root}", "{root}", "{root}dell"};
    } else if (id == "faerie") {
        n.onsets = {"", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"};
        n.nuclei = {"i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"};
        n.codas = {"", "", "ll", "n", "sh", "th", "p", "sk", "ss"};
        n.patterns = {"ONON", "ONONON", "ONC", "ONONC", "ONCON"};
        n.min_letters = 3;
        n.max_letters = 10;
        n.forbidden = {"eee", "iee", "yy", "ouou", "shsh"};
        n.god_forms = {"{root}"};
        n.civilization_forms = {"The {root} Ring", "The Circle of {root}", "The {root} Court"};
        n.settlement_forms = {"{root}", "{root} Glade", "{root} Hollow", "{root}"};
    } else if (id == "goblin") {
        n.onsets = {"g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"};
        n.nuclei = {"a", "i", "u", "o", "ee"};
        n.codas = {"k", "g", "b", "z", "t", "x", "nk", "rk", "gg", "zz", "tch"};
        n.patterns = {"ONC", "ONC", "ONCONC", "ONC-ONC", "ONONC"};
        n.min_letters = 3;
        n.max_letters = 10;
        n.forbidden = {"eee", "ggg", "zzz", "xg", "xk"};
        n.god_forms = {"{root}"};
        n.civilization_forms = {"The {root} Horde", "The Warrens of {root}", "The {root} Mob"};
        n.settlement_forms = {"{root}", "{root}", "{root} Pit", "{root} Hole"};
    } else if (id == "human") {
        n.onsets = {"b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"};
        n.nuclei = {"a", "e", "i", "o", "u", "ea"};
        n.codas = {"n", "d", "r", "s", "l", "rd", "ck", "ld", "m", "nt"};
        n.patterns = {"ONC", "ONC", "ONCONC", "ONC"};
        n.min_letters = 4;
        n.max_letters = 9;
        n.forbidden = {"eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm"};
        n.god_forms = {"{root}"};
        n.civilization_forms = {"The Kingdom of {root}", "The {root} League", "The Duchy of {root}"};
        n.settlement_forms = {"{root}", "{root}ford", "{root}ton", "{root}bury", "{root}wick", "{root}ham", "{root}stead"};
    } else if (id == "orcish") {
        n.onsets = {"gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"};
        n.nuclei = {"a", "u", "o", "aa", "uu"};
        n.codas = {"g", "sh", "k", "rz", "z", "gh", "rk", "l", "m", "", "kh", "ld"};
        n.patterns = {"ONC", "ONC", "ONC'ONC", "ONCONC", "NC'ONC"};
        n.min_letters = 3;
        n.max_letters = 10;
        n.forbidden = {"aaa", "uuu", "ghgh", "shsh"};
        n.god_forms = {"{root}"};
        n.civilization_forms = {"The {root} Horde", "Clan {root}", "The Warband of {root}"};
        n.settlement_forms = {"{root}", "{root} Camp", "{root}", "{root} Fort"};
    } else if (id == "tiny") {
        // Exactly two valid names of every kind: "Ba" and "Da".
        n.onsets = {"b", "d"};
        n.nuclei = {"a"};
        n.codas = {""};
        n.patterns = {"ON"};
        n.min_letters = 2;
        n.max_letters = 2;
        n.god_forms = {"{root}"};
        n.civilization_forms = {"{root}"};
        n.settlement_forms = {"{root}"};
    } else if (id == "never") {
        // A one-letter root never reaches min_letters 30.
        n.onsets = {"b"};
        n.nuclei = {"a"};
        n.codas = {""};
        n.patterns = {"N"};
        n.min_letters = 30;
        n.max_letters = 32;
        n.god_forms = {"{root}"};
        n.civilization_forms = {"{root}"};
        n.settlement_forms = {"{root}"};
    } else {
        throw std::runtime_error("unknown culture " + std::string(id));
    }
    return n;
}

// --- Independent checks of section 4.2 and invariant 1 -----------------------------------------------

inline bool is_sep(char c) { return c == '\'' || c == '-'; }

inline std::string cap(std::string_view root) {
    std::string out(root);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const char c = out[i];
        if (c >= 'a' && c <= 'z' && (i == 0 || out[i - 1] == '-')) {
            out[i] = static_cast<char>(c - 'a' + 'A');
        }
    }
    return out;
}

inline bool root_valid(const Naming& n, std::string_view root) {
    std::int64_t letters = 0;
    for (const char c : root) {
        if (c >= 'a' && c <= 'z') ++letters;
    }
    if (letters < n.min_letters || letters > n.max_letters) return false;
    for (const std::string& f : n.forbidden) {
        if (root.find(f) != std::string_view::npos) return false;
    }
    if (!root.empty() && (is_sep(root.front()) || is_sep(root.back()))) return false;
    for (std::size_t i = 0; i + 1 < root.size(); ++i) {
        if (is_sep(root[i]) && is_sep(root[i + 1])) return false;
    }
    return true;
}

inline std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// Roman numerals by the rule of P1-06 RG11 4, written again here.
inline std::string roman_ref(std::uint64_t k) {
    static constexpr std::array<std::pair<std::uint64_t, std::string_view>, 13> R = {{{1000, "M"}, {900, "CM"},
        {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"}, {10, "X"}, {9, "IX"}, {5, "V"},
        {4, "IV"}, {1, "I"}}};
    std::string out;
    for (const auto& [v, s] : R) {
        while (k >= v) {
            out += s;
            k -= v;
        }
    }
    return out;
}

// Splits a trailing " <numeral>" (numeral >= II, at most 3999 checked) off a name.
inline bool split_numeral(std::string_view name, std::string_view& head, std::uint64_t& k) {
    const std::size_t sp = name.rfind(' ');
    if (sp == std::string_view::npos) return false;
    static const std::map<std::string, std::uint64_t, std::less<>> numerals = [] {
        std::map<std::string, std::uint64_t, std::less<>> m;
        for (std::uint64_t i = 2; i < 4000; ++i) m.emplace(roman_ref(i), i);
        return m;
    }();
    const auto it = numerals.find(name.substr(sp + 1));
    if (it == numerals.end()) return false;
    head = name.substr(0, sp);
    k = it->second;
    return true;
}

inline bool charset_ok(std::string_view name) {
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ' || is_sep(c);
        if (!ok) return false;
    }
    return true;
}

// Invariant 1: ASCII letters, spaces, ' and -; at most 64 bytes; the form's
// fixed text plus a root within the culture's bounds, optionally followed
// by a space and a Roman numeral >= II.
inline bool shape_ok(const Naming& n, Kind k, std::string_view name, bool* numbered = nullptr) {
    if (name.size() > 64 || !charset_ok(name)) return false;
    std::vector<std::pair<std::string_view, bool>> cands = {{name, false}};
    std::string_view head;
    std::uint64_t num = 0;
    if (split_numeral(name, head, num)) cands.emplace_back(head, true);
    for (const auto& [cand, has_num] : cands) {
        if (!has_num && cand.size() > 48) continue;
        for (const std::string& form : forms_of(n, k)) {
            const std::size_t at = form.find("{root}");
            const std::string_view pre = std::string_view(form).substr(0, at);
            const std::string_view post = std::string_view(form).substr(at + 6);
            if (cand.size() < pre.size() + post.size() + 1) continue;
            if (cand.substr(0, pre.size()) != pre || cand.substr(cand.size() - post.size()) != post) continue;
            const std::string_view root = cand.substr(pre.size(), cand.size() - pre.size() - post.size());
            const std::string lo = lower(root);
            if (cap(lo) == root && root_valid(n, lo)) {
                if (numbered != nullptr) *numbered = has_num;
                return true;
            }
        }
    }
    return false;
}

// --- Golden file ---------------------------------------------------------------------------------

inline std::vector<std::string> split(std::string_view s, std::size_t max_fields) {
    // Splits on single spaces into at most max_fields tokens; the last takes the rest.
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (out.size() + 1 < max_fields) {
        const std::size_t sp = s.find(' ', pos);
        if (sp == std::string_view::npos) break;
        out.emplace_back(s.substr(pos, sp - pos));
        pos = sp + 1;
    }
    out.emplace_back(s.substr(pos));
    return out;
}

inline std::uint64_t hex(const std::string& s) { return std::stoull(s, nullptr, 16); }
inline std::uint64_t dec_u(const std::string& s) { return std::stoull(s, nullptr, 10); }
inline std::int64_t dec_i(const std::string& s) { return std::stoll(s, nullptr, 10); }

struct Golden {
    std::vector<std::string> lines;  // every non-comment line, in file order
    // lines whose first token is the key ("draw", "pick", ...); name lines are under "name".
    std::map<std::string, std::vector<std::string>> by_kind;
};

inline const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        std::ifstream in(UNMAPPED_P3_02_GOLDEN_PATH, std::ios::binary);
        if (!in) throw std::runtime_error("cannot open " + std::string(UNMAPPED_P3_02_GOLDEN_PATH));
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') throw std::runtime_error("golden file has CR");
            if (line.empty() || line[0] == '#') continue;
            out.lines.push_back(line);
            const std::string key = (line[0] >= '0' && line[0] <= '9') ? "name" : line.substr(0, line.find(' '));
            out.by_kind[key].push_back(line);
        }
        return out;
    }();
    return g;
}

inline const std::vector<std::string>& golden_lines(const char* key) {
    static const std::vector<std::string> empty;
    const auto it = golden().by_kind.find(key);
    return it == golden().by_kind.end() ? empty : it->second;
}

}  // namespace names_test
