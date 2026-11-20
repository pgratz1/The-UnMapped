#pragma once

// The test's own RG11 r3 (spec P1-06 r3 and r4, generator 0.17.0: region
// names drawn from the lists of the data kind `placename`, no numerals),
// written from the spec text alone, independently of the stage. Used by the
// test's own stage (own_regions in p1_06_regions_support.hpp), by the unit
// tests of 10.5 (against values written in the tests) and by the property
// tests of invariant 6.
//
// The placename files are read here with toml++ directly, not through the
// production loader, so that this code does not depend on the kind being
// registered. Draws use the library core (stream_key, substream, hash_at3:
// P0-01 7.3, tested against core_vectors.txt), as 10.5 allows.

#include <doctest/doctest.h>

#include <toml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/hash.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace region_names {

// The eight word lists, in the order of 4.1's fields and of record
// name_pools.
inline constexpr std::array<std::string_view, 8> LISTS = {"nouns",      "suffixes",   "prefixes", "adjectives",
                                                          "features",   "possessors", "places",   "objects"};
// The slot of each list, same order.
inline constexpr std::array<std::string_view, 8> SLOTS = {"noun",    "suffix",    "prefix", "adjective",
                                                          "feature", "possessor", "place",  "object"};
// The trait order of RG11 5 (the order of the `trait` enumeration of 4.1).
inline constexpr std::array<std::string_view, 8> TRAITS = {"coast", "river", "lake",   "high",
                                                           "ore",   "pass",  "barren", "fertile"};

inline std::size_t list_of_slot(std::string_view slot) {
    for (std::size_t i = 0; i < SLOTS.size(); ++i) {
        if (SLOTS[i] == slot) {
            return i;
        }
    }
    return SLOTS.size();
}

struct Form {
    std::string form;
    std::int64_t weight = 1;
};

struct PlaceDef {
    std::string id, role, special = "none", trait = "none";
    std::vector<std::string> biomes;
    std::vector<Form> forms;
    std::array<std::vector<std::string>, 8> lists;
    std::vector<std::string> forbidden, excluded;
};

struct PlaceData {
    std::vector<PlaceDef> defs;  // ascending id
    [[nodiscard]] const PlaceDef& common() const {
        for (const PlaceDef& d : defs) {
            if (d.role == "common") {
                return d;
            }
        }
        FAIL("no common placename definition");
        return defs.front();
    }
    [[nodiscard]] const PlaceDef& by_id(std::string_view id) const {
        for (const PlaceDef& d : defs) {
            if (d.id == id) {
                return d;
            }
        }
        FAIL("no placename definition " << std::string(id));
        return defs.front();
    }
    [[nodiscard]] const PlaceDef* special(std::string_view s) const {
        for (const PlaceDef& d : defs) {
            if (d.role == "land" && d.special == s) {
                return &d;
            }
        }
        return nullptr;
    }
    [[nodiscard]] const PlaceDef* land_of_biome(std::string_view key) const {
        for (const PlaceDef& d : defs) {
            if (d.role == "land") {
                for (const std::string& b : d.biomes) {
                    if (b == key) {
                        return &d;
                    }
                }
            }
        }
        return nullptr;
    }
    [[nodiscard]] const PlaceDef* trait(std::string_view t) const {
        for (const PlaceDef& d : defs) {
            if (d.role == "trait" && d.trait == t) {
                return &d;
            }
        }
        return nullptr;
    }
};

// The tuning records name_pools and name_traits of P1-06 4 (r3), shipped.
struct NameTuning {
    std::array<std::array<std::int64_t, 3>, 8> pools = {{{0, 1, 0}, {0, 4, 1}, {6, 3, 2}, {6, 3, 2},
                                                         {5, 1, 2}, {8, 1, 1}, {8, 1, 1}, {6, 3, 2}}};
    std::int64_t coast_cells = 128, river_class = 7, lake_permille = 125, high_mean_m = 400, ore_rich_cells = 1000,
                 barren_hab_max = 20, fertile_min = 200;
};

inline std::vector<std::string> strings_of(const toml::table& t, std::string_view key) {
    std::vector<std::string> out;
    if (const toml::array* a = t[key].as_array()) {
        for (const auto& e : *a) {
            out.push_back(std::string(e.value_or(std::string_view{})));
        }
    }
    return out;
}

inline void add_definitions(PlaceData& data, std::string_view text) {
    const toml::table doc = toml::parse(text);
    const toml::table* kind = doc["placename"].as_table();
    REQUIRE(kind != nullptr);
    for (const auto& [key, node] : *kind) {
        const toml::table* t = node.as_table();
        REQUIRE(t != nullptr);
        PlaceDef d;
        d.id = std::string(key.str());
        d.role = std::string((*t)["role"].value_or(std::string_view{}));
        d.special = std::string((*t)["special"].value_or(std::string_view{"none"}));
        d.trait = std::string((*t)["trait"].value_or(std::string_view{"none"}));
        d.biomes = strings_of(*t, "biomes");
        if (const toml::array* fa = (*t)["forms"].as_array()) {
            for (const auto& e : *fa) {
                const toml::table* ft = e.as_table();
                REQUIRE(ft != nullptr);
                d.forms.push_back(Form{std::string((*ft)["form"].value_or(std::string_view{})),
                                       (*ft)["weight"].value_or(std::int64_t{0})});
            }
        }
        for (std::size_t i = 0; i < LISTS.size(); ++i) {
            d.lists[i] = strings_of(*t, LISTS[i]);
        }
        d.forbidden = strings_of(*t, "forbidden");
        d.excluded = strings_of(*t, "excluded");
        data.defs.push_back(std::move(d));
    }
}

inline std::string file_text(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Every *.toml of a placenames directory, definitions in ascending id.
inline PlaceData read_dir(const std::filesystem::path& dir) {
    PlaceData data;
    std::vector<std::filesystem::path> files;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() == ".toml") {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        add_definitions(data, file_text(f));
    }
    std::sort(data.defs.begin(), data.defs.end(), [](const PlaceDef& a, const PlaceDef& b) { return a.id < b.id; });
    return data;
}

inline std::filesystem::path shipped_dir() {
    return std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / "game" / "data" / "placenames";
}

inline const PlaceData& shipped() {
    static const PlaceData d = read_dir(shipped_dir());
    return d;
}

// --- Forms --------------------------------------------------------------------------------------

struct Token {
    bool slot = false;
    std::string text;  // the literal, or the slot name
};

// The place_form grammar of 4.1; nullopt when the form is not of it.
inline std::optional<std::vector<Token>> parse_form(std::string_view f) {
    if (f.empty() || f.front() == ' ' || f.back() == ' ' || f.find("  ") != std::string_view::npos) {
        return std::nullopt;
    }
    std::vector<Token> out;
    std::set<std::string> seen;
    std::string lit;
    for (std::size_t i = 0; i < f.size();) {
        const char c = f[i];
        if (c == '{') {
            const std::size_t j = f.find('}', i);
            if (j == std::string_view::npos) {
                return std::nullopt;
            }
            const std::string s(f.substr(i + 1, j - i - 1));
            if (list_of_slot(s) == SLOTS.size() || !seen.insert(s).second) {
                return std::nullopt;
            }
            if (!lit.empty()) {
                out.push_back(Token{false, lit});
                lit.clear();
            }
            out.push_back(Token{true, s});
            i = j + 1;
            continue;
        }
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ' ' || c == '\'';
        if (!ok) {
            return std::nullopt;
        }
        lit += c;
        ++i;
    }
    if (!lit.empty()) {
        out.push_back(Token{false, lit});
    }
    if (seen.empty()) {
        return std::nullopt;
    }
    return out;
}

inline std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

inline std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
        if (c == ' ') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

inline std::string fill(const std::vector<Token>& toks, const std::vector<std::string>& values) {
    std::string s;
    std::size_t k = 0;
    for (const Token& t : toks) {
        s += t.slot ? values.at(k++) : t.text;
    }
    return s;
}

inline bool vowel(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

// RG11 7: "" when valid, else the first rule that fails ("V1" ... "V6").
inline std::string invalid_rule(const std::vector<Token>& toks, const std::vector<std::string>& values,
                                const PlaceDef& common) {
    std::size_t k = 0;
    std::vector<std::pair<bool, std::string>> parts;
    for (const Token& t : toks) {
        parts.emplace_back(t.slot, t.slot ? values.at(k++) : t.text);
    }
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        if (parts[i].first && parts[i + 1].first) {
            const std::string x = lower(parts[i].second), y = lower(parts[i + 1].second);
            if (x.back() == y.front() || (vowel(x.back()) && vowel(y.front())) || x.size() + y.size() > 12) {
                return "V1";
            }
        }
    }
    const std::string name = fill(toks, values);
    const std::string low = lower(name);
    for (std::size_t i = 0; i + 2 < low.size(); ++i) {
        if (low[i] >= 'a' && low[i] <= 'z' && low[i] == low[i + 1] && low[i] == low[i + 2]) {
            return "V2";
        }
    }
    std::vector<std::string> sv;
    for (const auto& p : parts) {
        if (p.first) {
            sv.push_back(lower(p.second));
        }
    }
    for (std::size_t i = 0; i < sv.size(); ++i) {
        for (std::size_t j = 0; j < sv.size(); ++j) {
            if (i != j && sv[j].find(sv[i]) != std::string::npos) {
                return "V3";
            }
        }
    }
    for (const std::string& w : words(low)) {
        for (const std::string& f : common.forbidden) {
            if (w.find(f) != std::string::npos) {
                return "V4";
            }
        }
    }
    for (const std::string& e : common.excluded) {
        if (e == name) {
            return "V5";
        }
        if (e.find(' ') == std::string::npos) {
            for (const std::string& w : words(name)) {
                if (w == e) {
                    return "V5";
                }
            }
        }
    }
    if (name.size() > 48) {
        return "V6";
    }
    return "";
}

// --- Draws --------------------------------------------------------------------------------------

inline std::uint64_t k_name(std::uint64_t seed, std::uint64_t n) {
    return worldgen::substream(worldgen::stream_key(seed, "geo.regions.name"), n);
}

// P3-02 4.1.
inline std::uint64_t pick(std::uint64_t u, std::uint64_t n) { return ((u >> 32U) * n) >> 32U; }

// The weights here are below 2^31 in total (forms at most 16 x 1000, pools
// at most 3 x 1000), so every product fits in 64 bits.
inline std::size_t weighted(std::uint64_t u, const std::vector<std::int64_t>& w) {
    std::uint64_t total = 0;
    for (const std::int64_t x : w) {
        REQUIRE(x >= 0);
        total += static_cast<std::uint64_t>(x);
    }
    REQUIRE(total > 0);
    REQUIRE(total < (std::uint64_t{1} << 31U));
    const std::uint64_t hi = (u >> 32U) * total;
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < w.size(); ++i) {
        acc += static_cast<std::uint64_t>(w[i]);
        if (hi < (acc << 32U)) {
            return i;
        }
    }
    FAIL("weighted");
    return 0;
}

// The pools of one list for a region: common, land, the traits in trait order.
struct Pools {
    std::array<std::vector<std::string>, 3> p;
    std::vector<std::int64_t> w;
};

inline Pools pools_of(std::size_t list, const PlaceDef& land, const PlaceDef& common,
                      const std::vector<const PlaceDef*>& traits, const NameTuning& t) {
    Pools out;
    out.p[0] = common.lists[list];
    out.p[1] = land.lists[list];
    for (const PlaceDef* d : traits) {
        out.p[2].insert(out.p[2].end(), d->lists[list].begin(), d->lists[list].end());
    }
    for (std::size_t i = 0; i < 3; ++i) {
        out.w.push_back(out.p[i].empty() ? 0 : t.pools[list][i]);
    }
    return out;
}

struct Step {
    std::string slot;
    std::uint64_t u_pool = 0, u_index = 0;
    std::vector<std::int64_t> weights;
    std::size_t pool = 0, size = 0;
    std::uint64_t index = 0;
    std::string word;
};

struct Attempt {
    std::uint64_t u0 = 0;
    std::size_t form = 0;
    std::vector<Step> steps;
    std::string name, rule;  // rule "" = valid
    [[nodiscard]] bool valid() const { return rule.empty(); }
};

inline Attempt attempt(std::uint64_t key, std::int64_t region, std::int64_t t, const PlaceDef& land,
                       const PlaceDef& common, const std::vector<const PlaceDef*>& traits, const NameTuning& tu) {
    Attempt a;
    auto u = [&](std::int64_t k) { return worldgen::hash_at3(key, region, t, k); };
    std::vector<std::int64_t> fw;
    for (const Form& f : land.forms) {
        fw.push_back(f.weight);
    }
    a.u0 = u(0);
    a.form = weighted(a.u0, fw);
    const auto toks = parse_form(land.forms[a.form].form);
    REQUIRE(toks.has_value());
    std::int64_t k = 1;
    std::vector<std::string> values;
    for (const Token& tok : *toks) {
        if (!tok.slot) {
            continue;
        }
        const Pools P = pools_of(list_of_slot(tok.text), land, common, traits, tu);
        Step s;
        s.slot = tok.text;
        s.u_pool = u(k);
        s.u_index = u(k + 1);
        s.weights = P.w;
        s.pool = weighted(s.u_pool, P.w);
        s.size = P.p[s.pool].size();
        s.index = pick(s.u_index, s.size);
        s.word = P.p[s.pool][s.index];
        values.push_back(s.word);
        a.steps.push_back(s);
        k += 2;
    }
    a.name = fill(*toks, values);
    a.rule = invalid_rule(*toks, values, common);
    return a;
}

inline std::optional<std::string> fallback(const PlaceDef& land, const PlaceDef& common,
                                           const std::set<std::string>& used) {
    const std::vector<Token> toks = {Token{true, "prefix"}, Token{true, "suffix"}};
    std::vector<std::string> P = common.lists[2];
    P.insert(P.end(), land.lists[2].begin(), land.lists[2].end());
    std::vector<std::string> S = land.lists[1];
    S.insert(S.end(), common.lists[1].begin(), common.lists[1].end());
    for (const std::string& p : P) {
        for (const std::string& s : S) {
            const std::string n = p + s;
            if (invalid_rule(toks, {p, s}, common).empty() && used.count(n) == 0U) {
                return n;
            }
        }
    }
    return std::nullopt;
}

// --- Traits (RG11 5) ----------------------------------------------------------------------------

struct TraitRow {
    std::int64_t coast_cells = 0, river_class_max = 0, lake_cells = 0, cells = 1, elev_mean_dm = 0,
                 ore_rich_cells = 0, habitability_mean = 255, fertility_mean = 0;
    bool pass = false;
};

inline std::vector<std::string> traits_of(const TraitRow& r, const NameTuning& t) {
    const std::array<bool, 8> held = {
        r.coast_cells >= t.coast_cells,
        r.river_class_max >= t.river_class,
        r.lake_cells > 0 && 1000 * r.lake_cells >= t.lake_permille * r.cells,
        r.elev_mean_dm >= 10 * t.high_mean_m,
        r.ore_rich_cells >= t.ore_rich_cells,
        r.pass,
        r.habitability_mean <= t.barren_hab_max,
        r.fertility_mean >= t.fertile_min,
    };
    std::vector<std::string> out;
    for (std::size_t i = 0; i < TRAITS.size(); ++i) {
        if (held[i]) {
            out.emplace_back(TRAITS[i]);
        }
    }
    return out;
}

inline std::vector<const PlaceDef*> trait_defs(const PlaceData& d, const std::vector<std::string>& traits) {
    std::vector<const PlaceDef*> out;
    for (const std::string& t : traits) {
        if (const PlaceDef* p = d.trait(t)) {
            out.push_back(p);
        }
    }
    return out;
}

// --- RG11 4 and 8 -------------------------------------------------------------------------------

struct NamedRegion {
    std::int64_t id = 0, cells = 0;
    std::string base;
    const PlaceDef* land = nullptr;
    TraitRow traits;
};

struct Named {
    std::vector<std::string> names;
    std::vector<std::string> how;  // "K", "F" or the attempt number
};

// Throws (FAIL) where the stage is fatal; returns nullopt on "no free name".
inline std::optional<Named> name_regions(const std::vector<NamedRegion>& regions, std::uint64_t seed, std::uint64_t n,
                                         const PlaceData& data, const NameTuning& t) {
    const std::uint64_t key = k_name(seed, n);
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> best;  // base -> (cells, -id)
    for (const NamedRegion& r : regions) {
        auto it = best.find(r.base);
        if (it == best.end() || std::make_pair(r.cells, -r.id) > it->second) {
            best[r.base] = std::make_pair(r.cells, -r.id);
        }
    }
    std::set<std::string> used;
    for (const auto& [b, v] : best) {
        used.insert(b);
    }
    Named out;
    const PlaceDef& common = data.common();
    for (const NamedRegion& r : regions) {
        if (best[r.base].second == -r.id) {
            out.names.push_back(r.base);
            out.how.emplace_back("K");
            continue;
        }
        const auto td = trait_defs(data, traits_of(r.traits, t));
        std::optional<std::pair<std::string, std::string>> got;
        for (std::int64_t at = 0; at < 64 && !got; ++at) {
            const Attempt a = attempt(key, r.id, at, *r.land, common, td, t);
            if (a.valid() && used.count(a.name) == 0U) {
                got = std::make_pair(a.name, std::to_string(at));
            }
        }
        if (!got) {
            const auto f = fallback(*r.land, common, used);
            if (!f) {
                return std::nullopt;
            }
            got = std::make_pair(*f, std::string("F"));
        }
        used.insert(got->first);
        out.names.push_back(got->first);
        out.how.push_back(got->second);
    }
    return out;
}

// Invariant 6 (r3) for one drawn name: one of the land's forms filled with
// words of the region's pools of non-zero weight, valid by V1 to V6.
inline bool parses_as_land_name(const std::string& name, const PlaceDef& land, const PlaceDef& common,
                                const std::vector<const PlaceDef*>& traits, const NameTuning& t) {
    for (const Form& f : land.forms) {
        const auto toks = parse_form(f.form);
        if (!toks) {
            continue;
        }
        std::vector<std::vector<std::string>> choices;
        for (const Token& tok : *toks) {
            if (tok.slot) {
                const Pools P = pools_of(list_of_slot(tok.text), land, common, traits, t);
                std::set<std::string> s;
                for (std::size_t i = 0; i < 3; ++i) {
                    if (P.w[i] > 0) {
                        s.insert(P.p[i].begin(), P.p[i].end());
                    }
                }
                choices.emplace_back(s.begin(), s.end());
            }
        }
        // Backtracking over the tokens.
        std::vector<std::string> vals;
        std::function<bool(std::size_t, std::size_t, std::size_t)> go = [&](std::size_t ti, std::size_t pos,
                                                                           std::size_t si) -> bool {
            if (ti == toks->size()) {
                return pos == name.size() && invalid_rule(*toks, vals, common).empty();
            }
            const Token& tok = (*toks)[ti];
            if (!tok.slot) {
                return name.compare(pos, tok.text.size(), tok.text) == 0 && go(ti + 1, pos + tok.text.size(), si);
            }
            for (const std::string& v : choices[si]) {
                if (name.compare(pos, v.size(), v) == 0) {
                    vals.push_back(v);
                    if (go(ti + 1, pos + v.size(), si + 1)) {
                        return true;
                    }
                    vals.pop_back();
                }
            }
            return false;
        };
        if (go(0, 0, 0)) {
            return true;
        }
    }
    return false;
}

}  // namespace region_names
