#pragma once

// Support for the P5-12 tests (docs/specs/p5-12-renamed-regions.md r0): the golden file
// worldgen/tests/golden/p5-12-renaming.txt (written by reference/p5_12_renaming_reference.py), the shipped
// rules as test-owned data, the invariants of section 8 from the rows, and the fixtures of 13 (rename-small,
// off) as edited content. Written from the specs before the implementation (test-writer rule); every library
// name used here is older than P5-12 except the table region_name (rows_of) and the event kind 43.

#include "p5_02_monsters_content.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "p3_06_conflict_support.hpp"

namespace renaming_test {

using namespace conflict_test;

inline constexpr std::int64_t k_region_renamed = 43;   // P3-04 r21 23.2 (provisional id, P5-00 4.4)

struct RRule {
    std::string key;
    std::vector<std::string> kinds;
    std::int64_t weight;
};

// P5-12 4.1: the keys, kinds and weights the invariants read.
inline const std::vector<RRule>& shipped_rule_kinds() {
    static const std::vector<RRule> v = {{"conquest", {"civ_conquered"}, 100}, {"capital_sack", {"sacked"}, 90},
                                         {"great_sack", {"sacked"}, 80},
                                         {"slaughter", {"battle", "battle_at_pass", "allied_battle"}, 70},   // P5-04 11.1
                                         {"plague", {"plague_outbreak"}, 60}, {"heartland", {"became_city"}, 50},
                                         {"ravage", {"settlement_overrun"}, 75}};   // P5-02 9.3: rule 7
    return v;
}

struct Tuning {
    std::int64_t region_max = 2, region_gap_years = 300, world_max_permille = 150, century_max_permille = 50,
                 century_years = 100;
};

inline Tuning tuning_of(const wd::ContentSet& c) {
    Tuning t;
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != "renaming") continue;
            t.region_max = d.record.get_int("region_max");
            t.region_gap_years = d.record.get_int("region_gap_years");
            t.world_max_permille = d.record.get_int("world_max_permille");
            t.century_max_permille = d.record.get_int("century_max_permille");
            t.century_years = d.record.get_int("century_years");
        }
    }
    return t;
}

// history.renaming with fields replaced (past the loader); `rules` edits apply to every rule.
inline wd::ContentSet with_renaming(const std::map<std::string, wd::Value>& edits,
                                    const std::map<std::string, wd::Value>& rule_edits,
                                    const wd::ContentSet& base = shipped()) {
    for (const wd::KindContent& k : base.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != "renaming") continue;
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const std::string& n = d.record.name_at(i);
                if (n == "rules") {
                    std::vector<wd::Value> rules;
                    for (const wd::Value& x : d.record.value_at(i).as_array()) {
                        wd::Record rr;
                        const wd::Record& src = x.as_record();
                        for (std::size_t f = 0; f < src.size(); ++f) {
                            const auto it = rule_edits.find(src.name_at(f));
                            rr.add(src.name_at(f), it != rule_edits.end() ? it->second : src.value_at(f));
                        }
                        rules.push_back(wd::Value::record(std::move(rr)));
                    }
                    r.add(n, wd::Value::array(std::move(rules)));
                    continue;
                }
                const auto it = edits.find(n);
                r.add(n, it != edits.end() ? it->second : d.record.value_at(i));
            }
            return with_history_record(base, "renaming", std::move(r));
        }
    }
    FAIL("no history.renaming in the content");
    return base;
}

// 13 fixture rename-small (the reference's rename_small_data): the stress content with every rule's
// min_magnitude 1 and chance_permille 1000 and the two caps opened (the reference's reading).
inline const wd::ContentSet& rename_small_content() {
    static const wd::ContentSet c = with_renaming(
        {{"world_max_permille", wd::Value::integer(1000)}, {"century_max_permille", wd::Value::integer(1000)}},
        {{"min_magnitude", wd::Value::integer(1)}, {"chance_permille", wd::Value::integer(1000)}},
        monsters_content::monsters_small_content());   // P5-02 (0.23.0): the monsters-small edits, so that rule 7 renames
    return c;
}
// 13: region_max 0 (the subsystem off), on the shipped content.
inline const wd::ContentSet& off_content() {
    static const wd::ContentSet c = with_renaming({{"region_max", wd::Value::integer(0)}}, {});
    return c;
}

// --- The golden file -------------------------------------------------------------------------------

struct RenameRow {
    std::int64_t region, from_year;
    std::string rule;
    std::int64_t culture, event, cause;
    std::string name;
};

struct RCase {
    std::string kind, name, data;   // kind world or fixture; data rename-small, off
    std::uint64_t seed = 0;
    std::int64_t size = 192, span = 0;
    std::string races, detail = "normal", civs = "auto", intensity = "normal";
    std::int64_t rows = 0;
    std::string hash, stats;
    std::vector<RenameRow> renames;
};

struct RGolden {
    std::vector<RCase> cases;
    std::vector<std::string> rules_seen;
};

inline std::vector<std::string> words(const std::string& s) { return split(s, ' '); }

inline const RGolden& renaming_golden() {
    static const RGolden g = [] {
        RGolden out;
        std::ifstream f(UNMAPPED_P5_12_GOLDEN_PATH, std::ios::binary);
        REQUIRE_MESSAGE(f.good(), "cannot read " << UNMAPPED_P5_12_GOLDEN_PATH);
        std::map<std::string, std::size_t> idx;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            const std::vector<std::string> w = words(line);
            if (w[0] == "world" || w[0] == "fixture") {
                RCase c;
                c.kind = w[0];
                c.name = w[1];
                c.seed = std::stoull(w[2]);
                if (w[0] == "world") {
                    c.size = std::stoll(w[3]);
                    c.span = std::stoll(w[4]);
                    c.races = w[5];
                    c.detail = w[6];
                    c.civs = w[7];
                    c.intensity = w[8];
                } else {
                    c.span = std::stoll(w[3]);
                    c.races = w[4];
                    c.civs = w[5];
                    c.intensity = w[6];
                    c.data = w[7];
                }
                idx[c.name] = out.cases.size();
                out.cases.push_back(c);
                continue;
            }
            if (w[0] == "rules_seen") {
                out.rules_seen = split(w[1], ',');
                continue;
            }
            const auto it = idx.find(w[0]);
            REQUIRE_MESSAGE(it != idx.end(), "golden line of no case: " << line);
            RCase& c = out.cases[it->second];
            if (w[1] == "region_name") {
                c.rows = std::stoll(w[2]);
                c.hash = w[3];
            } else if (w[1] == "rename") {
                // <case> rename <region> <from_year> <rule> culture <c> e<event> cause e<cause> <name ...>
                RenameRow r{std::stoll(w[2]), std::stoll(w[3]), w[4], std::stoll(w[6]), std::stoll(w[7].substr(1)),
                            std::stoll(w[9].substr(1)), {}};
                std::size_t at = 0;
                for (int k = 0; k < 10; ++k) at = line.find(' ', at) + 1;
                r.name = line.substr(at);
                c.renames.push_back(r);
            } else if (w[1] == "rename_stats") {
                c.stats = line.substr(w[0].size() + 1 + w[1].size() + 1);
            } else {
                FAIL("unknown golden item: " << line);
            }
        }
        return out;
    }();
    return g;
}

inline const RCase& renaming_case(const char* name) {
    for (const RCase& c : renaming_golden().cases) {
        if (c.name == name) return c;
    }
    FAIL("no renaming golden case " << name);
    return renaming_golden().cases.front();
}

// --- Invariants of section 8 from the rows (1 to 5; 6 needs the --until regions world) -------------

inline std::vector<std::string> renaming_violations(const Tables& t, const Tuning& tu, std::int64_t regions,
                                                    const std::set<std::string>& other_names,
                                                    const std::map<std::int64_t, std::string>& geo_names) {
    std::vector<std::string> out;
    std::map<std::int64_t, std::string> key_of;
    for (const Row& k : t.event_kind) key_of[at(k, 0)] = k[1].bytes;
    std::map<std::int64_t, const Row*> ev;
    for (const Row& e : t.event) ev[at(e, 0)] = &e;
    std::map<std::int64_t, std::vector<std::int64_t>> causes;
    for (const Row& c : t.event_cause) causes[at(c, 0)].push_back(at(c, 1));
    std::map<std::string, std::int64_t> weight_of_kind;   // the largest weight of the rules reading a kind
    for (const RRule& r : shipped_rule_kinds()) {
        for (const std::string& k : r.kinds) weight_of_kind[k] = std::max(weight_of_kind[k], r.weight);
    }
    std::map<std::int64_t, const Row*> row_of_event;
    for (const Row& r : t.region_name) {
        const auto e = ev.find(at(r, 4));
        if (e == ev.end() || key_of[at(*e->second, 3)] != "region_renamed" || at(*e->second, 5) != at(r, 0) ||
            at(*e->second, 1) != at(r, 1)) {
            out.push_back("1: row (" + std::to_string(at(r, 0)) + ", " + std::to_string(at(r, 1)) +
                          ") has no region_renamed of its region and year");
        }
        if (!row_of_event.emplace(at(r, 4), &r).second) out.push_back("1: two rows share an event");
    }
    for (const Row& e : t.event) {
        if (key_of[at(e, 3)] != "region_renamed") continue;
        if (row_of_event.find(at(e, 0)) == row_of_event.end()) {
            out.push_back("1: event " + std::to_string(at(e, 0)) + " has no row");
        }
        const std::vector<std::int64_t>& cs = causes[at(e, 0)];
        if (cs.size() != 1U) {
            out.push_back("5: event " + std::to_string(at(e, 0)) + " has " + std::to_string(cs.size()) + " causes");
            continue;
        }
        const Row& c = *ev.at(cs[0]);
        if (!(at(c, 0) < at(e, 0) && at(c, 5) == at(e, 5) && weight_of_kind.count(key_of[at(c, 3)]) == 1U)) {
            out.push_back("5: event " + std::to_string(at(e, 0)) + "'s cause is not an earlier rule event there");
        }
    }
    std::map<std::int64_t, std::vector<const Row*>> per;
    for (const Row& r : t.region_name) per[at(r, 0)].push_back(&r);
    auto weight_of_row = [&](const Row& r) -> std::int64_t {
        const std::vector<std::int64_t>& cs = causes[at(r, 4)];
        return cs.empty() ? -1 : weight_of_kind[key_of[at(*ev.at(cs[0]), 3)]];
    };
    for (const auto& [rid, rs] : per) {
        if (static_cast<std::int64_t>(rs.size()) > tu.region_max) {
            out.push_back("2: region " + std::to_string(rid) + " above region_max");
        }
        for (std::size_t i = 1; i < rs.size(); ++i) {
            if (at(*rs[i], 1) - at(*rs[i - 1], 1) < tu.region_gap_years) {
                out.push_back("2: region " + std::to_string(rid) + " renamed within the gap");
            }
            if (!(weight_of_row(*rs[i]) > weight_of_row(*rs[i - 1]))) {
                out.push_back("2: region " + std::to_string(rid) + "'s later renaming is not weightier");
            }
        }
    }
    const std::int64_t wmax = std::max<std::int64_t>(1, regions * tu.world_max_permille / 1000);
    const std::int64_t cmax = std::max<std::int64_t>(1, regions * tu.century_max_permille / 1000);
    if (static_cast<std::int64_t>(t.region_name.size()) > wmax) out.push_back("3: above the world cap");
    std::vector<std::int64_t> years;
    for (const Row& r : t.region_name) years.push_back(at(r, 1));
    std::sort(years.begin(), years.end());
    for (const std::int64_t y : years) {
        std::int64_t n = 0;
        for (const std::int64_t z : years) n += (z > y - tu.century_years && z <= y) ? 1 : 0;
        if (n > cmax) {
            out.push_back("3: above the century cap at " + std::to_string(y));
            break;
        }
    }
    std::set<std::string> seen;
    for (const Row& r : t.region_name) {
        const std::string& n = r[2].bytes;
        if (!seen.insert(n).second) out.push_back("4: " + n + " repeats");
        if (other_names.count(n) == 1U) out.push_back("4: " + n + " equals another name of the world");
        for (const auto& [id, g] : geo_names) {
            if (g == n) out.push_back("4: " + n + " equals region " + std::to_string(id) + "'s geographic name");
        }
        if (n.empty() || n.size() > 48U) out.push_back("4: " + n + " has a bad length");
        for (std::size_t i = 0; i < n.size(); ++i) {
            const char ch = n[i];
            const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == ' ' || ch == '\'' || ch == '-';
            if (!ok) out.push_back("4: " + n + " holds a bad byte");
            if (i + 2 < n.size()) {
                auto low = [](char x) { return static_cast<char>(x >= 'A' && x <= 'Z' ? x + 32 : x); };
                if (low(n[i]) == low(n[i + 1]) && low(n[i]) == low(n[i + 2]) && low(n[i]) >= 'a' && low(n[i]) <= 'z') {
                    out.push_back("4: " + n + " has three equal letters");
                }
            }
        }
    }
    return out;
}

// The other names of a stored world (ages, gods, civilizations, settlements) and its geographic region names.
inline std::set<std::string> other_names_of(const db::WorldReader& r, const Tables& t) {
    std::set<std::string> s;
    for (const Row& a : rows_of(r, "age")) s.insert(a[4].bytes);
    for (const Row& g : rows_of(r, "god")) s.insert(g[1].bytes);
    for (const Row& c : t.civilization) s.insert(c[1].bytes);
    for (const Row& x : t.settlement) s.insert(x[1].bytes);
    return s;
}
inline std::map<std::int64_t, std::string> geo_names_of(const db::WorldReader& r) {
    std::map<std::int64_t, std::string> m;
    for (const Row& x : rows_of(r, "region")) m[at(x, 0)] = x[1].bytes;
    return m;
}

}  // namespace renaming_test
