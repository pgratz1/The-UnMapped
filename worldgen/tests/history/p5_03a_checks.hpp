#pragma once

// The invariants of spec P5-03a (docs/specs/p5-03a-relations-followups.md r1) 14 item 12 on a world's rows, on top
// of P5-03's (p5_03_relations_checks.hpp, which holds the amended invariant 4 of 12 item 1 and the arrival causes
// of 7.3):
//   (a) a warmer turn (to a higher band) cites only positive contributions (refugees_arrived, war_declared,
//       monster_raid, settlement_overrun) or, alone, its pair's meeting; a colder turn cites negative contributions
//       (6.2.2's war events and people_expelled), else its last turn, else its meeting (3.5);
//   (b) two expulsions by one realm are at least expel_gap_years apart (5.3);
//   (c) a peoples_met with sealed 1 has no open path within meet_hops and a border path within meet_sealed_hops
//       between the two peoples' presences (4.2). Read from the rows: a presence at year Y is the regions held by
//       an alive civilization of the race (region_control in force at Y) and the regions of its standing
//       settlements (their census rows up to Y); reading C1: the rows hold the state after the step, which is the
//       state the relations step reads (it is the step's last subsystem), up to civilizations and settlements made
//       in the same step (not yet visible to it);
//   10.4's value ranges (sealed, standing, majority, years); 5.1's settlers on an owner change to another people.

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_checks.hpp"

namespace p503a_check {

using p503check::at;
using p503check::Index;
using p503check::is_null;
using p503check::n;
using p503check::Pair;
using p503check::World;
using Row = eng::Row;

// Hops over every border (4.1's border graph), from region r0.
inline std::map<std::int64_t, std::int64_t> border_hops(const World& w, std::int64_t r0) {
    std::map<std::int64_t, std::vector<std::int64_t>> nb;
    for (const p503check::Border& b : w.borders) {
        nb[b.a].push_back(b.b);
        nb[b.b].push_back(b.a);
    }
    std::map<std::int64_t, std::int64_t> d{{r0, 0}};
    std::vector<std::int64_t> fifo{r0};
    for (std::size_t i = 0; i < fifo.size(); ++i) {
        std::vector<std::int64_t> ns = nb[fifo[i]];
        std::sort(ns.begin(), ns.end());
        for (const std::int64_t y : ns) {
            if (d.count(y) == 0U) {
                d[y] = d[fifo[i]] + 1;
                fifo.push_back(y);
            }
        }
    }
    return d;
}

// The regions present for race r at the end of year y (reading C1).
inline std::set<std::int64_t> presence(const World& w, const Index& x, std::int64_t r, std::int64_t y) {
    const eng::Tables& t = w.rows.t;
    std::set<std::int64_t> civs;
    for (const Row& c : t.civilization) {
        if (at(c, 2) == r && at(c, 6) <= y && (is_null(c.at(8)) || c.at(8).integer > y)) civs.insert(at(c, 0));
    }
    std::set<std::int64_t> out;
    for (const Row& rc : t.region_control) {
        if (civs.count(at(rc, 3)) != 0U && at(rc, 1) <= y && (is_null(rc.at(2)) || rc.at(2).integer > y)) {
            out.insert(at(rc, 0));
        }
    }
    std::map<std::int64_t, const Row*> last;
    for (const Row& cr : t.settlement_census) {
        if (at(cr, 1) <= y) last[at(cr, 0)] = &cr;
    }
    for (const auto& [s, cr] : last) {
        if ((at(*cr, 4) == 1 || at(*cr, 4) == 2) && civs.count(at(*cr, 2)) != 0U && x.stl.count(s) != 0U) {
            out.insert(at(*x.stl.at(s), 4));
        }
    }
    return out;
}

inline std::vector<std::string> violations(const World& w) {
    std::vector<std::string> out;
    const Index x(w);
    const eng::Tables& t = w.rows.t;
    auto rel = [&](const char* f) { return p503api::tuning_int(*w.content, "relations", f); };
    // P5-04 6.9 (0.26.0): the credits of alliances, trade and arms are contributions too (the reference's
    // POSITIVE_KINDS_025 and NEGATIVE_KINDS_025).
    static const std::set<std::string> positive = {"refugees_arrived", "war_declared", "monster_raid",
                                                   "settlement_overrun", "alliance_formed", "alliance_renewed",
                                                   "ally_joined_war", "spoils_shared", "arms_sold", "trade_opened"};
    static const std::set<std::string> negative = {"war_won", "peace", "truce", "civ_conquered", "sacked",
                                                   "settlement_captured", "people_expelled", "ally_refused",
                                                   "alliance_betrayed", "spoils_withheld", "ally_withdrew",
                                                   "search_abandoned", "civ_ended"};
    // (a) and 3.5
    std::map<Pair, std::int64_t> met, band;
    for (const std::int64_t e : x.events_of("peoples_met")) {
        const Pair p{x.part(e, 1), x.part(e, 2)};
        met[p] = e;
        band[p] = x.val(e, 1);
        const std::int64_t sealed = x.val(e, 2);
        if (sealed != 0 && sealed != 1) out.push_back("10.4: peoples_met e" + n(e) + " sealed " + n(sealed));
    }
    for (const std::int64_t e : x.events_of("disposition_turned")) {
        const Pair p{x.part(e, 1), x.part(e, 2)};
        if (met.count(p) == 0U) continue;   // P5-03's invariant 1 reports it
        const std::int64_t nb = x.val(e, 1);
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (nb > band[p]) {
            const bool only_meeting = cs.size() == 1U && cs[0] == met[p];
            bool all_positive = !cs.empty();
            for (const std::int64_t c : cs) all_positive = all_positive && positive.count(x.kind_of(c)) != 0U;
            if (!only_meeting && !all_positive) out.push_back("12a: warmer turn e" + n(e) + " cites other causes");
        } else {
            bool ok = !cs.empty();
            for (const std::int64_t c : cs) {
                const std::string k = x.kind_of(c);
                ok = ok && (negative.count(k) != 0U || k == "disposition_turned" || k == "peoples_met");
            }
            if (!ok) out.push_back("3.5: colder turn e" + n(e) + " cites a positive contribution or none");
        }
        band[p] = nb;
    }
    // (b) and 10.4 of kind 62
    const std::int64_t gap = rel("expel_gap_years");
    std::map<std::int64_t, std::int64_t> last_expel;
    for (const std::int64_t e : x.events_of("people_expelled")) {
        const std::int64_t c = x.part(e, 2), y = at(*x.ev.at(e), 1);
        if (const auto it = last_expel.find(c); it != last_expel.end() && y - it->second < gap) {
            out.push_back("12b: realm c" + n(c) + " expels at " + n(it->second) + " and " + n(y));
        }
        last_expel[c] = y;
        const std::int64_t st = x.val(e, 1), yrs = x.val(e, 2);
        if (st != 0 && st != 1) out.push_back("10.4: people_expelled e" + n(e) + " standing " + n(st));
        if (yrs < 0) out.push_back("10.4: people_expelled e" + n(e) + " years " + n(yrs));
        if (static_cast<std::int64_t>(x.vals.count(e) != 0U ? x.vals.at(e).size() : 0U) != 3) {
            out.push_back("10.4: people_expelled e" + n(e) + " has not three values");
        }
    }
    // 10.4 of kind 60
    for (const std::int64_t e : x.events_of("settlement_mixed")) {
        const std::int64_t st = x.val(e, 1), mj = x.val(e, 2);
        if (st < 0 || st > 2) out.push_back("10.4: settlement_mixed e" + n(e) + " standing " + n(st));
        if (mj != 0 && mj != 1) out.push_back("10.4: settlement_mixed e" + n(e) + " majority " + n(mj));
        if (static_cast<std::int64_t>(x.vals.count(e) != 0U ? x.vals.at(e).size() : 0U) != 3) {
            out.push_back("10.4: settlement_mixed e" + n(e) + " has not three values");
        }
    }
    // (c)
    const std::int64_t meet_hops = rel("meet_hops"), sealed_hops = rel("meet_sealed_hops");
    for (const auto& [p, e] : met) {
        if (x.val(e, 2) != 1) continue;
        if (sealed_hops < 0) {
            out.push_back("12c: e" + n(e) + " met by word with meet_sealed_hops -1");
            continue;
        }
        const std::int64_t y = at(*x.ev.at(e), 1);
        const std::set<std::int64_t> pa = presence(w, x, p.first, y), pb = presence(w, x, p.second, y);
        std::optional<std::int64_t> open_min, border_min;
        for (const std::int64_t ra : pa) {
            const auto ho = x.hops(ra);
            const auto hb = border_hops(w, ra);
            for (const std::int64_t rb : pb) {
                if (const auto it = ho.find(rb); it != ho.end() && (!open_min || it->second < *open_min)) {
                    open_min = it->second;
                }
                if (const auto it = hb.find(rb); it != hb.end() && (!border_min || it->second < *border_min)) {
                    border_min = it->second;
                }
            }
        }
        if (open_min && meet_hops >= 0 && *open_min <= meet_hops) {
            out.push_back("12c: sealed meeting e" + n(e) + " with an open path of " + n(*open_min) + " hops");
        }
        if (!border_min || *border_min > sealed_hops) {
            out.push_back("12c: sealed meeting e" + n(e) + " beyond meet_sealed_hops");
        }
    }
    // 5.1: on an owner change to another people (census), the new rulers' share is x + (1000 - x) sigma div 1000
    // with sigma in [least, least + span]; checked where nothing else changed the settlement that year
    const std::int64_t least = rel("conquest_settlers_permille"), span = rel("conquest_settlers_span_permille");
    std::set<std::int64_t> phase_starts;
    for (const Row& ph : t.phase) phase_starts.insert(at(ph, 3));
    const auto srows = p503check::share_rows(w);
    std::map<std::int64_t, std::vector<const Row*>> census;
    for (const Row& cr : t.settlement_census) census[at(cr, 0)].push_back(&cr);
    for (const auto& [s, cs] : census) {
        for (std::size_t i = 1; i < cs.size(); ++i) {
            const std::int64_t y = at(*cs[i], 1), o0 = at(*cs[i - 1], 2), o1 = at(*cs[i], 2);
            const std::int64_t q0 = x.civ_race(o0), q1 = x.civ_race(o1);
            if (o1 == 0 || q1 == 0 || q0 == q1 || phase_starts.count(y) != 0U || srows.count(s) == 0U) continue;
            bool other = false;
            for (const auto& [id, row] : x.ev) {
                const std::string k = x.kind_of(id);
                other = other || (at(*row, 1) == y && at(*row, 6) == s &&
                                  (k == "refugees_arrived" || k == "people_expelled" || k == "colony_founded"));
                // people who fled a sack, an overrun or an abandonment elsewhere that year may have reached s before
                // the owner change through the ledger (P5-03 6.4.1) without an event at s: the rows of y - 1 are then
                // not the shares the settlers' rule was applied to (integration of P5-03a: s116 of seed 42 at 1024
                // took 2,008 people of Edemford's sack, mostly of the conquerors' race, before the cession of 5821)
                other = other || (at(*row, 1) == y && (k == "sacked" || k == "settlement_overrun" ||
                                                       k == "settlement_fled" || k == "settlement_abandoned" ||
                                                       k == "settlement_abandoned_to"));
            }
            if (other) continue;
            const auto before = p503check::in_force(srows.at(s), y - 1), after = p503check::in_force(srows.at(s), y);
            if (before.empty() || after.count(q1) == 0U) continue;
            const std::int64_t xq = before.count(q1) != 0U ? before.at(q1).first : 0;
            const std::int64_t sq = after.at(q1).first;
            const std::int64_t lo = xq + (1000 - xq) * least / 1000, hi = xq + (1000 - xq) * (least + span) / 1000;
            if (sq < lo || sq > hi) {
                out.push_back("5.1: s" + n(s) + " taken in " + n(y) + ": the new rulers' share " + n(sq) +
                              " outside " + n(lo) + " to " + n(hi));
            }
        }
    }
    return out;
}

}  // namespace p503a_check
