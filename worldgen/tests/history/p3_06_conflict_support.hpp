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
                CHECK(at(r, 6) > at(r, 5));  // 7.3: no war ends in its declaration year (N5)
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
        if ((k == k_peace || k == k_truce || k == k_region_ceded) && quiet) {
            CHECK((at(e, 1) - at(*x.ph.at(at(e, 4)), 3)) % quiet_step_years == 0);
        }
        if (k == k_battle || k == k_battle_at_pass) {
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
        if (at(s, 10) == ruined) CHECK((x.kind(at(s, 13)) == k_settlement_abandoned || x.kind(at(s, 13)) == k_sacked));
    }
    // 5 territory
    for (const Row& r : t.region_control) {
        const std::int64_t k = x.kind(at(r, 4));
        CHECK((k == 2 || k == k_colony_founded || k == k_civ_seceded || k == k_settlement_captured || k == k_region_ceded));
    }
    // 7 races
    if (last_refuge) {
        for (const db::AgeRow& a : in.ages) {
            bool alive = false;
            for (const Row& c : t.civilization) alive = alive || (at(c, 2) == a.race && is_null(c[8]));
            CHECK_MESSAGE(alive, "race " << a.race << " has no alive civilization");
        }
    }
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
