#pragma once

// Test-owned helpers of the P3-06 conflict tests: the reader of
// worldgen/tests/golden/p3-06-conflict.txt (written by the independent
// reference reference/p3_conflict_reference.py), the stress tuning, the
// [stats] keys of P3-05 15.4 and P3-06 15.4 from rows, the per-war lines of
// the golden file re-derived from rows, the invariants of 14 re-derived from
// rows, and (with the implementation's header) the shipped configuration
// with probe subsystems. Library API of P3-06 only through
// p3_06_conflict_api.hpp.

#include "p3_05_peoples_support.hpp"
#include "p3_06_conflict_api.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef UNMAPPED_P3_06_GOLDEN_PATH
#error "UNMAPPED_P3_06_GOLDEN_PATH must name worldgen/tests/golden/p3-06-conflict.txt"
#endif

namespace conflict_test {

using namespace peoples_test;

// Event kind ids of P3-06 11.
enum CKind : std::int64_t {
    k_claim = 20, k_war_declared, k_battle, k_battle_at_pass, k_siege, k_sacked, k_settlement_captured, k_peace,
    k_truce, k_region_ceded, k_civ_conquered
};
// P3-04 r17 21.2 / P3-06 r19 23.2, 23.3 (generator 0.20.0): kinds 39 and 40. Wherever a rule, invariant or
// statistic reads a claim, kinds 20 and 39 are read alike; a cession of (13), (17) and the companions is 29 or 40.
inline constexpr std::int64_t k_claim_settlements = 39, k_settlements_ceded = 40;
inline bool is_claim(std::int64_t k) { return k == k_claim || k == k_claim_settlements; }
inline bool is_cede(std::int64_t k) { return k == k_region_ceded || k == k_settlements_ceded; }
// P3-04 r19 22.2 / P3-06 r22 24.3, 24.4 (P5-01, generator 0.21.0): kinds 41 and 42; and the peace-like endings.
inline constexpr std::int64_t k_claim_dropped = 41, k_refuge_hidden = 42;

// --- Golden file ------------------------------------------------------------------------------------

struct CCase {
    std::string kind;  // world, synthetic, stress
    std::string name;
    std::uint64_t seed = 0;
    std::int64_t size = 0, span = 0;
    std::string races, detail = "normal", civs = "auto", intensity = "normal";
    std::string input_region;
    std::map<std::string, std::string> tables;
    std::vector<std::string> report;   // after "[history]"
    std::vector<std::string> stats;
    std::vector<std::string> wars;     // "e<w> <start>-<end> c<a> c<b> rg<g> claim e<c> battles n sacked n taken n <how> <outcome>"
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> tension;  // quiet step start -> (value, trigger)
    std::map<std::int64_t, std::string> sentences;
    // P3-09 (generator 0.18.0): P3-07 r8 21.4's notes and 21.3's summary line of the world and synthetic cases
    std::map<std::int64_t, std::string> notes;
    std::string summary;  // "threshold <T> shown <n> ages <n1,...>"
    // P3-10 second round (P3-06 r17 22.5): "quick_aim_wins <n> foothold_cessions <m> held_goal_wars <a>/<b>/<c>/<d>"
    std::string r17;
    // P3-11 (P3-06 r19 23.1-23.4): "t11 <years|-> first_orc_war <years|-> claims39 <a>/<third>/<none> cessions40 <n>
    // goal_differs <n>"
    std::string r19;
    // P5-01 (P3-06 r22 24.1-24.5, P3-07 r19 25.9): "fresh <20>/<39> dropped <n> refuge_hidden <n> repeated_tolls <n>
    // mixed_tolls <n>", "open_claims <e..|->" and "hidden_refuges <s..|->" (the reference's state at E)
    std::string r22, open_claims, hidden_refuges;
};

inline const std::vector<CCase>& conflict_golden() {
    static const std::vector<CCase> g = [] {
        std::vector<CCase> out;
        std::ifstream f(UNMAPPED_P3_06_GOLDEN_PATH, std::ios::binary);
        REQUIRE_MESSAGE(f.good(), "cannot read " << UNMAPPED_P3_06_GOLDEN_PATH);
        std::map<std::string, std::size_t> idx;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::vector<std::string> w = split(line, ' ');
            if (w[0] == "world" || w[0] == "synthetic" || w[0] == "stress" || w[0] == "norefuge") {
                CCase c;
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
                    c.size = 192;
                    c.span = std::stoll(w[3]);
                    c.races = w[4];
                    c.civs = w[5];
                    c.intensity = w[6];
                }
                idx[c.name] = out.size();
                out.push_back(c);
                continue;
            }
            const auto it = idx.find(w[0]);
            REQUIRE_MESSAGE(it != idx.end(), "golden line of no case: " << line);
            CCase& c = out[it->second];
            const std::string rest = line.substr(w[0].size() + 1);
            if (w[1] == "input_region") {
                c.input_region = w[2];
            } else if (w[1] == "table") {
                c.tables[w[2]] = w[3];
            } else if (w[1] == "report") {
                c.report.push_back(rest.substr(7));
            } else if (w[1] == "stats") {
                c.stats.push_back(rest.substr(6));
            } else if (w[1] == "war") {
                c.wars.push_back(rest.substr(4));
            } else if (w[1] == "note") {
                c.notes[std::stoll(w[2].substr(1))] = rest.substr(5 + w[2].size() + 1);
            } else if (w[1] == "summary") {
                c.summary = rest.substr(8);
            } else if (w[1] == "r17") {
                c.r17 = rest.substr(4);  // P3-10 second round (P3-06 r17 22.5): the reference's replay facts
            } else if (w[1] == "r19") {
                c.r19 = rest.substr(4);  // P3-11 (P3-06 r19 23.1-23.4): T11 and the counts of kinds 39 and 40
            } else if (w[1] == "r22") {  // P5-01 (P3-06 r22): the counts and the reference's state at E
                if (w.size() > 2 && w[2] == "open_claims") {
                    c.open_claims = rest.substr(4 + 12);
                } else if (w.size() > 2 && w[2] == "hidden_refuges") {
                    c.hidden_refuges = rest.substr(4 + 15);
                } else {
                    c.r22 = rest.substr(4);
                }
            } else if (w[1].size() > 1 && w[1][0] == 'q') {
                c.tension[std::stoll(w[1].substr(1))] = {std::stoll(w[2]), std::stoll(w[3].substr(1))};
            } else if (w[1].size() > 1 && w[1][0] == 'e') {
                c.sentences[std::stoll(w[1].substr(1))] = rest.substr(w[1].size() + 1);
            } else {
                FAIL("unknown golden item: " << line);
            }
        }
        return out;
    }();
    return g;
}
inline const CCase& conflict_case(const char* name) {
    for (const CCase& c : conflict_golden()) {
        if (c.name == name) {
            return c;
        }
    }
    FAIL("no golden case " << name);
    return conflict_golden().front();
}

// --- Content ----------------------------------------------------------------------------------------

// history.conflict with fields replaced (past the loader).
inline wd::ContentSet with_conflict(const std::map<std::string, wd::Value>& edits, const wd::ContentSet& base = shipped()) {
    for (const wd::KindContent& k : base.kinds) {
        if (k.kind != "history") {
            continue;
        }
        for (const wd::Definition& d : k.definitions) {
            if (d.id != "conflict") {
                continue;
            }
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const auto it = edits.find(d.record.name_at(i));
                r.add(d.record.name_at(i), it != edits.end() ? it->second : d.record.value_at(i));
            }
            return with_history_record(base, "conflict", std::move(r));
        }
    }
    FAIL("no history.conflict in the content");
    return base;
}
inline std::int64_t conflict_int(const wd::ContentSet& c, std::string_view f) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == "conflict") return d.record.get_int(f);
        }
    }
    FAIL("no history.conflict");
    return 0;
}

// P3-06 r17 22.2, P3-01 r13 3.6 (P3-10 second round): the hidden years of a refuge by its founder's race (race
// order, the civilization row's race column): the race field hidden_years; a content set without it (the first
// P3-10 round, 0.18.0) gives history.conflict.hidden_years for every race.
inline std::map<std::int64_t, std::int64_t> race_hidden_years(const wd::ContentSet& c) {
    std::map<std::int64_t, std::int64_t> out;
    const wd::KindContent* race = c.find_kind("race");
    REQUIRE(race != nullptr);
    bool per_race = true;
    for (const wd::Definition& d : race->definitions) {
        bool has = false;
        for (std::size_t i = 0; i < d.record.size(); ++i) has = has || d.record.name_at(i) == "hidden_years";
        per_race = per_race && has;
    }
    for (const wd::Definition& d : race->definitions) {
        out[d.record.get_int("order")] = per_race ? d.record.get_int("hidden_years") : conflict_int(c, "hidden_years");
    }
    return out;
}

// The reference's STRESS_PEOPLES and STRESS_CONFLICT (p3_conflict_reference.py; the golden header).
inline const wd::ContentSet& stress_content() {
    static const wd::ContentSet c = with_conflict(
        {
            {"border_friction", I64(600)},
            {"tick_strain_permille", I64(400)},
            {"quiet_strain_permille", I64(100)},
            {"claim_strain", I64(100)},
            {"declare_min", I64(100)},
            {"declare_span", I64(200)},
            {"declare_permille", I64(600)},
            {"shelter_years", I64(60)},
            {"march_hops", I64(3)},
            {"battle_permille", I64(900)},
            {"garrison_permille", I64(200)},
            {"exhaustion_permille", I64(150)},
            {"war_max_years", I64(25)},
            {"truce_years", I64(40)},
        },
        with_peoples({
            {"found_rate_permille", I64(1000)},
            {"split_min_settlements", I64(2)},
            {"split_min_regions", I64(2)},
            {"split_permille", I64(1000)},
            {"crisis_per_generation_permille", I64(300)},
        }));
    return c;
}

// The guards' counterfactual: the stress tuning with last_refuge 0 and shelter_years 0 (cases norefuge-*).
inline const wd::ContentSet& norefuge_content() {
    static const wd::ContentSet c = with_conflict({{"last_refuge", I64(0)}, {"shelter_years", I64(0)}}, stress_content());
    return c;
}
inline const wd::ContentSet& content_of(const CCase& g) {
    return g.kind == "stress" ? stress_content() : (g.kind == "norefuge" ? norefuge_content() : shipped());
}

inline Settings settings_of(const CCase& c) {
    Settings s;
    s.span = c.span;
    s.races = chrono_test::races_of_text(c.races);
    s.civs = c.civs == "auto" ? std::nullopt : std::optional<std::int64_t>(std::stoll(c.civs));
    s.intensity = c.intensity;
    s.detail = c.detail;
    return s;
}

inline eng::HistoryInputs synthetic_inputs(const CCase& c) {
    return inputs_of(make_synthetic(c.seed), c.seed, settings_of(c), content_of(c));
}

// --- Lines from rows ---------------------------------------------------------------------------------

struct Index {
    std::map<std::int64_t, const Row*> ev, st, cv, ph;
    std::map<std::int64_t, std::vector<std::int64_t>> causes, caused_by;
    std::map<std::int64_t, std::map<std::int64_t, std::vector<std::int64_t>>> roles;  // event -> seq -> ids
    std::map<std::int64_t, std::map<std::int64_t, std::int64_t>> values;              // event -> seq -> value
    explicit Index(const Tables& t) {
        for (const Row& r : t.event) ev[at(r, 0)] = &r;
        for (const Row& r : t.settlement) st[at(r, 0)] = &r;
        for (const Row& r : t.civilization) cv[at(r, 0)] = &r;
        for (const Row& r : t.phase) ph[at(r, 0)] = &r;
        for (const Row& c : t.event_cause) {
            causes[at(c, 0)].push_back(at(c, 1));
            caused_by[at(c, 1)].push_back(at(c, 0));
        }
        for (const Row& p : t.event_participant) roles[at(p, 0)][at(p, 2)].push_back(at(p, 4));
        for (const Row& v : t.event_value) values[at(v, 0)][at(v, 1)] = at(v, 2);
    }
    [[nodiscard]] std::int64_t kind(std::int64_t e) const { return at(*ev.at(e), 3); }
    [[nodiscard]] std::int64_t year(std::int64_t e) const { return at(*ev.at(e), 1); }
    [[nodiscard]] std::int64_t role1(std::int64_t e, std::int64_t seq) const {
        const auto it = roles.find(e);
        if (it == roles.end() || it->second.count(seq) == 0 || it->second.at(seq).empty()) return 0;
        return it->second.at(seq).front();
    }
    [[nodiscard]] std::vector<std::int64_t> kids(std::int64_t e) const {
        const auto it = caused_by.find(e);
        return it == caused_by.end() ? std::vector<std::int64_t>{} : it->second;
    }
};

// P3-06 r14 21.8, P3-07 r12 22.5b (P3-10, generator 0.19.0): the years in which region g's holder changes, from
// the rows: the from_year of each region_control row of g whose civ differs from that of g's previous row (g's
// first row is a change); a row of the same civilization after a gap (a release and a resettlement) is not a
// change. Before 0.19.0 both ends of every row counted (the 0.18.0 reading, kept for the record only).
inline std::vector<std::int64_t> holder_change_years(const Tables& t, std::int64_t g) {
    std::vector<const Row*> rows;
    for (const Row& rc : t.region_control) {
        if (at(rc, 0) == g) rows.push_back(&rc);
    }
    std::sort(rows.begin(), rows.end(), [](const Row* a, const Row* b) {
        return std::make_pair(at(*a, 1), at(*a, 3)) < std::make_pair(at(*b, 1), at(*b, 3));
    });
    std::vector<std::int64_t> out;
    std::int64_t prev = -1;
    for (const Row* rc : rows) {
        if (at(*rc, 3) != prev) out.push_back(at(*rc, 1));
        prev = at(*rc, 3);
    }
    return out;
}

// P3-07 r8 21.5 (P3-09, generator 0.18.0): the six [stats] keys appended after wars_attacker_<race>, from the
// rows. "The war of d": the battles caused by d, the sieges caused by those, the falls caused by those, and the
// war row's until_event u with the cessions and tributes caused by u.
inline std::vector<std::string> stats_lines_21(const Tables& t, const eng::HistoryInputs& in) {
    const Index x(t);
    const std::int64_t E = in.params.span;
    std::map<std::int64_t, const Row*> war_row;
    for (const Row& r : t.relation) {
        if (r[4].bytes == "war") war_row[at(r, 7)] = &r;
    }
    std::int64_t decl = 0, hollow = 0, unfound = 0;
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<std::int64_t>> groups;  // (defender, region) -> wars
    for (const Row& e : t.event) {
        if (at(e, 3) != k_war_declared) continue;
        const std::int64_t d = at(e, 0);
        ++decl;
        groups[{x.role1(d, 2), at(e, 5)}].push_back(d);
        bool changed = false;
        for (const std::int64_t b : x.kids(d)) {
            if (x.kind(b) != k_battle && x.kind(b) != k_battle_at_pass) continue;
            for (const std::int64_t sg : x.kids(b)) {
                if (x.kind(sg) != k_siege) continue;
                for (const std::int64_t f : x.kids(sg)) {
                    changed = changed || x.kind(f) == k_sacked || x.kind(f) == k_settlement_captured;
                }
            }
        }
        const auto it = war_row.find(d);
        if (it != war_row.end() && !is_null((*it->second)[6])) {
            const std::int64_t u = at(*it->second, 8);
            if (x.kind(u) == k_search_abandoned) {  // P3-07 r8 21.8 item 2: counted apart
                ++unfound;
                continue;
            }
            changed = changed || x.kind(u) == k_civ_conquered;
            for (const std::int64_t k : x.kids(u)) changed = changed || is_cede(x.kind(k)) || x.kind(k) == k_tribute;
        }
        hollow += changed ? 0 : 1;
    }
    std::int64_t same_max = 0, unchanged_max = 0;
    for (const auto& [key, ds] : groups) {
        same_max = std::max<std::int64_t>(same_max, static_cast<std::int64_t>(ds.size()));
        const std::vector<std::int64_t> changes = holder_change_years(t, key.second);  // r12 22.5b: by holder
        auto moved = [&changes](std::int64_t a, std::int64_t b) {
            for (const std::int64_t y : changes) {
                if (a <= y && y <= b) return true;
            }
            return false;
        };
        // P3-07 r11 21.8 item 5 (P3-06 r13 20.28), the incremental form: the counted wars leave out those closed by
        // a side's end (until_event a civ_conquered or civ_ended), which neither extend nor break a run; a run's
        // window ends at the latest end in it; the first counted war after a change starts a run of 1.
        std::int64_t run = 0, lo = 0, hi = 0;
        for (const std::int64_t d : ds) {  // id order
            const Row& r = *war_row.at(d);
            if (!is_null(r[6]) && (x.kind(at(r, 8)) == k_civ_conquered || x.kind(at(r, 8)) == k_civ_ended)) continue;
            const std::int64_t s = at(r, 5), u = is_null(r[6]) ? E : at(r, 6);
            if (run > 0 && !moved(lo, std::max(hi, u))) {
                ++run;
                hi = std::max(hi, u);
            } else if (!moved(s, u)) {
                run = 1;
                lo = s;
                hi = u;
            } else {
                run = 0;
            }
            unchanged_max = std::max(unchanged_max, run);
        }
    }
    std::map<std::int64_t, std::int64_t> civ_race;
    for (const Row& c : t.civilization) civ_race[at(c, 0)] = at(c, 2);
    std::int64_t pop = 0, held = 0;
    std::map<std::int64_t, std::int64_t> pop_r, held_r;
    for (const Row& s : t.settlement) {
        if (at(s, 10) != 1 && at(s, 10) != 2) continue;
        pop += at(s, 11);
        pop_r[civ_race[at(s, 8)]] += at(s, 11);
    }
    for (const Row& rc : t.region_control) {
        if (!is_null(rc[2])) continue;
        ++held;
        held_r[civ_race[at(rc, 3)]] += 1;
    }
    std::int64_t pshare = 0, rshare = 0, held_1 = 0;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_refuge_held && x.values.count(at(e, 0)) != 0 && x.values.at(at(e, 0)).count(1) != 0 &&
            x.values.at(at(e, 0)).at(1) == 1) {
            ++held_1;
        }
    }
    for (const db::RaceRow& r : in.races) {
        if (pop > 0) pshare = std::max(pshare, pop_r[r.id] * 1000 / pop);
        if (held > 0) rshare = std::max(rshare, held_r[r.id] * 1000 / held);
    }
    return {"wars_hollow = " + std::to_string(hollow),
            "wars_hollow_permille = " + std::to_string(decl > unfound ? 1000 * hollow / (decl - unfound) : 0),
            "wars_same_goal_max = " + std::to_string(same_max),
            "wars_same_goal_unchanged_max = " + std::to_string(unchanged_max),
            "race_population_share_max_permille = " + std::to_string(pshare),
            "race_regions_share_max_permille = " + std::to_string(rshare),
            // P3-07 r8 21.8 items 2 and 3
            "wars_unfound = " + std::to_string(unfound),
            "refuges_held_no_destination = " + std::to_string(held_1)};
}

// P3-05 15.4's three keys, then P3-06 15.4's, in order.
inline std::vector<std::string> stats_lines_06(const Tables& t, const eng::HistoryInputs& in) {
    std::vector<std::string> o = stats_lines(t);
    const Index x(t);
    auto rel_kind = [](const Row& r) { return r[4].bytes; };
    std::int64_t enmity = 0, peace = 0, total = 0, mx = 0;
    for (const Row& r : t.relation) {
        enmity += rel_kind(r) == "enmity" ? 1 : 0;
        peace += rel_kind(r) == "peace" ? 1 : 0;
        if (rel_kind(r) == "war") {
            const std::int64_t y = (is_null(r[6]) ? in.params.span : at(r, 6)) - at(r, 5);
            total += y;
            mx = std::max(mx, y);
        }
    }
    o.push_back("relations_enmity = " + std::to_string(enmity));
    o.push_back("relations_peace = " + std::to_string(peace));
    o.push_back("war_years_total = " + std::to_string(total));
    o.push_back("war_years_max = " + std::to_string(mx));
    std::int64_t conquered = 0, sacked = 0, regions = 0, alive_races = 0;
    for (const Row& c : t.civilization) conquered += (at(c, 9) != 0 && x.kind(at(c, 9)) == k_civ_conquered) ? 1 : 0;
    for (const Row& s : t.settlement) sacked += (at(s, 13) != 0 && x.kind(at(s, 13)) == k_sacked) ? 1 : 0;
    std::set<std::int64_t> captured;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_settlement_captured) captured.insert(at(e, 6));
    }
    for (const Row& r : t.region_control) {
        const std::int64_t k = x.kind(at(r, 4));
        regions += (k == k_settlement_captured || k == k_region_ceded) ? 1 : 0;
    }
    for (const db::RaceRow& r : in.races) {
        bool alive = false;
        for (const Row& c : t.civilization) alive = alive || (at(c, 2) == r.id && is_null(c[8]));
        alive_races += alive ? 1 : 0;
    }
    o.push_back("civilizations_conquered = " + std::to_string(conquered));
    o.push_back("settlements_sacked = " + std::to_string(sacked));
    o.push_back("settlements_captured = " + std::to_string(captured.size()));
    o.push_back("regions_conquered = " + std::to_string(regions));
    o.push_back("races_alive = " + std::to_string(alive_races));
    for (const db::RaceRow& r : in.races) {
        std::int64_t n = 0;
        for (const Row& rel : t.relation) {
            n += (rel_kind(rel) == "war" && at(*x.cv.at(at(rel, 1)), 2) == r.id) ? 1 : 0;
        }
        o.push_back("wars_attacker_" + r.key + " = " + std::to_string(n));
    }
    for (const std::string& l : stats_lines_21(t, in)) o.push_back(l);
    return o;
}

// A golden war line (after "<case> war ") without its <how> token: the end rule is not stored, everything
// else is. Tokens: e<w> <start>-<end> c<a> c<b> rg<g> claim e<c> battles n sacked n taken n <how> <outcome>.
inline std::string without_how(const std::string& line) {
    std::vector<std::string> w = split(line, ' ');
    REQUIRE(w.size() == 15U);
    w.erase(w.begin() + 13);
    std::string out;
    for (const std::string& s : w) out += (out.empty() ? "" : " ") + s;
    return out;
}

// The per-war lines of the golden file re-derived from rows (without <how>): one per war_declared, from its
// war row, its battles (caused by it), their sieges and falls, and the event that closed the war row.
inline std::vector<std::string> war_lines_from_rows(const Tables& t) {
    const Index x(t);
    std::map<std::int64_t, const Row*> war_row;
    for (const Row& r : t.relation) {
        if (r[4].bytes == "war") war_row[at(r, 7)] = &r;
    }
    std::vector<std::string> out;
    for (const Row& e : t.event) {
        if (at(e, 3) != k_war_declared) continue;
        const std::int64_t w = at(e, 0);
        std::int64_t claim = 0;
        for (const std::int64_t c : x.causes.at(w)) {
            if (is_claim(x.kind(c))) claim = c;
        }
        std::int64_t battles = 0, sacked = 0, taken = 0;
        for (const std::int64_t b : x.kids(w)) {
            if (x.kind(b) != k_battle && x.kind(b) != k_battle_at_pass) continue;
            ++battles;
            for (const std::int64_t s : x.kids(b)) {
                if (x.kind(s) != k_siege) continue;
                for (const std::int64_t f : x.kids(s)) {
                    sacked += x.kind(f) == k_sacked ? 1 : 0;
                    taken += x.kind(f) == k_settlement_captured ? 1 : 0;
                }
            }
        }
        std::string end = "open", outcome = "-";
        const auto it = war_row.find(w);
        if (it != war_row.end() && !is_null((*it->second)[6])) {
            end = std::to_string(at(*it->second, 6));
            const std::int64_t u = at(*it->second, 8);
            if (x.kind(u) == k_peace || x.kind(u) == k_war_won) {  // r10: war_won (kind 34) too
                outcome = "won-c" + std::to_string(x.role1(u, 1));
                for (const std::int64_t k : x.kids(u)) {
                    if (x.kind(k) == k_region_ceded) outcome += "-ceded";
                    if (x.kind(k) == k_settlements_ceded) outcome += "-foothold";  // P3-06 r19 23.3
                }
                for (const std::int64_t k : x.kids(u)) {
                    if (x.kind(k) == k_tribute) outcome += "-tribute";  // r9 20.3 (0.18.0)
                }
            } else if (x.kind(u) == k_truce) {
                outcome = "truce";
            } else if (x.kind(u) == k_search_abandoned) {
                outcome = "unfound";  // r10 20.17
            }
        }
        out.push_back("e" + std::to_string(w) + " " + std::to_string(at(e, 1)) + "-" + end + " c" +
                      std::to_string(x.role1(w, 1)) + " c" + std::to_string(x.role1(w, 2)) + " rg" +
                      std::to_string(at(e, 5)) + " claim e" + std::to_string(claim) + " battles " +
                      std::to_string(battles) + " sacked " + std::to_string(sacked) + " taken " + std::to_string(taken) +
                      " " + outcome);
    }
    return out;
}
inline std::vector<std::string> golden_wars_without_how(const CCase& g) {
    std::vector<std::string> out;
    for (const std::string& w : g.wars) out.push_back(without_how(w));
    return out;
}

// --- Invariants of 14 from rows (items 1-5, 7 and the trigger of 8; 6 and 9 are tests of their own) ----

inline void check_invariants_06(const Tables& t, const eng::HistoryInputs& in, std::int64_t quiet_step_years,
                                bool last_refuge) {
    const Index x(t);
    const std::int64_t INF = std::int64_t{1} << 60;
    // 1 relations
    std::set<std::pair<std::int64_t, std::int64_t>> enmity;
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<std::pair<std::int64_t, std::int64_t>>> spans;
    std::map<std::int64_t, const Row*> war_row;
    for (const Row& r : t.relation) {
        CAPTURE(str(r));
        const std::string& k = r[4].bytes;
        CHECK(at(r, 0) == 3);
        CHECK(at(r, 2) == 3);
        CHECK((k == "enmity" || k == "war" || k == "peace"));
        const std::int64_t a = at(r, 1), b = at(r, 3), sk = x.kind(at(r, 7));
        const std::int64_t uk = is_null(r[6]) ? 0 : x.kind(at(r, 8));
        if (k == "enmity") {
            CHECK(a < b);
            CHECK(enmity.insert({a, b}).second);
            CHECK((sk == 2 || sk == 15));  // civ_founded, civ_seceded
            CHECK(at(r, 5) == at(*x.cv.at(b), 6));
            CHECK(at(r, 7) == at(*x.cv.at(b), 7));
            if (uk != 0) CHECK((uk == 18 || uk == k_civ_conquered));
        } else if (k == "war") {
            CHECK(sk == k_war_declared);
            CHECK(x.role1(at(r, 7), 1) == a);
            CHECK(x.role1(at(r, 7), 2) == b);
            CHECK(x.year(at(r, 7)) == at(r, 5));
            if (uk != 0) {
                CHECK((uk == k_peace || uk == k_truce || uk == 18 || uk == k_civ_conquered || uk == k_war_won ||
                       uk == k_search_abandoned));  // r10: kinds 34, 35
                // 7.3: no war ends in its declaration year by peace or truce; a side conquered by another
                // war of that year's T6 closes it with since = until (8.3 item 7, P3-04 6.7 r9)
                CHECK((at(r, 6) > at(r, 5) || uk == 18 || uk == k_civ_conquered));
            }
            war_row[at(r, 7)] = &r;
        } else {
            CHECK(a < b);
            CHECK((sk == k_peace || sk == k_truce || sk == k_war_won || sk == k_search_abandoned));
            if (uk != 0) CHECK((uk == k_war_declared || uk == 18 || uk == k_civ_conquered));
        }
        if (k != "enmity") {
            spans[{std::min(a, b), std::max(a, b)}].push_back({at(r, 5), is_null(r[6]) ? INF : at(r, 6)});
        }
    }
    for (auto& [pair, v] : spans) {
        std::sort(v.begin(), v.end());
        for (std::size_t i = 1; i < v.size(); ++i) {
            CAPTURE(pair.first);
            CAPTURE(pair.second);
            CHECK(v[i - 1].second <= v[i].first);  // war and peace rows of a pair never overlap (half-open)
        }
    }
    // 2 wars
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        const bool quiet = at(*x.ph.at(at(e, 4)), 2) == 2;
        CAPTURE(id);
        if (k == k_war_declared) {
            CHECK(war_row.count(id) == 1U);  // N5: a war always leaves a war row
            bool claimed = false;
            for (const std::int64_t c : x.causes.at(id)) {
                claimed = claimed || (is_claim(x.kind(c)) && x.role1(c, 1) == x.role1(id, 1) &&
                                      x.role1(c, 2) == x.role1(id, 2) && x.year(c) <= at(e, 1));
            }
            CHECK(claimed);
        }
        if (k == k_battle || k == k_battle_at_pass || k == k_siege || k == k_sacked || k == k_settlement_captured ||
            k == k_refuge_held || k == k_search_abandoned) {
            CHECK_FALSE(quiet);
        }
        if ((k == k_peace || k == k_truce || is_cede(k) || k == k_tribute || k == k_war_won) && quiet) {
            CHECK((at(e, 1) - at(*x.ph.at(at(e, 4)), 3)) % quiet_step_years == 0);
        }
        if (k == k_battle || k == k_battle_at_pass) {
            // r7 7.3 item 4 (0.17.0): with the shipped loss_winner 80 <= loss_loser 250 the victors never lose
            // more than the defeated (dead_W from the smaller force)
            CHECK(x.values.at(id).at(1) <= x.values.at(id).at(2));
            REQUIRE(x.causes.at(id).size() == 1U);
            CHECK(x.kind(x.causes.at(id)[0]) == k_war_declared);
        }
        if (is_claim(k)) {  // 8.3 item 4 (N2): a colony of the same step is no claim cause, unless it is
                             // the claimed region's owner_event (its ownership is live state)
            for (const std::int64_t c : x.causes.at(id)) {
                if (x.kind(c) != k_colony_founded || x.year(c) != at(e, 1)) continue;
                bool owner_event = false;
                for (const Row& rc : t.region_control) owner_event = owner_event || (at(rc, 0) == at(e, 5) && at(rc, 4) == c);
                // P3-10 r3 (stress-7 e1153): a colony of the holder in the claimed region, which the holder did not
                // hold the year before, was the owner_event at the claim even when a later step of the same year
                // (a secession) passed the region on and the net rule left it no region_control row
                if (!owner_event && at(*x.ev.at(c), 5) == at(e, 5) && x.role1(c, 2) == x.role1(id, 2)) {
                    bool held_before = false;
                    for (const Row& rc : t.region_control) {
                        held_before = held_before || (at(rc, 0) == at(e, 5) && at(rc, 3) == x.role1(id, 2) &&
                                                      at(rc, 1) <= at(e, 1) - 1 && (is_null(rc[2]) || at(rc, 2) > at(e, 1) - 1));
                    }
                    owner_event = !held_before;
                }
                CHECK(owner_event);
            }
        }
        // 3 falls
        if (k == k_sacked || k == k_settlement_captured) {
            REQUIRE(x.causes.at(id).size() == 1U);
            const std::int64_t s = x.causes.at(id)[0];
            CHECK(x.kind(s) == k_siege);
            CHECK(at(*x.ev.at(s), 6) == at(e, 6));
            CHECK(x.year(s) == at(e, 1));
            CHECK(x.values.at(s).at(1) == 1);
            const std::int64_t b = x.causes.at(s).at(0);
            CHECK((x.kind(b) == k_battle || x.kind(b) == k_battle_at_pass));
            CHECK(x.role1(b, 1) == x.role1(s, 1));
            CHECK(x.year(b) == at(e, 1));
        }
        // 4 ends
        if (k == k_civ_conquered) {
            const std::int64_t y = x.role1(id, 1), c = x.role1(id, 2);
            CHECK(at(*x.cv.at(y), 9) == id);
            REQUIRE(x.causes.at(id).size() == 1U);
            const std::int64_t f = x.causes.at(id)[0];
            CHECK((x.kind(f) == k_sacked || x.kind(f) == k_settlement_captured));
            CHECK(x.role1(f, 2) == y);
            const Row& cr = *x.cv.at(c);
            CHECK((is_null(cr[8]) || at(cr, 8) > at(e, 1) || (at(cr, 8) == at(e, 1) && at(cr, 9) > id)));
        }
    }
    for (const Row& c : t.civilization) {
        if (is_null(c[8])) continue;
        for (const Row& r : t.relation) {
            CHECK_FALSE((is_null(r[6]) && (at(r, 1) == at(c, 0) || at(r, 3) == at(c, 0))));
        }
    }
    for (const Row& s : t.settlement) {
        if (at(s, 10) == ruined) {
            CHECK((x.kind(at(s, 13)) == k_settlement_abandoned || x.kind(at(s, 13)) == k_settlement_abandoned_to ||
                   x.kind(at(s, 13)) == k_sacked));
        }
    }
    // 5 territory
    for (const Row& r : t.region_control) {
        const std::int64_t k = x.kind(at(r, 4));
        // r9 20.13 item 1 (0.18.0): or a refugees_settled into a free region
        CHECK((k == 2 || k == k_colony_founded || k == k_civ_seceded || k == k_settlement_captured || k == k_region_ceded ||
               k == k_refugees_settled));
    }
    // 7 races
    if (last_refuge) {
        for (const db::AgeRow& a : in.ages) {
            bool alive = false;
            for (const Row& c : t.civilization) alive = alive || (at(c, 2) == a.race && is_null(c[8]));
            CHECK_MESSAGE(alive, "race " << a.race << " has no alive civilization");
        }
    }
    // P3-05 14 r6 items (6 as D1 reads it, 11, 12, 13) with conflict installed
    check_invariants_r6(t, 40, nullptr);
    // 8 every flare-up's trigger is a claim
    for (const Row& p : t.phase) {
        if (at(p, 2) == 3) CHECK(is_claim(x.kind(at(p, 5))));
    }
    // 8.3 item 4 (N2, r7): "a cession in a quiet step never transfers a colony founded earlier in the same step"
    // no longer holds: P3-06 r13 20.27 passes every settlement of the loser standing in the ceded region, a
    // same-step colony included (invariant (13), cession_violations, checked with the r9 invariants).
}

// --- P3-06 r9 20.13 items 3-6 from the rows (P3-09, generator 0.18.0) --------------------------------

// B(R, r) of P3-05 4.2 from the inputs: sum over the race's biome weights of weight x share, div 1000, share =
// cells x 1000 div the region's land cells.
inline std::int64_t biome_term(const eng::HistoryInputs& in, std::int64_t region, std::int64_t race) {
    std::map<std::int64_t, bool> water;
    for (const db::BiomeRow& b : in.biomes) water[b.id] = b.water != 0;
    std::int64_t land = 0;
    std::map<std::int64_t, std::int64_t> cells;
    for (const db::RegionBiomeRow& rb : in.region_biomes) {
        if (rb.region != region) continue;
        cells[rb.biome] = rb.cells;
        land += water.count(rb.biome) != 0 && !water.at(rb.biome) ? rb.cells : 0;
    }
    std::int64_t sum = 0;
    for (const db::RaceBiomeRow& w : in.race_biomes) {
        if (w.race != race) continue;
        const auto it = cells.find(w.biome);
        sum += w.weight * (it == cells.end() || land == 0 ? 0 : it->second * 1000 / land);
    }
    return sum / 1000 - ((sum % 1000 != 0 && sum < 0) ? 1 : 0);  // floor division, as the reference's //
}

// A race field of the content (by race key).
inline std::int64_t race_field(const wd::ContentSet& c, const std::string& key, std::string_view field) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == key) return d.record.get_int(field);
        }
    }
    FAIL("no race " << key);
    return 0;
}

// 20.13 items 5 and 12 (r11 20.23: with the wars open over (defender, region)), sound and conservative from the rows (the reference's weary_violations): per (defender, region)
// the wars ended by peace or truce, in id order, during whose span (and since the previous one) the region's
// owner never changed count; a claim or declaration naming (defender, region) after `weary` of them, with no
// change since the last, is a violation.
inline std::vector<std::int64_t> weary_violations(const Tables& t, std::int64_t weary) {
    std::vector<std::int64_t> out;
    if (weary == 0) return out;
    const Index x(t);
    std::map<std::int64_t, const Row*> war_row;
    for (const Row& r : t.relation) {
        if (r[4].bytes == "war") war_row[at(r, 7)] = &r;
    }
    std::map<std::int64_t, std::vector<std::int64_t>> changes;  // P3-06 r14 21.8: changes of holder
    for (const Row& rc : t.region_control) {
        if (changes.count(at(rc, 0)) == 0) changes[at(rc, 0)] = holder_change_years(t, at(rc, 0));
    }
    struct End {
        std::int64_t u, y0, y1;
    };
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<End>> ends;
    for (const Row& e : t.event) {
        if (at(e, 3) != k_war_declared) continue;
        const auto it = war_row.find(at(e, 0));
        if (it == war_row.end() || is_null((*it->second)[6])) continue;
        const std::int64_t u = at(*it->second, 8);
        if (x.kind(u) != k_peace && x.kind(u) != k_truce && x.kind(u) != k_war_won && x.kind(u) != k_search_abandoned) {
            continue;
        }
        ends[{x.role1(at(e, 0), 2), at(e, 5)}].push_back({u, at(e, 1), at(*it->second, 6)});
    }
    auto any_in = [&](std::int64_t g, std::int64_t lo, std::int64_t hi) {
        for (const std::int64_t y : changes[g]) {
            if (lo <= y && y <= hi) return true;
        }
        return false;
    };
    for (const Row& e : t.event) {
        if (!is_claim(at(e, 3)) && at(e, 3) != k_war_declared) continue;
        const std::int64_t b = x.role1(at(e, 0), 2), g = at(e, 5);
        std::vector<End> done;
        for (const End& d : ends[{b, g}]) {
            if (d.u < at(e, 0)) done.push_back(d);
        }
        std::sort(done.begin(), done.end(), [](const End& p, const End& q) { return p.u < q.u; });
        std::int64_t run = 0, last = -1;
        for (const End& d : done) {
            if (last >= 0 && any_in(g, last, d.y0)) run = 0;
            if (any_in(g, d.y0, d.y1)) {
                run = 0;
            } else {
                ++run;
            }
            last = d.y1;
        }
        // r11 20.23 (invariant 12): the wars over (b, g) open at e, by any attacker, count too
        std::int64_t open = 0;
        for (const Row& r : t.relation) {
            if (r[4].bytes != "war" || at(r, 7) >= at(e, 0)) continue;
            if (at(*x.ev.at(at(r, 7)), 5) != g || x.role1(at(r, 7), 2) != b) continue;
            open += (is_null(r[6]) || at(r, 8) > at(e, 0)) ? 1 : 0;
        }
        const std::int64_t now = (last >= 0 && !any_in(g, last, at(e, 1))) ? run : 0;
        if (now + open >= weary) out.push_back(at(e, 0));
    }
    return out;
}

// P3-06 r13 20.27 item 6, invariant (13) of 20.13, from the rows (the reference's cession_violations, same
// messages): settlement owners replayed from the log in event id order. A settlement's founding_event gives it to
// its founder_civ; a civ_seceded passes `from`'s standing settlements in the regions whose region_control row has
// that event (and its own region); a settlement_captured gives it to the attacker; sacked, settlement_abandoned and
// settlement_abandoned_to ruin it; a region_ceded of G at year Y from L to W passes every settlement of L standing
// in G. At each region_ceded: L held G (a region_control row of L for G open at Y, or, when L's hold began in the
// same step and P3-04 6.7 item 1's net rule left L no row, a settlement of L standing in G in the replay), and
// every settlement of L standing in G just before it has a census row of year Y whose owner is not L. Finally the
// replay's owners and standing equal the settlement table. Empty when the invariant holds.
inline std::vector<std::string> cession_violations(const Tables& t) {
    const Index x(t);
    std::map<std::int64_t, std::int64_t> founded;  // founding event -> settlement
    for (const Row& s : t.settlement) founded[at(s, 7)] = at(s, 0);
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> census;  // (settlement, year) -> owner
    for (const Row& c : t.settlement_census) census[{at(c, 0), at(c, 1)}] = at(c, 2);
    std::map<std::int64_t, std::set<std::int64_t>> rc_regions;  // event -> regions of its region_control rows
    for (const Row& rc : t.region_control) rc_regions[at(rc, 4)].insert(at(rc, 0));
    std::map<std::int64_t, std::int64_t> owner;
    std::map<std::int64_t, bool> standing;
    std::vector<std::string> out;
    for (const auto& [eid, ep] : x.ev) {  // id order
        const Row& e = *ep;
        const std::int64_t y = at(e, 1), k = at(e, 3);
        if (const auto f = founded.find(eid); f != founded.end()) {
            owner[f->second] = at(*x.st.at(f->second), 5);
            standing[f->second] = true;
        }
        if (k == k_civ_seceded) {
            const std::int64_t c2 = x.role1(eid, 1), from = x.role1(eid, 2);
            std::set<std::int64_t> regs = rc_regions[eid];
            regs.insert(at(e, 5));
            for (auto& [s, o] : owner) {
                if (o == from && standing[s] && regs.count(at(*x.st.at(s), 4)) != 0) o = c2;
            }
        } else if (k == k_settlement_captured) {
            owner[at(e, 6)] = x.role1(eid, 1);
        } else if (k == k_sacked || k == k_settlement_abandoned || k == k_settlement_abandoned_to) {
            owner[at(e, 6)] = 0;
            standing[at(e, 6)] = false;
        } else if (is_cede(k)) {
            const std::int64_t g = at(e, 5), L = x.role1(eid, 1), W = x.role1(eid, 2);
            std::vector<std::int64_t> mine;
            for (const auto& [s, o] : owner) {
                if (o == L && standing[s] && at(*x.st.at(s), 4) == g) mine.push_back(s);
            }
            bool held_row = false;
            // P3-06 r19 23.3: (13) for the foothold cession (kind 40): L stands in G and W holds it
            const std::int64_t holder_civ = k == k_settlements_ceded ? W : L;
            for (const Row& rc : t.region_control) {
                held_row = held_row || (at(rc, 0) == g && at(rc, 3) == holder_civ && at(rc, 1) <= y &&
                                        (is_null(rc[2]) || at(rc, 2) >= y));
            }
            if (k == k_settlements_ceded) held_row = held_row && !mine.empty();
            if (!held_row && mine.empty()) {
                out.push_back("e" + std::to_string(eid) + ": c" + std::to_string(L) + " ceded rg" + std::to_string(g) +
                              ", which it did not hold");
            }
            for (const std::int64_t s : mine) {
                const auto c = census.find({s, y});
                if (c == census.end() || c->second == L) {
                    out.push_back("e" + std::to_string(eid) + ": s" + std::to_string(s) + " of c" + std::to_string(L) +
                                  " in rg" + std::to_string(g) + " did not pass to c" + std::to_string(W) + " (census of " +
                                  std::to_string(y) + ": " + (c == census.end() ? std::string("none") : "c" + std::to_string(c->second)) +
                                  ")");
                }
                owner[s] = W;
            }
        }
    }
    for (const Row& s : t.settlement) {
        const std::int64_t id = at(s, 0);
        const bool st = at(s, 10) == thriving || at(s, 10) == declining;
        const bool known = standing.count(id) != 0;
        if (!known || standing[id] != st || (st && owner[id] != at(s, 8))) {
            out.push_back("s" + std::to_string(id) + ": replayed owner c" + (known ? std::to_string(owner[id]) : std::string("None")) +
                          " standing " + (known ? (standing[id] ? "True" : "False") : "None") + ", stored c" +
                          std::to_string(at(s, 8)) + " status " + std::to_string(at(s, 10)));
        }
    }
    return out;
}

// Items 3, 4 and 6 of 20.13, the cost rule of 20.3 and P3-09 10 (every won war costs the loser a region or a
// tribute, except a last refuge's or when it has no visible people), with `hidden_years`, `abandon_min`,
// `capital_sacked` and `weary` the tuning's values.
inline void check_invariants_09(const Tables& t, const eng::HistoryInputs& in, const wd::ContentSet& content) {
    const Index x(t);
    (void)in;  // r10: the race fields are no longer read here (concealed people are in presence)
    const std::map<std::int64_t, std::int64_t> hidden_of_race = race_hidden_years(content);  // r17 22.2
    const std::int64_t capital_sacked = conflict_int(content, "capital_sacked_salience");
    std::int64_t abandon_min = 0;
    for (const wd::KindContent& k : content.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == "peoples") abandon_min = d.record.get_int("abandon_min");
        }
    }
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3), y = at(e, 1);
        CAPTURE(id);
        if (k == k_refugees_settled) {
            const std::int64_t h = x.role1(id, 1), founder = x.role1(id, 2), origin = x.role1(id, 3);
            const std::int64_t hidden_years = hidden_of_race.at(at(*x.cv.at(founder), 2));
            CHECK(x.values.at(id).at(1) >= abandon_min);
            REQUIRE(x.causes.at(id).size() == 1U);
            const std::int64_t sack = x.causes.at(id)[0];
            CHECK(x.kind(sack) == k_sacked);
            CHECK(x.year(sack) == y);
            CHECK(at(*x.ev.at(sack), 6) == origin);
            CHECK(x.role1(sack, 2) == founder);
            CHECK(at(*x.ev.at(sack), 7) == capital_sacked);  // the origin was the founder's capital (its only one)
            CHECK(at(e, 6) == h);
            const Row& s = *x.st.at(h);
            CHECK(at(s, 7) == id);  // founded by the event
            CHECK(at(s, 5) == founder);
            CHECK(at(s, 4) == at(e, 5));
            for (const std::int64_t kid : x.kids(sack)) CHECK(x.kind(kid) != k_settlement_abandoned_to);
            // the founder was its race's only alive civilization
            const std::int64_t race = at(*x.cv.at(founder), 2);
            for (const Row& c : t.civilization) {
                if (at(c, 0) == founder || at(c, 2) != race || at(c, 6) > y) continue;
                const bool alive = is_null(c[8]) || at(c, 8) > y || (at(c, 8) == y && at(c, 9) > id);
                CHECK_MESSAGE(!alive, "civilization " << at(c, 0) << " of the founder's race was alive");
            }
            // hidden for hidden_years: never besieged; no battle, claim or declaration against the founder in its
            // region while it is the founder's only settlement there (conservatively: no other settlement of the
            // founder's there founded by then)
            for (const Row& o : t.event) {
                const std::int64_t oy = at(o, 1);
                if (at(o, 0) <= id || oy >= y + hidden_years) continue;
                if (at(o, 3) == k_siege) CHECK_MESSAGE(at(o, 6) != h, "siege e" << at(o, 0) << " of a hidden refuge");
                if (at(o, 5) != at(e, 5)) continue;
                const std::int64_t ok = at(o, 3);
                bool against = false;
                if (ok == k_battle || ok == k_battle_at_pass) {
                    against = x.role1(at(o, 0), 1) == founder || x.role1(at(o, 0), 2) == founder;
                } else if (is_claim(ok) || ok == k_war_declared) {
                    against = x.role1(at(o, 0), 2) == founder;
                }
                if (!against) continue;
                bool alone = true;
                for (const Row& s2 : t.settlement) {
                    alone = alone && !(at(s2, 0) != h && at(s2, 4) == at(e, 5) && at(s2, 6) <= oy &&
                                       (at(s2, 5) == founder || at(s2, 8) == founder));
                }
                CHECK_MESSAGE(!alone, "event e" << at(o, 0) << " against a hidden refuge's region");
            }
        }
        if (k == k_tribute) {
            REQUIRE(x.causes.at(id).size() == 1U);
            const std::int64_t p = x.causes.at(id)[0];
            CHECK((x.kind(p) == k_peace || x.kind(p) == k_war_won));  // r10 20.15: war_won too
            CHECK(x.role1(p, 2) == x.role1(id, 1));  // the loser pays
            CHECK(x.role1(p, 1) == x.role1(id, 2));  // to the winner
            CHECK(x.values.at(id).at(1) >= 1);
            CHECK(at(e, 5) == 0);
        }
        if (k == k_peace || k == k_truce || k == k_war_won || k == k_search_abandoned) {
            const bool won = k == k_peace || k == k_war_won;
            std::int64_t costs = 0;
            for (const std::int64_t kid : x.kids(id)) costs += (is_cede(x.kind(kid)) || x.kind(kid) == k_tribute) ? 1 : 0;
            // one cession or one tribute per won war; a truce or a war given up (r10 20.17) costs nothing
            CHECK(costs <= (won ? 1 : 0));
            if (won && costs == 0) {
                // the exception of 20.3 / P3-09 10: the loser was its race's last alive civilization (a last
                // refuge); from r10 concealed people are in presence and pay like any (20.21 item 3)
                const std::int64_t loser = x.role1(id, 2), race = at(*x.cv.at(loser), 2);
                bool alone = true;
                for (const Row& c : t.civilization) {
                    if (at(c, 0) == loser || at(c, 2) != race || at(c, 6) > y) continue;
                    alone = alone && !(is_null(c[8]) || at(c, 8) > y || (at(c, 8) == y && at(c, 9) > id));
                }
                CHECK_MESSAGE(alone, "e" << id << " won without a cession or tribute");
            }
        }
        // item 6 of r9 (no siege of a concealed settlement) is replaced by r10 20.17: a concealed settlement
        // is besieged once found; see check_invariants_10
    }
    // item 5 from the log
    CHECK(weary_violations(t, conflict_int(content, "weary_wars")) == std::vector<std::int64_t>{});
    // r13 20.27 item 6: invariant (13), a cession passes every settlement of the loser standing in the region
    CHECK(cession_violations(t) == std::vector<std::string>{});
}

// --- P3-06 r10 20.19 items 7-11 and 20.22 from the rows (P3-09 r3) ---------------------------------------

// The winner of the war of d's most recent battle (the battle with the largest id caused by d), 0 if none.
inline std::int64_t last_battle_winner(const Index& x, std::int64_t d) {
    std::int64_t last = 0;
    for (const std::int64_t b : x.kids(d)) {
        if ((x.kind(b) == k_battle || x.kind(b) == k_battle_at_pass) && b > last) last = b;
    }
    return last == 0 ? 0 : x.role1(last, 1);
}

// Sound checks from rows (the exact ones need the forces and the draws, which the reference recomputes and
// the golden tables pin): (8) T6, the loser of a war decided by the score never won its last battle (an
// attacker's win by aim excepted: a war in which the attacker took a settlement of the goal region);
// (7) every peace's loser bled in the war (lost people in its battles); (9) T7, the kill bound, by
// dead_W = min(F_W, F_L) x loss_winner div 1000 and dead_L <= F_W x kill_max div 1000 (and F_L x loss_loser):
// dead_L x loss_winner < (dead_W + 1) x kill_max; (10/11) a war given up had no battle in its last year;
// the war given up and the truce cost nothing (checked in check_invariants_09); refuge_held: caused by a battle
// its attacker won, reason 1 or 2, never followed by a fall.
inline void check_invariants_10(const Tables& t, const wd::ContentSet& content) {
    const Index x(t);
    const std::int64_t kill_max = conflict_int(content, "kill_max_permille");
    const std::int64_t loss_w = conflict_int(content, "loss_winner_permille");
    const std::int64_t loss_l = conflict_int(content, "loss_loser_permille");
    std::map<std::int64_t, std::int64_t> war_of_end;
    for (const Row& r : t.relation) {
        if (r[4].bytes == "war" && !is_null(r[6])) war_of_end[at(r, 8)] = at(r, 7);
    }
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        CAPTURE(id);
        if (k == k_peace || k == k_war_won) {
            REQUIRE(war_of_end.count(id) == 1U);
            const std::int64_t d = war_of_end.at(id);
            const std::int64_t winner = x.role1(id, 1), loser = x.role1(id, 2);
            // P3-06 r14 21.6 and invariant (8) as amended (generator 0.19.0): an ending by aim needs the attacker
            // holding the goal: an attacker's win where the attacker has a region_control row of the goal open at
            // the ending's year may be by aim (before 0.19.0: where the defender held none)
            bool aim = false;
            if (winner == x.role1(d, 1)) {
                const std::int64_t g = at(*x.ev.at(d), 5), y = at(e, 1), att = winner;
                bool holds = false;
                for (const Row& rc : t.region_control) {
                    holds = holds || (at(rc, 0) == g && at(rc, 3) == att && at(rc, 1) <= y && (is_null(rc[2]) || at(rc, 2) > y));
                }
                aim = holds;
            }
            CHECK_MESSAGE((aim || last_battle_winner(x, d) != loser),
                          "e" << id << ": the loser c" << loser << " won the war's last battle (T6)");
            if (k == k_peace) {
                std::int64_t bled = 0;
                for (const std::int64_t b : x.kids(d)) {
                    if (x.kind(b) != k_battle && x.kind(b) != k_battle_at_pass) continue;
                    bled += x.role1(b, 2) == loser ? x.values.at(b).at(2) : x.values.at(b).at(1);
                }
                CHECK_MESSAGE(bled > 0, "peace e" << id << ": its loser lost no one in the war (20.19 item 7)");
            }
        }
        if (k == k_battle || k == k_battle_at_pass) {
            const std::int64_t dw = x.values.at(id).at(1), dl = x.values.at(id).at(2);
            // F_W <= F_L: F_W < (dw + 1) / loss_w, dl <= F_W x kill_max; F_W > F_L: F_L < (dw + 1) / loss_w,
            // dl <= F_L x loss_l (all per mille): so dl x loss_w < (dw + 1) x max(kill_max, loss_l)
            CHECK_MESSAGE(dl * loss_w < (dw + 1) * std::max(kill_max, loss_l),
                          "battle e" << id << ": dead_L " << dl << " above the kill bound for dead_W " << dw);
        }
        if (k == k_search_abandoned) {
            REQUIRE(war_of_end.count(id) == 1U);
            const std::int64_t d = war_of_end.at(id);
            CHECK(x.role1(id, 1) != x.role1(id, 2));
            CHECK((x.role1(id, 1) == x.role1(d, 1) || x.role1(id, 1) == x.role1(d, 2)));
            CHECK(x.values.at(id).at(1) == at(e, 1) - x.year(d));
            for (const std::int64_t b : x.kids(d)) {
                if (x.kind(b) == k_battle || x.kind(b) == k_battle_at_pass) {
                    CHECK_MESSAGE(x.year(b) != at(e, 1), "battle e" << b << " in the year its war was given up");
                }
            }
        }
        if (k == k_refuge_held) {
            REQUIRE(x.causes.at(id).size() == 1U);
            const std::int64_t b = x.causes.at(id)[0];
            CHECK((x.kind(b) == k_battle || x.kind(b) == k_battle_at_pass));
            CHECK(x.role1(b, 1) == x.role1(id, 1));
            CHECK(x.year(b) == at(e, 1));
            CHECK((x.values.at(id).at(1) == 1 || x.values.at(id).at(1) == 2));
            for (const std::int64_t kid : x.kids(id)) {  // never followed by a fall (only by the war's end)
                CHECK((x.kind(kid) != k_sacked && x.kind(kid) != k_settlement_captured &&
                       x.kind(kid) != k_refugees_settled));
            }
        }
    }
}

// P3-06 16.2 item 18a (r7, the 0.16.0 validation's F1): below the first claim the log with conflict (t)
// equals the peoples-only log (pt) row for row in event, event_participant, event_cause and event_value,
// except the quiet_era events open in the quiet step of the first claim, whose end_year (not below the
// claim's year), years and generations may be smaller. With no claim, every history table but the three
// catalogue tables and relation (conflict opens enmity rows without a claim: the reading of the reference)
// is equal. Returns the number of excepted eras.
inline std::int64_t check_peoples_prefix(const Tables& t, const Tables& pt) {
    std::int64_t first = 0;
    for (const Row& e : t.event) {
        if (is_claim(at(e, 3))) {
            first = at(e, 0);
            break;
        }
    }
    if (first == 0) {
        CHECK(t.phase == pt.phase);
        CHECK(t.event == pt.event);
        CHECK(t.event_participant == pt.event_participant);
        CHECK(t.event_cause == pt.event_cause);
        CHECK(t.event_value == pt.event_value);
        CHECK(t.civilization == pt.civilization);
        CHECK(t.settlement == pt.settlement);
        CHECK(t.settlement_census == pt.settlement_census);
        CHECK(t.region_control == pt.region_control);
        return 0;
    }
    const std::int64_t claim_year = at(t.event[static_cast<std::size_t>(first - 1)], 1);
    std::set<std::int64_t> excepted;
    REQUIRE(pt.event.size() >= static_cast<std::size_t>(first - 1));
    for (std::size_t i = 0; i + 1 < static_cast<std::size_t>(first); ++i) {
        const Row& a = t.event[i];
        const Row& b = pt.event[i];
        if (a == b) continue;
        CAPTURE(at(a, 0));
        // P3-04 r12 20.1 (0.19.0): the quiet_stretch (kind 38) open in that step likewise, participants equal
        const bool era = (at(a, 3) == k_quiet_era || at(a, 3) == 38) && at(b, 3) == at(a, 3) && at(a, 1) == at(b, 1) &&
                         a[4] == b[4] && a[5] == b[5] && a[6] == b[6] && a[7] == b[7] && at(a, 2) >= claim_year &&
                         at(a, 2) <= at(b, 2);
        CHECK_MESSAGE(era, "event " << at(a, 0) << " below the first claim e" << first << " differs: " << str(a)
                                    << " against " << str(b));
        if (era) excepted.insert(at(a, 0));
    }
    auto below = [first](const std::vector<Row>& rows, const std::set<std::int64_t>& skip) {
        std::vector<Row> out;
        for (const Row& r : rows) {
            if (at(r, 0) < first && skip.count(at(r, 0)) == 0) out.push_back(r);
        }
        return out;
    };
    CHECK(below(t.event_participant, {}) == below(pt.event_participant, {}));
    CHECK(below(t.event_cause, {}) == below(pt.event_cause, {}));
    CHECK(below(t.event_value, excepted) == below(pt.event_value, excepted));
    return static_cast<std::int64_t>(excepted.size());
}

// P3-06 r14 21.6-21.10 (P3-10, generator 0.19.0), from the rows of a 0.19.0 world:
//   (14) no region_ceded or tribute is caused by a peace or war_won won by the attacker while neither side held the
//        goal at the ending's year and the defender stood nowhere in it (census of that year), unless the attacker
//        also won the war's last battle (21.6 item 2's score ending; the score itself needs the run);
//   21.7 item 2: no search_abandoned in a region in which its war fought a battle or battle_at_pass;
//   21.7 item 3: the first kind-35 event of the log has the kind's base salience, every later one
//        search_repeat_salience;
//   21.7 item 1: no claim (holder role) or war_declared (defender role) naming the hidden civilization y and the
//        searched region R in [Y_s, Y_s + search_weary_years) after the search, unless R's holder changed since;
//   (15) no colony_founded with founder y, no claim with claimant y and no war_declared with attacker y dated in
//        [Y_f, Y_f + hidden_years) after a refugees_settled of founder y in Y_f.
// (8) as amended is in check_invariants_10; (16) needs contact, which the rows do not hold (the reference
// recomputes it). Empty messages when every check holds.
inline std::vector<std::string> invariants_19(const Tables& t, const wd::ContentSet& content) {
    const Index x(t);
    std::vector<std::string> out;
    const std::int64_t weary_years = conflict_int(content, "search_weary_years");
    const std::int64_t repeat_sal = conflict_int(content, "search_repeat_salience");
    std::int64_t base35 = -1;
    for (const Row& k : t.event_kind) {
        if (at(k, 0) == k_search_abandoned) base35 = at(k, 5);
    }
    std::map<std::int64_t, std::int64_t> war_of_end;
    for (const Row& r : t.relation) {
        if (r[4].bytes == "war" && !is_null(r[6])) war_of_end[at(r, 8)] = at(r, 7);
    }
    auto holds = [&t](std::int64_t g, std::int64_t civ, std::int64_t y) {
        for (const Row& rc : t.region_control) {
            if (at(rc, 0) == g && at(rc, 3) == civ && at(rc, 1) <= y && (is_null(rc[2]) || at(rc, 2) > y)) return true;
        }
        return false;
    };
    auto stands = [&t, &x](std::int64_t g, std::int64_t civ, std::int64_t y) {
        for (const Row& c : t.settlement_census) {
            if (at(c, 1) != y || at(c, 2) != civ || (at(c, 4) != 1 && at(c, 4) != 2)) continue;
            const auto it = x.st.find(at(c, 0));
            if (it != x.st.end() && at(*it->second, 4) == g) return true;
        }
        return false;
    };
    bool first35 = true;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3), y = at(e, 1);
        if (k == k_peace || k == k_war_won) {
            const auto it = war_of_end.find(id);
            if (it == war_of_end.end()) continue;
            const std::int64_t d = it->second, a = x.role1(d, 1), b = x.role1(d, 2), g = at(*x.ev.at(d), 5);
            bool cost = false;
            for (const std::int64_t kid : x.kids(id)) cost = cost || is_cede(x.kind(kid)) || x.kind(kid) == k_tribute;
            if (cost && x.role1(id, 1) == a && !holds(g, a, y) && !holds(g, b, y) && !stands(g, b, y) &&
                last_battle_winner(x, d) != a) {
                out.push_back("(14) e" + std::to_string(id) + ": a cost after a lost goal's ending whose winner c" +
                              std::to_string(a) + " did not win the last battle");
            }
        }
        if (k == k_search_abandoned) {
            const auto it = war_of_end.find(id);
            if (it != war_of_end.end()) {
                for (const std::int64_t b : x.kids(it->second)) {
                    if ((x.kind(b) == k_battle || x.kind(b) == k_battle_at_pass) && at(*x.ev.at(b), 5) == at(e, 5)) {
                        out.push_back("21.7 item 2 e" + std::to_string(id) + ": its war fought battle e" +
                                      std::to_string(b) + " in the searched region");
                    }
                }
            }
            const std::int64_t want = first35 ? base35 : repeat_sal;
            if (at(e, 7) != want) {
                out.push_back("21.7 item 3 e" + std::to_string(id) + ": salience " + std::to_string(at(e, 7)) +
                              ", want " + std::to_string(want));
            }
            first35 = false;
            if (weary_years > 0) {
                const std::int64_t hid = x.role1(id, 2), r = at(e, 5);
                const std::vector<std::int64_t> ch = holder_change_years(t, r);
                for (const Row& z : t.event) {
                    if (at(z, 0) <= id || at(z, 5) != r || (!is_claim(at(z, 3)) && at(z, 3) != k_war_declared)) continue;
                    if (at(z, 1) >= y + weary_years || x.role1(at(z, 0), 2) != hid) continue;
                    bool changed = false;
                    for (const std::int64_t c : ch) changed = changed || (y <= c && c <= at(z, 1));
                    if (!changed) {
                        out.push_back("21.7 item 1 e" + std::to_string(at(z, 0)) + ": a claim or declaration over the "
                                      "region searched by e" + std::to_string(id));
                    }
                }
            }
        }
    }
    return out;
}

// P3-06 r17 22.3, 22.5 (P3-10 second round) from the rows, as the reference's replay_r17: settlement owners,
// standing and capitals replayed from the log (foundings; secessions passing the parent's standing settlements
// in the seceded regions; captures; ruins by sack or abandonment; cessions passing every standing settlement of
// `from` in the region, a foothold cession included; capital_moved; a refugees_settled is the founder's latest
// flight), region holders from region_control. Problems:
//   (8) restated: no peace or war_won whose loser won its war's last battle, unless its winner is the attacker
//       and holds the goal (region_control at the ending's year) with no standing settlement of the loser in it;
//   (15) restated: no colony_founded (founder), claim (claimant) or war_declared (attacker) by y in a year Y in
//       which y lies low: its latest flight (refuge h, year Y_f) has Y < Y_f + hidden_years of y's race and h is
//       standing and y's (by the replay just before the event);
//   (17) right after the cost of a peace or war_won won by the attacker W over L with goal G (the region_ceded or
//       tribute caused by the ending, if any): if W holds G, L has no standing settlement in G unless L's capital
//       is one of them;
//   the replay's owners and standing against table settlement, its capitals against table civilization.
// Facts: quick aim wins (an attacker's peace or war_won holding G with the loser out of G, no battle of the war,
// years <= 1), foothold cessions (a region_ceded with no region_control row of its event), and the wars declared
// over a region the attacker held (won by the attacker, by the defender, truce, open or otherwise).
struct R17 {
    std::vector<std::string> problems;
    std::int64_t quick_aim_wins = 0, foothold_cessions = 0;
    std::int64_t held_won_attacker = 0, held_won_defender = 0, held_truce = 0, held_other = 0;
    // `<quick> foothold_cessions <m> held_goal_wars <wa>/<wd>/<t>/<o>` as the golden file's r17 line after "r17 "
    [[nodiscard]] std::string line() const {
        return "quick_aim_wins " + std::to_string(quick_aim_wins) + " foothold_cessions " +
               std::to_string(foothold_cessions) + " held_goal_wars " + std::to_string(held_won_attacker) + "/" +
               std::to_string(held_won_defender) + "/" + std::to_string(held_truce) + "/" + std::to_string(held_other);
    }
};

inline R17 invariants_r17(const Tables& t, const wd::ContentSet& content) {
    const Index x(t);
    R17 out;
    const std::map<std::int64_t, std::int64_t> hidden_of_race = race_hidden_years(content);
    std::map<std::int64_t, std::int64_t> founded;  // founding event -> settlement
    for (const Row& s : t.settlement) founded[at(s, 7)] = at(s, 0);
    std::map<std::int64_t, std::vector<std::int64_t>> seceded_regions;
    std::set<std::int64_t> rc_events;
    for (const Row& rc : t.region_control) {
        seceded_regions[at(rc, 4)].push_back(at(rc, 0));
        rc_events.insert(at(rc, 4));
    }
    std::map<std::int64_t, std::int64_t> war_of_end;
    for (const Row& r : t.relation) {
        if (r[4].bytes == "war" && !is_null(r[6])) war_of_end[at(r, 8)] = at(r, 7);
    }
    // the owner of g in year y: the region_control row of g covering y with the largest from_year (and event),
    // rows opened in y by an event after `before` left out; 0 if none
    auto holder = [&t](std::int64_t g, std::int64_t y, std::int64_t before) {
        const Row* best = nullptr;
        for (const Row& rc : t.region_control) {
            if (at(rc, 0) != g || at(rc, 1) > y || (!is_null(rc[2]) && at(rc, 2) < y)) continue;
            if (at(rc, 1) == y && at(rc, 4) > before) continue;
            if (best == nullptr || at(rc, 1) > at(*best, 1) || (at(rc, 1) == at(*best, 1) && at(rc, 4) > at(*best, 4))) {
                best = &rc;
            }
        }
        return best == nullptr ? std::int64_t{0} : at(*best, 3);
    };
    std::map<std::int64_t, std::int64_t> owner, capital;
    std::map<std::int64_t, bool> standing;
    struct Flight {
        std::int64_t h, year;
    };
    std::map<std::int64_t, Flight> flights;
    auto mine_in = [&](std::int64_t L, std::int64_t g) {
        std::vector<std::int64_t> m;
        for (const auto& [s, o] : owner) {
            if (o == L && standing[s] && at(*x.st.at(s), 4) == g) m.push_back(s);
        }
        return m;
    };
    auto lies_low = [&](std::int64_t y, std::int64_t year) {
        const auto it = flights.find(y);
        if (it == flights.end()) return false;
        const std::int64_t hy = hidden_of_race.at(at(*x.cv.at(y), 2));
        const std::int64_t h = it->second.h;
        return year < it->second.year + hy && standing[h] && owner[h] == y;
    };
    struct Pending {
        std::int64_t w, l, g;
    };
    std::map<std::int64_t, Pending> pending17;
    std::map<std::int64_t, int> held_goal;  // war -> 0 other, 1 attacker won, 2 defender won, 3 truce
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), y = at(e, 1), k = at(e, 3);
        if (k == k_colony_founded && lies_low(x.role1(id, 2), y)) {
            out.problems.push_back("(15) e" + std::to_string(id) + ": colony_founded by c" +
                                   std::to_string(x.role1(id, 2)) + " while it lies low");
        }
        if (k == k_claim_settlements) {
            // P3-06 r19 23.7 item 3: every settlement participant stands in the event's region and is the holder's;
            // ascending id; the holder does not hold the region (23.2)
            const std::int64_t b = x.role1(id, 2);
            std::vector<std::int64_t> listed;
            for (const Row& p : t.event_participant) {
                if (at(p, 0) == id && at(p, 2) == 3) listed.push_back(at(p, 4));
            }
            bool ok = !listed.empty() && std::is_sorted(listed.begin(), listed.end());
            for (const std::int64_t s : listed) {
                ok = ok && standing[s] && owner[s] == b && at(*x.st.at(s), 4) == at(e, 5);
            }
            if (!ok) out.problems.push_back("(23.7.3) e" + std::to_string(id) + ": claim_settlements lists settlements "
                                            "that are not c" + std::to_string(b) + "'s in its region");
            if (holder(at(e, 5), y, id) == b) {
                out.problems.push_back("(23.2) e" + std::to_string(id) + ": claim_settlements on a region its holder holds");
            }
        }
        if ((is_claim(k) || k == k_war_declared) && lies_low(x.role1(id, 1), y)) {
            out.problems.push_back("(15) e" + std::to_string(id) + ": claim or war_declared by c" +
                                   std::to_string(x.role1(id, 1)) + " while it lies low");
        }
        if (k == k_war_declared && holder(at(e, 5), y, id) == x.role1(id, 1)) held_goal.emplace(id, 0);
        if (k == k_peace || k == k_war_won) {
            const auto it = war_of_end.find(id);
            if (it != war_of_end.end()) {
                const std::int64_t d = it->second, A = x.role1(d, 1), g = at(*x.ev.at(d), 5);
                const std::int64_t W = x.role1(id, 1), L = x.role1(id, 2);
                bool battles = false;
                for (const std::int64_t b : x.kids(d)) battles = battles || x.kind(b) == k_battle || x.kind(b) == k_battle_at_pass;
                const bool held = holder(g, y, id) == W, clear = mine_in(L, g).empty();
                if (last_battle_winner(x, d) == L && !(W == A && held && clear)) {
                    out.problems.push_back("(8) e" + std::to_string(id) + ": the loser c" + std::to_string(L) +
                                           " won the last battle of war e" + std::to_string(d) +
                                           ", and the winner is not the attacker holding the goal with the loser out of it");
                }
                const auto v = x.values.find(id);
                const std::int64_t years = v != x.values.end() && v->second.count(1) != 0 ? v->second.at(1) : 99;
                if (W == A && held && clear && !battles && years <= 1) ++out.quick_aim_wins;
                if (held_goal.count(d) != 0) held_goal[d] = W == A ? 1 : 2;
                if (W == A) pending17[id] = Pending{W, L, g};
            }
        }
        if (k == k_truce) {
            const auto it = war_of_end.find(id);
            if (it != war_of_end.end() && held_goal.count(it->second) != 0) held_goal[it->second] = 3;
        }
        // the event's changes
        if (const auto f = founded.find(id); f != founded.end()) {
            const std::int64_t s = f->second, c = at(*x.st.at(s), 5);
            owner[s] = c;
            standing[s] = true;
            capital.emplace(c, s);
        }
        if (k == k_civ_seceded) {
            const std::int64_t c2 = x.role1(id, 1), frm = x.role1(id, 2);
            std::set<std::int64_t> regs(seceded_regions[id].begin(), seceded_regions[id].end());
            regs.insert(at(e, 5));
            for (auto& [s, o] : owner) {
                if (o == frm && standing[s] && regs.count(at(*x.st.at(s), 4)) != 0) o = c2;
            }
            if (at(e, 6) != 0) {
                capital[c2] = at(e, 6);
            } else {
                for (const auto& [s, o] : owner) {
                    if (o == c2 && standing[s]) {
                        capital.emplace(c2, s);
                        break;
                    }
                }
            }
        } else if (k == k_settlement_captured) {
            owner[at(e, 6)] = x.role1(id, 1);
        } else if (k == k_sacked || k == k_settlement_abandoned || k == k_settlement_abandoned_to) {
            owner[at(e, 6)] = 0;
            standing[at(e, 6)] = false;
        } else if (k == k_capital_moved) {
            capital[x.role1(id, 1)] = at(e, 6);
        } else if (k == k_refugees_settled) {
            flights[x.role1(id, 2)] = Flight{at(e, 6), y};
        } else if (is_cede(k)) {
            const std::int64_t g = at(e, 5), L = x.role1(id, 1), W = x.role1(id, 2);
            if (k == k_settlements_ceded || rc_events.count(id) == 0) ++out.foothold_cessions;
            if (k == k_settlements_ceded) {
                // P3-06 r19 23.3, 23.7 item 3: the passing settlements, ascending, no region_control row
                std::vector<std::int64_t> listed;
                for (const Row& p : t.event_participant) {
                    if (at(p, 0) == id && at(p, 2) == 3) listed.push_back(at(p, 4));
                }
                std::vector<std::int64_t> passing = mine_in(L, g);
                if (passing.size() > 64U) passing.resize(64U);
                if (listed != passing) {
                    out.problems.push_back("(23.3) e" + std::to_string(id) + ": settlements_ceded does not list the "
                                           "standing settlements of c" + std::to_string(L) + " in its region");
                }
                if (rc_events.count(id) != 0) {
                    out.problems.push_back("(23.3) e" + std::to_string(id) + ": settlements_ceded opened a region_control row");
                }
            }
            for (const std::int64_t s : mine_in(L, g)) owner[s] = W;
        }
        // (17) once the ending's cost events (caused by it) are applied
        for (auto it = pending17.begin(); it != pending17.end();) {
            std::int64_t last = it->first;
            for (const std::int64_t c : x.kids(it->first)) {
                if (is_cede(x.kind(c)) || x.kind(c) == k_tribute) last = std::max(last, c);
            }
            if (last > id) {
                ++it;
                continue;
            }
            const Pending p = it->second;
            if (holder(p.g, x.year(it->first), last) == p.w) {
                const std::vector<std::int64_t> m = mine_in(p.l, p.g);
                if (!m.empty() && std::find(m.begin(), m.end(), capital[p.l]) == m.end()) {
                    out.problems.push_back("(17) e" + std::to_string(it->first) + ": c" + std::to_string(p.w) +
                                           " won and holds rg" + std::to_string(p.g) + ", but c" + std::to_string(p.l) +
                                           " still stands there");
                }
            }
            it = pending17.erase(it);
        }
    }
    for (const Row& s : t.settlement) {
        const std::int64_t id = at(s, 0);
        const bool st = at(s, 10) == 1 || at(s, 10) == 2;
        if (standing.count(id) == 0 || standing[id] != st || (st && owner[id] != at(s, 8))) {
            out.problems.push_back("replay: s" + std::to_string(id) + " owner or status differs from the table");
        }
    }
    for (const Row& c : t.civilization) {
        if (is_null(c[8]) && capital[at(c, 0)] != at(c, 5)) {
            out.problems.push_back("replay: c" + std::to_string(at(c, 0)) + " capital differs from the table");
        }
    }
    for (const auto& [d, v] : held_goal) {
        (v == 1 ? out.held_won_attacker : v == 2 ? out.held_won_defender : v == 3 ? out.held_truce : out.held_other) += 1;
    }
    return out;
}

// P3-06 r17 22.10 item 7 on one world or case: invariants (8), (15), (17) restated hold, and the replay's facts
// equal the reference's r17 line of the golden case (when the file has one).
// P3-06 r19 23.1-23.4 from the rows, as the reference's r19_facts: T11 (years from the orcs' awakening to the first
// orc colony_founded as founder, war_declared, sacked or settlement_captured as attacker; E - awakening year + 1
// without one; "-" without orcs), the first orc war_declared, kind 39 claims by the region's holder at the claim
// (the claimant, a third civilization, no one), kind 40 events, declarations whose region differs from their claim's.
inline std::string r19_line(const Tables& t, std::int64_t span) {
    const Index x(t);
    const std::int64_t ORC = 6;  // race id = order (P3-03)
    std::map<std::int64_t, std::int64_t> civ_race;
    for (const Row& c : t.civilization) civ_race[at(c, 0)] = at(c, 2);
    std::int64_t ya = -1, act = -1, war = -1;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3), y = at(e, 1);
        if (k == 1 && ya < 0 && x.role1(id, 1) == ORC) ya = y;
        const bool orc_act = (k == k_colony_founded && civ_race[x.role1(id, 2)] == ORC) ||
                             ((k == k_war_declared || k == k_sacked || k == k_settlement_captured) &&
                              civ_race[x.role1(id, 1)] == ORC);
        if (orc_act && act < 0) act = y;
        if (k == k_war_declared && civ_race[x.role1(id, 1)] == ORC && war < 0) war = y;
    }
    auto holder = [&](std::int64_t g, std::int64_t y, std::int64_t before) {
        const Row* best = nullptr;
        for (const Row& rc : t.region_control) {
            if (at(rc, 0) != g || at(rc, 1) > y || (!is_null(rc[2]) && at(rc, 2) < y)) continue;
            if (at(rc, 1) == y && at(rc, 4) > before) continue;
            if (best == nullptr || at(rc, 1) > at(*best, 1) || (at(rc, 1) == at(*best, 1) && at(rc, 4) > at(*best, 4))) best = &rc;
        }
        return best == nullptr ? std::int64_t{0} : at(*best, 3);
    };
    std::int64_t c_a = 0, c_third = 0, c_none = 0, ceded = 0, differs = 0;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        if (k == k_claim_settlements) {
            const std::int64_t h = holder(at(e, 5), at(e, 1), id);
            (h == x.role1(id, 1) ? c_a : h == 0 ? c_none : c_third) += 1;
        } else if (k == k_settlements_ceded) {
            ++ceded;
        } else if (k == k_war_declared) {
            std::int64_t cl = 0;
            for (const std::int64_t c : x.causes.at(id)) {
                if (is_claim(x.kind(c)) && (cl == 0 || c < cl)) cl = c;
            }
            if (cl != 0 && at(*x.ev.at(cl), 5) != at(e, 5)) ++differs;
        }
    }
    const std::string t11 = ya < 0 ? "-" : std::to_string(act >= 0 ? act - ya : span - ya + 1);
    const std::string fw = (ya < 0 || war < 0) ? "-" : std::to_string(war - ya);
    return "t11 " + t11 + " first_orc_war " + fw + " claims39 " + std::to_string(c_a) + "/" + std::to_string(c_third) +
           "/" + std::to_string(c_none) + " cessions40 " + std::to_string(ceded) + " goal_differs " +
           std::to_string(differs);
}

// --- P3-06 r22 24.7 (P5-01, generator 0.21.0) from the rows ------------------------------------------

// Invariants (18) to (21) from the rows, as the reference's r22_violations (and (21) whenever conflict_toll_spread
// holds, which the shipped and every golden tuning do). hidden_years: race id -> the race's hidden_years.
inline std::vector<std::string> invariants_22(const Tables& t, const std::map<std::int64_t, std::int64_t>& hidden_years,
                                              std::int64_t span) {
    const Index x(t);
    std::vector<std::string> out;
    std::map<std::int64_t, std::int64_t> civ_race, phase_kind;
    for (const Row& c : t.civilization) civ_race[at(c, 0)] = at(c, 2);
    for (const Row& p : t.phase) phase_kind[at(p, 0)] = at(p, 2);
    auto causes = [&](std::int64_t e) {
        const auto it = x.causes.find(e);
        return it == x.causes.end() ? std::vector<std::int64_t>{} : it->second;
    };
    auto e_ = [&](std::int64_t id) { return std::to_string(id); };
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        if (k == k_war_declared) {  // (18)
            std::vector<std::int64_t> cl;
            for (const std::int64_t c : causes(id)) {
                if (is_claim(x.kind(c))) cl.push_back(c);
            }
            if (cl.size() != 1U) {
                out.push_back("(18) e" + e_(id) + ": " + std::to_string(cl.size()) + " claim causes");
                continue;
            }
            const std::int64_t c0 = cl[0];
            if (at(*x.ev.at(c0), 5) != at(e, 5) || x.role1(c0, 1) != x.role1(id, 1) || x.role1(c0, 2) != x.role1(id, 2)) {
                out.push_back("(18) e" + e_(id) + ": its claim e" + e_(c0) + " differs in region, claimant or holder");
            }
        }
        if (is_claim(k)) {  // (18): a fresh claim is followed by the war_declared that cites it
            bool fresh = false;
            for (const std::int64_t c : causes(id)) fresh = fresh || is_claim(x.kind(c));
            if (fresh) {
                const auto nx = x.ev.find(id + 1);
                bool ok = nx != x.ev.end() && at(*nx->second, 3) == k_war_declared && at(*nx->second, 1) == at(e, 1);
                if (ok) {
                    const std::vector<std::int64_t> nc = causes(id + 1);
                    ok = std::find(nc.begin(), nc.end(), id) != nc.end();
                }
                if (!ok) out.push_back("(18) fresh claim e" + e_(id) + " is not followed by the declaration citing it");
            }
        }
        if (k == k_claim_dropped) {  // (19)
            const std::vector<std::int64_t> cs = causes(id);
            if (cs.size() != 1U || !is_claim(x.kind(cs[0]))) {
                out.push_back("(19) e" + e_(id) + ": not caused by one claim");
                continue;
            }
            const std::int64_t c0 = cs[0];
            if (at(*x.ev.at(c0), 5) != at(e, 5) || x.role1(c0, 1) != x.role1(id, 1) || x.role1(c0, 2) != x.role1(id, 2)) {
                out.push_back("(19) e" + e_(id) + ": its claim e" + e_(c0) + " differs in region, claimant or holder");
            }
            if (phase_kind[at(e, 4)] == 2) out.push_back("(19) e" + e_(id) + ": dated in a quiet step");
            for (const std::int64_t kid : x.kids(c0)) {
                if (kid == id) continue;
                if (x.kind(kid) == k_war_declared || is_claim(x.kind(kid))) {
                    out.push_back("(19) e" + e_(id) + ": its claim is cited by e" + e_(kid));
                }
                if (kid > id) out.push_back("(19) e" + e_(id) + ": its claim is cited afterwards by e" + e_(kid));
            }
        }
        if (k == k_refugees_settled) {  // (20)
            const std::int64_t y = x.role1(id, 2);
            const auto hy = hidden_years.find(civ_race[y]);
            const auto nx = x.ev.find(id + 1);
            const bool next42 = nx != x.ev.end() && at(*nx->second, 3) == k_refuge_hidden;
            if (hy != hidden_years.end() && hy->second >= 1) {
                bool ok = next42;
                if (ok) {
                    const Row& r = *nx->second;
                    ok = causes(id + 1) == std::vector<std::int64_t>{id} && x.role1(id + 1, 1) == x.role1(id, 1) &&
                         x.role1(id + 1, 2) == y && at(r, 5) == at(e, 5) && at(r, 6) == at(e, 6) && at(r, 1) == at(e, 1) &&
                         at(r, 2) == std::min(at(e, 1) + hy->second, span) - 1;
                }
                if (!ok) out.push_back("(20) e" + e_(id) + ": not followed by its refuge_hidden");
            } else if (next42) {
                out.push_back("(20) e" + e_(id) + ": a refuge_hidden for a race with hidden_years 0");
            }
        }
        if (k == k_refuge_hidden) {
            const auto pv = x.ev.find(id - 1);
            if (pv == x.ev.end() || at(*pv->second, 3) != k_refugees_settled || causes(id) != std::vector<std::int64_t>{id - 1}) {
                out.push_back("(20) e" + e_(id) + ": follows no refugees_settled");
            }
            if (!(at(e, 1) <= at(e, 2) && at(e, 2) <= span - 1)) out.push_back("(20) e" + e_(id) + ": its span");
        }
        if (k == k_battle || k == k_battle_at_pass) {  // (21)
            const auto& v = x.values.at(id);
            if (v.at(1) > v.at(2)) out.push_back("(21) e" + e_(id) + ": winner_dead above loser_dead");
        }
    }
    return out;
}

// The claims open at E by the rows (P3-07 24.7's tests with r19 25.9's (iv), as the reference's
// open_claims_from_rows): the last claim of each ordered pair, closed by a war_declared of the pair (either
// direction), a peace, truce, war_won or search_abandoned of the pair, or a claim_dropped citing it; both sides
// alive at E. "e<id>,..." or "-".
inline std::string open_claims_line(const Tables& t) {
    const Index x(t);
    std::set<std::int64_t> alive;
    for (const Row& c : t.civilization) {
        if (is_null(c[8])) alive.insert(at(c, 0));
    }
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> last;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        const std::int64_t a = x.role1(id, 1), b = x.role1(id, 2);
        if (is_claim(k)) {
            last[std::make_pair(a, b)] = id;
        } else if (k == k_war_declared) {
            last.erase(std::make_pair(a, b));
            last.erase(std::make_pair(b, a));
        } else if (k == k_claim_dropped) {
            const auto it = last.find(std::make_pair(a, b));
            const auto it_c = x.causes.find(id);
            if (it != last.end() && it_c != x.causes.end() &&
                std::find(it_c->second.begin(), it_c->second.end(), it->second) != it_c->second.end()) {
                last.erase(it);
            }
        } else if (k == k_peace || k == k_truce || k == k_war_won || k == k_search_abandoned) {
            std::set<std::int64_t> sides;
            for (const auto& [seq, ids] : x.roles.at(id)) {
                for (const std::int64_t c : ids) sides.insert(c);
            }
            if (sides.size() == 2U) {
                const std::int64_t p = *sides.begin(), q = *sides.rbegin();
                last.erase(std::make_pair(p, q));
                last.erase(std::make_pair(q, p));
            }
        }
    }
    std::vector<std::int64_t> open;
    for (const auto& [pr, e] : last) {
        if (alive.count(pr.first) != 0U && alive.count(pr.second) != 0U) open.push_back(e);
    }
    std::sort(open.begin(), open.end());
    std::string out;
    for (const std::int64_t e : open) out += (out.empty() ? "e" : ",e") + std::to_string(e);
    return out.empty() ? "-" : out;
}

// The refuges standing at E whose refuge_hidden ends at E - 1 (P3-07 r19 25.9 item 1's `; hidden`). "s<id>,..." or "-".
inline std::string hidden_refuges_line(const Tables& t, std::int64_t span) {
    const Index x(t);
    std::vector<std::int64_t> out;
    for (const Row& e : t.event) {
        if (at(e, 3) != k_refuge_hidden || at(e, 2) != span - 1) continue;
        const std::int64_t s = at(e, 6);
        const auto it = x.st.find(s);
        if (it != x.st.end() && (at(*it->second, 10) == 1 || at(*it->second, 10) == 2)) out.push_back(s);
    }
    std::sort(out.begin(), out.end());
    std::string r;
    for (const std::int64_t s : out) r += (r.empty() ? "s" : ",s") + std::to_string(s);
    return r.empty() ? "-" : r;
}

// The counts of the reference's r22 line without its last item (mixed_tolls reads rendered sentences; the golden
// sentences are compared on their own): "fresh <20>/<39> dropped <n> refuge_hidden <n> repeated_tolls <n>".
inline std::string r22_counts(const Tables& t) {
    const Index x(t);
    std::int64_t f20 = 0, f39 = 0, dropped = 0, hidden = 0, repeats = 0;
    std::set<std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t>> seen;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        if (is_claim(k)) {
            bool fresh = false;
            const auto it = x.causes.find(id);
            if (it != x.causes.end()) {
                for (const std::int64_t c : it->second) fresh = fresh || is_claim(x.kind(c));
            }
            if (fresh) (k == k_claim ? f20 : f39) += 1;
        }
        dropped += k == k_claim_dropped ? 1 : 0;
        hidden += k == k_refuge_hidden ? 1 : 0;
        if (k == k_battle || k == k_battle_at_pass) {
            const auto& v = x.values.at(id);
            const auto key = std::make_tuple(x.role1(id, 1), x.role1(id, 2), v.at(1), v.at(2));
            repeats += seen.count(key) != 0U ? 1 : 0;
            seen.insert(key);
        }
    }
    return "fresh " + std::to_string(f20) + "/" + std::to_string(f39) + " dropped " + std::to_string(dropped) +
           " refuge_hidden " + std::to_string(hidden) + " repeated_tolls " + std::to_string(repeats);
}

inline void check_r22(const Tables& t, const wd::ContentSet& c, const CCase& g) {
    CHECK(invariants_22(t, race_hidden_years(c), g.span) == std::vector<std::string>{});
    CHECK_MESSAGE(!g.r22.empty(), "the golden case has no r22 line (P5-01)");
    if (g.r22.empty()) return;
    const std::string counts = g.r22.substr(0, g.r22.find(" mixed_tolls "));
    CHECK(r22_counts(t) == counts);
    CHECK(g.r22.substr(g.r22.find(" mixed_tolls ") + 13) == "0");   // T16's second count (P3-04 r19 22.1)
    CHECK(open_claims_line(t) == g.open_claims);         // (19), T19: the rows close what the state closes
    CHECK(hidden_refuges_line(t, g.span) == g.hidden_refuges);
}

inline void check_r17(const Tables& t, const wd::ContentSet& c, const CCase& g) {
    const R17 r = invariants_r17(t, c);
    CHECK(r.problems == std::vector<std::string>{});
    CHECK_MESSAGE(!g.r17.empty(), "the golden case has no r17 line (P3-10 second round)");
    if (!g.r17.empty()) CHECK(r.line() == g.r17);
    // P3-11: the r19 facts (T11, kinds 39 and 40, war goals) equal the reference's
    CHECK_MESSAGE(!g.r19.empty(), "the golden case has no r19 line (P3-11)");
    if (!g.r19.empty()) CHECK(r19_line(t, g.span) == g.r19);
    check_r22(t, c, g);   // P5-01 (P3-06 r22 24.7, T16, T19)
}

}  // namespace conflict_test

#if UNMAPPED_HAVE_CONFLICT

namespace conflict_test {

// The shipped configuration (peoples, conflict; conflict the tension source) with probe subsystems before
// peoples, between peoples and conflict, and after conflict.
struct ConflictConfig {
    std::unique_ptr<cfl::Conflict> pair;
    std::vector<std::unique_ptr<FnSubsystem>> before, between, after;
    std::unique_ptr<eng::InterventionSource> interventions = std::make_unique<eng::EmptyInterventionSource>();
    eng::EventCatalogue catalogue;

    explicit ConflictConfig(const wd::ContentSet& c)
        : pair(std::make_unique<cfl::Conflict>(c)), catalogue(eng::catalogue_of(c)) {}
    void add_before(std::vector<eng::Step> steps, FnSubsystem::Fn fn) {
        before.push_back(std::make_unique<FnSubsystem>(std::move(steps), std::move(fn)));
    }
    void add_between(std::vector<eng::Step> steps, FnSubsystem::Fn fn) {
        between.push_back(std::make_unique<FnSubsystem>(std::move(steps), std::move(fn)));
    }
    void add_after(std::vector<eng::Step> steps, FnSubsystem::Fn fn) {
        after.push_back(std::make_unique<FnSubsystem>(std::move(steps), std::move(fn)));
    }
    [[nodiscard]] eng::EngineConfig get() const {
        eng::EngineConfig c{};
        c.awakening = pair->peoples.awakening.get();
        for (const auto& s : before) c.subsystems.push_back(s.get());
        c.subsystems.push_back(pair->peoples.sub.get());
        for (const auto& s : between) c.subsystems.push_back(s.get());
        c.subsystems.push_back(pair->subsystem());
        for (const auto& s : after) c.subsystems.push_back(s.get());
        c.tension.push_back(pair->tension());
        c.interventions = interventions.get();
        c.catalogue = &catalogue;
        return c;
    }
};

}  // namespace conflict_test

#endif  // UNMAPPED_HAVE_CONFLICT

#endif  // UNMAPPED_HAVE_PEOPLES
