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
    std::int64_t decl = 0, hollow = 0;
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
            changed = changed || x.kind(u) == k_civ_conquered;
            for (const std::int64_t k : x.kids(u)) changed = changed || x.kind(k) == k_region_ceded || x.kind(k) == k_tribute;
        }
        hollow += changed ? 0 : 1;
    }
    std::int64_t same_max = 0, unchanged_max = 0;
    for (const auto& [key, ds] : groups) {
        same_max = std::max<std::int64_t>(same_max, static_cast<std::int64_t>(ds.size()));
        std::vector<std::int64_t> changes;
        for (const Row& rc : t.region_control) {
            if (at(rc, 0) != key.second) continue;
            changes.push_back(at(rc, 1));
            if (!is_null(rc[2])) changes.push_back(at(rc, 2));
        }
        std::int64_t run = 0;
        for (const std::int64_t d : ds) {  // id order
            const Row& r = *war_row.at(d);
            const std::int64_t lo = at(r, 5), hi = is_null(r[6]) ? E : at(r, 6);
            bool moved = false;
            for (const std::int64_t y : changes) moved = moved || (lo <= y && y <= hi);
            run = moved ? 0 : run + 1;
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
    std::int64_t pshare = 0, rshare = 0;
    for (const db::RaceRow& r : in.races) {
        if (pop > 0) pshare = std::max(pshare, pop_r[r.id] * 1000 / pop);
        if (held > 0) rshare = std::max(rshare, held_r[r.id] * 1000 / held);
    }
    return {"wars_hollow = " + std::to_string(hollow),
            "wars_hollow_permille = " + std::to_string(decl > 0 ? 1000 * hollow / decl : 0),
            "wars_same_goal_max = " + std::to_string(same_max),
            "wars_same_goal_unchanged_max = " + std::to_string(unchanged_max),
            "race_population_share_max_permille = " + std::to_string(pshare),
            "race_regions_share_max_permille = " + std::to_string(rshare)};
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
            if (x.kind(c) == k_claim) claim = c;
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
            if (x.kind(u) == k_peace) {
                outcome = "won-c" + std::to_string(x.role1(u, 1));
                for (const std::int64_t k : x.kids(u)) {
                    if (x.kind(k) == k_region_ceded) outcome += "-ceded";
                }
                for (const std::int64_t k : x.kids(u)) {
                    if (x.kind(k) == k_tribute) outcome += "-tribute";  // r9 20.3 (0.18.0)
                }
            } else if (x.kind(u) == k_truce) {
                outcome = "truce";
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
                CHECK((uk == k_peace || uk == k_truce || uk == 18 || uk == k_civ_conquered));
                // 7.3: no war ends in its declaration year by peace or truce; a side conquered by another
                // war of that year's T6 closes it with since = until (8.3 item 7, P3-04 6.7 r9)
                CHECK((at(r, 6) > at(r, 5) || uk == 18 || uk == k_civ_conquered));
            }
            war_row[at(r, 7)] = &r;
        } else {
            CHECK(a < b);
            CHECK((sk == k_peace || sk == k_truce));
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
                claimed = claimed || (x.kind(c) == k_claim && x.role1(c, 1) == x.role1(id, 1) &&
                                      x.role1(c, 2) == x.role1(id, 2) && x.year(c) <= at(e, 1));
            }
            CHECK(claimed);
        }
        if (k == k_battle || k == k_battle_at_pass || k == k_siege || k == k_sacked || k == k_settlement_captured) {
            CHECK_FALSE(quiet);
        }
        if ((k == k_peace || k == k_truce || k == k_region_ceded || k == k_tribute) && quiet) {  // r9: tribute
            CHECK((at(e, 1) - at(*x.ph.at(at(e, 4)), 3)) % quiet_step_years == 0);
        }
        if (k == k_battle || k == k_battle_at_pass) {
            // r7 7.3 item 4 (0.17.0): with the shipped loss_winner 80 <= loss_loser 250 the victors never lose
            // more than the defeated (dead_W from the smaller force)
            CHECK(x.values.at(id).at(1) <= x.values.at(id).at(2));
            REQUIRE(x.causes.at(id).size() == 1U);
            CHECK(x.kind(x.causes.at(id)[0]) == k_war_declared);
        }
        if (k == k_claim) {  // 8.3 item 4 (N2): a colony of the same step is no claim cause, unless it is
                             // the claimed region's owner_event (its ownership is live state)
            for (const std::int64_t c : x.causes.at(id)) {
                if (x.kind(c) != k_colony_founded || x.year(c) != at(e, 1)) continue;
                bool owner_event = false;
                for (const Row& rc : t.region_control) owner_event = owner_event || (at(rc, 0) == at(e, 5) && at(rc, 4) == c);
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
        if (at(p, 2) == 3) CHECK(x.kind(at(p, 5)) == k_claim);
    }
    // 8.3 item 4 (N2): a cession in a quiet step never transfers a colony founded earlier in the same step
    for (const Row& e : t.event) {
        if (at(e, 3) != k_region_ceded || at(*x.ph.at(at(e, 4)), 2) != 2) continue;
        const std::int64_t to = x.role1(at(e, 0), 2);
        for (const Row& c : t.settlement_census) {
            if (at(c, 1) != at(e, 1) || at(c, 2) != to) continue;
            const Row& s = *x.st.at(at(c, 0));
            if (at(s, 4) != at(e, 5)) continue;
            CHECK_MESSAGE(at(s, 7) < at(e, 0), "settlement " << at(s, 0));
            CHECK_FALSE((x.kind(at(s, 7)) == k_colony_founded && at(s, 6) == at(e, 1)));
        }
    }
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

// 20.13 item 5, sound and conservative from the rows (the reference's weary_violations): per (defender, region)
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
    std::map<std::int64_t, std::vector<std::int64_t>> changes;
    for (const Row& rc : t.region_control) {
        changes[at(rc, 0)].push_back(at(rc, 1));
        if (!is_null(rc[2])) changes[at(rc, 0)].push_back(at(rc, 2));
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
        if (x.kind(u) != k_peace && x.kind(u) != k_truce) continue;
        ends[{x.role1(at(e, 0), 2), at(e, 5)}].push_back({u, at(e, 1), at(*it->second, 6)});
    }
    auto any_in = [&](std::int64_t g, std::int64_t lo, std::int64_t hi) {
        for (const std::int64_t y : changes[g]) {
            if (lo <= y && y <= hi) return true;
        }
        return false;
    };
    for (const Row& e : t.event) {
        if (at(e, 3) != k_claim && at(e, 3) != k_war_declared) continue;
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
        if (run >= weary && last >= 0 && !any_in(g, last, at(e, 1))) out.push_back(at(e, 0));
    }
    return out;
}

// Items 3, 4 and 6 of 20.13, the cost rule of 20.3 and P3-09 10 (every won war costs the loser a region or a
// tribute, except a last refuge's or when it has no visible people), with `hidden_years`, `abandon_min`,
// `capital_sacked` and `weary` the tuning's values.
inline void check_invariants_09(const Tables& t, const eng::HistoryInputs& in, const wd::ContentSet& content) {
    const Index x(t);
    const std::int64_t hidden_years = conflict_int(content, "hidden_years");
    const std::int64_t capital_sacked = conflict_int(content, "capital_sacked_salience");
    std::int64_t abandon_min = 0;
    for (const wd::KindContent& k : content.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == "peoples") abandon_min = d.record.get_int("abandon_min");
        }
    }
    std::map<std::int64_t, std::string> race_key;
    for (const db::RaceRow& r : in.races) race_key[r.id] = r.key;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3), y = at(e, 1);
        CAPTURE(id);
        if (k == k_refugees_settled) {
            const std::int64_t h = x.role1(id, 1), founder = x.role1(id, 2), origin = x.role1(id, 3);
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
                } else if (ok == k_claim || ok == k_war_declared) {
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
            CHECK(x.kind(p) == k_peace);
            CHECK(x.role1(p, 2) == x.role1(id, 1));  // the loser pays
            CHECK(x.role1(p, 1) == x.role1(id, 2));  // to the winner
            CHECK(x.values.at(id).at(1) >= 1);
            CHECK(at(e, 5) == 0);
        }
        if (k == k_peace || k == k_truce) {
            std::int64_t costs = 0;
            for (const std::int64_t kid : x.kids(id)) costs += (x.kind(kid) == k_region_ceded || x.kind(kid) == k_tribute) ? 1 : 0;
            CHECK(costs <= (k == k_peace ? 1 : 0));  // one cession or one tribute per peace; a truce costs nothing
            if (k == k_peace && costs == 0) {
                // the exceptions of 20.3 / P3-09 10: the loser was its race's last alive civilization (a last
                // refuge), or its people were all out of sight (concealed: a race that can be concealed)
                const std::int64_t loser = x.role1(id, 2), race = at(*x.cv.at(loser), 2);
                bool alone = true;
                for (const Row& c : t.civilization) {
                    if (at(c, 0) == loser || at(c, 2) != race || at(c, 6) > y) continue;
                    alone = alone && !(is_null(c[8]) || at(c, 8) > y || (at(c, 8) == y && at(c, 9) > id));
                }
                CHECK_MESSAGE((alone || race_field(content, race_key.at(race), "conceal_min") <= 1000),
                              "peace e" << id << " won without a cession or tribute");
            }
        }
        if (k == k_siege) {  // item 6: never a concealed settlement
            const std::int64_t def = x.role1(id, 2);
            const std::int64_t race = at(*x.cv.at(def), 2);
            const std::int64_t region = at(*x.st.at(at(e, 6)), 4);
            CHECK_MESSAGE(biome_term(in, region, race) < race_field(content, race_key.at(race), "conceal_min"),
                          "siege e" << id << " of a concealed settlement");
        }
    }
    // item 5 from the log
    CHECK(weary_violations(t, conflict_int(content, "weary_wars")) == std::vector<std::int64_t>{});
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
        if (at(e, 3) == k_claim) {
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
        const bool era = at(a, 3) == k_quiet_era && at(b, 3) == k_quiet_era && at(a, 1) == at(b, 1) &&
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
