#pragma once

// P5-07 (docs/specs/p5-07-mountain-depths.md r2): the formulas of 6.2 to 6.7 and 6.3, (r2) 6.10 and 6.11, as the spec
// writes them (p507_f, checked on hand values in history_depths_unit_test.cpp), and the invariants of 13 items 1 to 9
// and (r2) 12 to 15 that rows show (p507_check::violations), the statistics of 10 item 7 and the derived depths of 9
// (p507_check::stats, mine_depth_at, warren_depth_at), ported from the reference
// (reference/p5_07_depths_reference.py: violations, outcome_violations, warren_violations, aid_violations,
// stats_of_tables, mine_depth_at, warren_depth_at). Messages name the item first. No library name beyond P5-03's
// tests'.

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p5_03_relations_checks.hpp"
#include "p5_07_api.hpp"

namespace p507_f {

// 6.2: a mine at depth d (0 to 2) may reach d + 1 once Y - origin_year >= deep_years[d].
inline bool can_delve(std::int64_t Y, std::int64_t origin, std::int64_t d, const std::vector<std::int64_t>& years) {
    return d >= 0 && d < 3 && Y - origin >= years[static_cast<std::size_t>(d)];
}
// 6.2: p = deep_delve_permille x mining_permille div 1000 (T4); Q4: min(1000, p x L) (L = 0: T4).
inline std::int64_t delve_permille(std::int64_t delve, std::int64_t mining, std::int64_t quiet_length = 0) {
    std::int64_t p = delve * mining / 1000;
    if (quiet_length > 0) p = std::min<std::int64_t>(1000, p * quiet_length);
    return p;
}
// 6.4 item 2: p = min(10^6, deep_wake_ppm[d - 1] x M div 1000).
inline std::int64_t wake_ppm(const std::vector<std::int64_t>& ppm, std::int64_t d, std::int64_t M) {
    return std::min<std::int64_t>(1000000, ppm[static_cast<std::size_t>(d - 1)] * M / 1000);
}
// 6.5: S = P_t x levy div 1000 x prowess div 1000; odds = min(cap, S x 1000 div (S + might)), S = 0: 0.
inline std::int64_t gate_strength(std::int64_t P, std::int64_t levy, std::int64_t prowess) {
    return P * levy / 1000 * prowess / 1000;
}
inline std::int64_t gate_odds(std::int64_t S, std::int64_t might, std::int64_t cap) {
    if (S == 0) return 0;
    return std::min(cap, S * 1000 / (S + might));
}
inline bool sealed_at(std::int64_t u, std::int64_t odds) { return u < odds; }
// 6.6 item 1: (dead, survivors).
inline std::pair<std::int64_t, std::int64_t> take_toll(std::int64_t P, std::int64_t toll) {
    const std::int64_t dead = P * toll / 1000;
    return {dead, P - dead};
}
// 6.7: base x f_p div 1000, base = min(1000, overrun_permille x M div 1000), f_p = min(1000, ref x 1000 div max(1, P)).
inline std::int64_t take_permille(std::int64_t overrun, std::int64_t M, std::int64_t ref, std::int64_t P) {
    const std::int64_t base = std::min<std::int64_t>(1000, overrun * M / 1000);
    const std::int64_t fp = std::min<std::int64_t>(1000, ref * 1000 / std::max<std::int64_t>(1, P));
    return base * fp / 1000;
}
// 6.7's conditions before the draw.
inline bool may_take_later(std::int64_t takes, std::int64_t takes_max, std::int64_t hops, std::int64_t reach,
                           std::int64_t owner, std::int64_t waking_realm, bool realm_alive, bool sealed) {
    return !sealed && owner == waking_realm && realm_alive && hops >= 0 && hops <= reach && takes <= takes_max;
}
// 6.3 item 1: f x (1000 + forge_strength + forge_depth x D) div 1000 when forged, else f.
inline std::int64_t forge_factor(std::int64_t f, std::int64_t strength, std::int64_t depth_permille, std::int64_t D,
                                 bool forged) {
    return forged ? f * (1000 + strength + depth_permille * D) / 1000 : f;
}
// 8.1: telling, a function of ids.
inline std::int64_t telling_delve(std::int64_t site, std::int64_t depth) { return (site + depth) % 3; }
inline std::int64_t telling_woken(std::int64_t monster) { return monster % 3; }
inline std::int64_t telling_sealed(std::int64_t monster) { return (monster + 1) % 3; }
inline std::int64_t telling_lost(std::int64_t monster, std::int64_t settlement) { return (monster + settlement) % 3; }
// r2 6.10 (4.2): a warren's region: upland (4 x walk_cells < cells) or ore_reach_cells >= deep_warren_ore_min; 0: any.
inline bool warren_region_ok(std::int64_t cells, std::int64_t walk, std::int64_t ore, std::int64_t ore_min) {
    return ore_min == 0 || 4 * walk < cells || ore >= ore_min;
}
// r2 6.10 item 2: p = min(10^6, deep_wake_ppm[d - 1] x M div 1000 x deep_warren_wake_permille div 1000).
inline std::int64_t warren_wake_ppm(const std::vector<std::int64_t>& ppm, std::int64_t d, std::int64_t M,
                                    std::int64_t warren) {
    return std::min<std::int64_t>(1000000, ppm[static_cast<std::size_t>(d - 1)] * M / 1000 * warren / 1000);
}
// r2 6.11: p = deep_aid_permille x ally_join_permille div 1000; S_a = P_q x levy div 1000 x prowess div 1000.
inline std::int64_t aid_permille(std::int64_t aid, std::int64_t join) { return aid * join / 1000; }
inline std::int64_t aid_strength(std::int64_t P, std::int64_t levy, std::int64_t prowess) {
    return P * levy / 1000 * prowess / 1000;
}
inline std::int64_t telling_warren_delved(std::int64_t settlement, std::int64_t depth) { return (settlement + depth) % 3; }
inline std::int64_t telling_aid(std::int64_t monster, std::int64_t ally) { return (monster + ally) % 3; }
// 11.1 mine_depth_word, as the spec lists it.
inline std::string depth_word(std::int64_t d) {
    return d == 1 ? "below daylight" : d == 2 ? "to the mountain's roots" : d == 3 ? "to the old deep" : "";
}

}  // namespace p507_f

namespace p507_check {

namespace wd = worldgen::data;
using p503check::at;
using p503check::Index;
using p503check::World;
using Row = eng::Row;

inline bool null_at(const Row& r, std::size_t i) { return r.at(i).type == worldgen::db::ValueType::null; }

struct Tuning {
    std::vector<std::int64_t> deep_years{150, 500, 1000}, deep_wake_ppm{0, 150, 600};
    std::int64_t toll = 400, takes_max = 2, reach_hops = 1;
    std::map<std::string, std::int64_t> mining;   // race key -> mining_permille
    std::map<std::string, std::int64_t> delving;  // r2: race key -> delving_permille (absent 0)
    std::int64_t aid_max = 2, aid_hops = 2;      // r2
};
inline Tuning tuning_of(const wd::ContentSet& c) {
    Tuning t;
    t.deep_years = p507api::tuning_array_or(c, "monsters", "deep_years", t.deep_years);
    t.deep_wake_ppm = p507api::tuning_array_or(c, "monsters", "deep_wake_ppm", t.deep_wake_ppm);
    t.toll = p507api::tuning_or(c, "monsters", "deep_toll_permille", t.toll);
    t.takes_max = p507api::tuning_or(c, "monsters", "deep_takes_max", t.takes_max);
    t.reach_hops = p507api::tuning_or(c, "monsters", "deep_reach_hops", t.reach_hops);
    t.aid_max = p507api::tuning_or(c, "monsters", "deep_aid_max", t.aid_max);
    t.aid_hops = p507api::tuning_or(c, "monsters", "deep_aid_hops", t.aid_hops);
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (const wd::Definition& d : k.definitions) {
            const wd::Value* v = d.record.find("mining_permille");
            t.mining[d.id] = v == nullptr ? 0 : v->as_int();
            const wd::Value* dv = d.record.find("delving_permille");
            t.delving[d.id] = dv == nullptr ? 0 : dv->as_int();
        }
    }
    return t;
}

// GeoView hops (every region_border row; reading D-R5), from r0.
inline std::map<std::int64_t, std::int64_t> geo_hops(const World& w, std::int64_t r0) {
    std::map<std::int64_t, std::vector<std::int64_t>> nb;
    for (const auto& b : w.borders) {
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

inline bool names_monster(const Index& x, std::int64_t e, std::int64_t m) {
    const auto it = x.parts.find(e);
    if (it == x.parts.end()) return false;
    for (const Row* p : it->second) {
        if (at(*p, 3) == 8 && at(*p, 4) == m) return true;
    }
    return false;
}

// The role seq of `role` in kind `kind` (event_kind_role: kind, seq, role, ...), 0 when absent.
inline std::int64_t role_seq(const World& w, std::int64_t kind, const std::string& role) {
    for (const Row& r : w.rows.t.event_kind_role) {
        if (at(r, 0) == kind && r.at(2).bytes == role) return at(r, 1);
    }
    return 0;
}

struct Rows {
    std::map<std::int64_t, const Row*> site, monster, kind_row;
    std::set<std::int64_t> deep_kinds;
    explicit Rows(const World& w) {
        for (const Row& s : w.rows.t.site) site[at(s, 0)] = &s;
        for (const Row& m : w.rows.t.monster) monster[at(m, 0)] = &m;
        for (const Row& k : w.rows.t.monster_kind) {
            kind_row[at(k, 0)] = &k;
            if (at(k, 5) == p507api::WAKE_DEEP) deep_kinds.insert(at(k, 0));
        }
    }
};

// 13 item 3 (r2: the aids between the waking and its outcome, the outcome citing the waking then the aids;
// warren_sealed only at a warren's waking, deep_sealed only at a mine's); item 14's count and order.
inline std::vector<std::string> outcome_violations(const Index& x, const Rows& rw, std::int64_t e, std::int64_t mid,
                                                   std::int64_t lid, const std::vector<std::int64_t>& aids,
                                                   const Tuning& t, bool warren) {
    using p503check::n;
    std::vector<std::string> out;
    const std::int64_t y = at(*x.ev.at(e), 1);
    std::int64_t outcome = 0;
    bool other = false;
    for (auto it = x.ev.upper_bound(e); it != x.ev.end() && at(*it->second, 1) == y; ++it) {
        const std::string k = x.kind_of(it->first);
        if ((k == "deep_sealed" || k == "hold_lost" || k == "warren_sealed") && names_monster(x, it->first, mid)) {
            outcome = it->first;
            break;
        }
        const auto pit = x.parts.find(it->first);
        if (pit == x.parts.end()) continue;
        for (const Row* p : pit->second) other = other || (at(*p, 3) == 8 && at(*p, 4) != mid);
    }
    std::vector<std::int64_t> want{e};
    want.insert(want.end(), aids.begin(), aids.end());
    std::sort(want.begin(), want.end());
    if (outcome == 0 || other || x.cause_list(outcome) != want) {
        out.push_back("3: deep_woken e" + n(e) + " outcome e" + n(outcome));
        return out;
    }
    const std::string k = x.kind_of(outcome);
    if (k != (warren ? "warren_sealed" : "deep_sealed") && k != "hold_lost") {
        out.push_back("3: deep_woken e" + n(e) + " outcome e" + n(outcome) + " is a " + k);
    }
    for (const std::int64_t a : aids) {
        if (a <= e || a >= outcome) out.push_back("3: aid e" + n(a) + " not between e" + n(e) + " and its outcome");
    }
    if (static_cast<std::int64_t>(aids.size()) > t.aid_max) out.push_back("14: deep_woken e" + n(e) + " has too many aids");
    std::vector<std::int64_t> allies;
    for (const std::int64_t a : aids) allies.push_back(x.part(a, 1));
    for (std::size_t i = 1; i < allies.size(); ++i) {
        if (allies[i] <= allies[i - 1]) out.push_back("14: deep_woken e" + n(e) + " aids not in ascending ally id");
    }
    const auto li = rw.site.find(lid);
    if (li != rw.site.end()) {
        const bool fall = k == "hold_lost";
        if (!fall && at(*li->second, 6) != 0) out.push_back("3: sealed e" + n(outcome) + " lair holds a settlement");
        if (fall && at(*li->second, 6) != at(*x.ev.at(e), 6)) out.push_back("3: fall e" + n(outcome) + " lair settlement");
    }
    return out;
}

// 13 items 1 to 9 (10 and 11 are the reference's replay and P5-02's own checks); (r2) 12 to 15.
inline std::vector<std::string> violations(const World& w, const Tuning& t) {
    using p503check::n;
    const Index x(w);
    const Rows rw(w);
    std::vector<std::string> out;
    const auto year = [&](std::int64_t e) { return at(*x.ev.at(e), 1); };
    const auto quiet = [&](std::int64_t e) {
        const auto it = x.phase_kind.find(at(*x.ev.at(e), 4));
        return it != x.phase_kind.end() && it->second == 2;
    };
    std::map<std::int64_t, std::int64_t> race_of_civ;
    for (const auto& [c, r] : x.civ) race_of_civ[c] = at(*r, 2);
    std::map<std::int64_t, std::string> race_key;
    for (const auto& r : w.races) race_key[r.id] = r.key;
    const auto mining_of_civ = [&](std::int64_t c) {
        const auto it = t.mining.find(race_key[race_of_civ[c]]);
        return it == t.mining.end() ? 0 : it->second;
    };
    const auto delving_of_civ = [&](std::int64_t c) {
        const auto it = t.delving.find(race_key[race_of_civ[c]]);
        return it == t.delving.end() ? 0 : it->second;
    };
    std::map<std::int64_t, std::int64_t> sealed_of_m, sealed_of_s;
    for (const std::int64_t e : x.events_of("deep_sealed")) {
        sealed_of_m[x.part(e, 2)] = e;
        sealed_of_s[x.part(e, 3)] = e;
    }
    for (const std::int64_t e : x.events_of("warren_sealed")) sealed_of_m[x.part(e, 2)] = e;   // r2
    std::map<std::int64_t, std::vector<std::int64_t>> aids_of, per_stl;   // r2
    for (const std::int64_t e : x.events_of("ally_aided_hold")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        aids_of[cs.size() == 1U ? cs[0] : 0].push_back(e);
    }
    for (const std::int64_t e : x.events_of("warren_delved")) per_stl[at(*x.ev.at(e), 6)].push_back(e);
    // 1
    std::map<std::int64_t, std::vector<std::int64_t>> per_site;
    for (const std::int64_t e : x.events_of("delving_deepened")) per_site[x.part(e, 2)].push_back(e);
    for (const auto& [sid, es] : per_site) {
        const auto si = rw.site.find(sid);
        if (si == rw.site.end() || at(*si->second, 2) != p507api::SITE_MINE) {
            out.push_back("1: delving_deepened at site " + n(sid) + ", not a mine");
            continue;
        }
        const Row& s = *si->second;
        for (std::size_t i = 0; i < es.size(); ++i) {
            const std::int64_t e = es[i];
            const std::int64_t d = x.val(e, 0);
            if (d != static_cast<std::int64_t>(i) + 1 || i >= 3U) out.push_back("1: mine " + n(sid) + " e" + n(e) + " depth " + n(d));
            if (d < 1 || d > 3 || year(e) - at(s, 9) < t.deep_years[static_cast<std::size_t>(std::clamp<std::int64_t>(d, 1, 3) - 1)]) {
                out.push_back("1: mine " + n(sid) + " depth " + n(d) + " at worked years " + n(year(e) - at(s, 9)));
            }
            if (at(s, 13) != 0 && !null_at(s, 11) && (year(e) > at(s, 11) || (year(e) == at(s, 11) && e > at(s, 12)))) {
                out.push_back("1: mine " + n(sid) + " deepened e" + n(e) + " after its end");
            }
            if (sealed_of_s.count(sid) != 0U && e > sealed_of_s[sid]) {
                out.push_back("1: mine " + n(sid) + " deepened e" + n(e) + " after its sealing");
            }
            const std::int64_t c = x.part(e, 1);
            if (c != at(s, 7) || mining_of_civ(c) <= 0) out.push_back("1: e" + n(e) + " by c" + n(c));
            if (x.val(e, 1) != p507_f::telling_delve(sid, d)) out.push_back("1: e" + n(e) + " telling");
            const std::int64_t want = i == 0 ? at(s, 10) : es[i - 1];
            if (x.cause_list(e) != std::vector<std::int64_t>{want}) out.push_back("1: e" + n(e) + " causes");
        }
    }
    // 2 and 3
    const std::vector<std::int64_t> woken = x.events_of("deep_woken");
    std::map<std::int64_t, std::int64_t> realm, woken_of_m;
    std::map<std::int64_t, int> wakings_of_site, wakings_of_stl;
    std::set<std::int64_t> warren_m;
    for (const std::int64_t e : woken) {
        const std::int64_t mid = x.part(e, 1), c = x.part(e, 2), sid = x.part(e, 3), lid = x.part(e, 4);
        const std::int64_t d = x.val(e, 0);
        realm[mid] = c;
        woken_of_m[mid] = e;
        if (quiet(e)) out.push_back("2: deep_woken e" + n(e) + " in a quiet step");
        if (sid == 0) {   // r2: a warren's waking (no site)
            const std::int64_t tid = at(*x.ev.at(e), 6);
            warren_m.insert(mid);
            ++wakings_of_stl[tid];
            std::int64_t last = 0, got = 0;
            for (const std::int64_t y : per_stl[tid]) {
                if (y < e) {
                    last = std::max(last, y);
                    got = std::max(got, x.val(y, 0));
                }
            }
            if (last == 0 || x.part(last, 1) != c) {
                out.push_back("2: warren waking e" + n(e) + " at settlement " + n(tid) + " without a warren of c" + n(c));
                continue;
            }
            if (d < 1 || d > 3 || t.deep_wake_ppm[static_cast<std::size_t>(d - 1)] <= 0 || got != d) {
                out.push_back("2: warren waking e" + n(e) + " at depth " + n(d));
            }
            const auto mi = rw.monster.find(mid);
            if (mi == rw.monster.end() || rw.deep_kinds.count(at(*mi->second, 2)) == 0U || at(*mi->second, 5) != e) {
                out.push_back("2: deep_woken e" + n(e) + " monster " + n(mid) + " not a deep kind born of it");
            }
            const auto li = rw.site.find(lid);
            const auto st = x.stl.find(tid);
            if (li == rw.site.end() || st == x.stl.end() || at(*li->second, 2) != p507api::SITE_LAIR ||
                at(*li->second, 4) != at(*st->second, 2) || at(*li->second, 5) != at(*st->second, 3) ||
                at(*li->second, 10) != e || at(*li->second, 8) != mid) {
                out.push_back("2: warren waking e" + n(e) + " lair " + n(lid) + " not at the settlement's cell");
            }
            if (x.val(e, 1) != 3 + p507_f::telling_woken(mid)) out.push_back("15: warren waking e" + n(e) + " telling");
            if (x.cause_list(e) != std::vector<std::int64_t>{last}) out.push_back("2: warren waking e" + n(e) + " causes");
            for (const std::string& v : outcome_violations(x, rw, e, mid, lid, aids_of[e], t, true)) out.push_back(v);
            continue;
        }
        ++wakings_of_site[sid];
        const auto si = rw.site.find(sid);
        if (si == rw.site.end() || at(*si->second, 2) != p507api::SITE_MINE || d < 1 || d > 3 ||
            t.deep_wake_ppm[static_cast<std::size_t>(d - 1)] <= 0) {
            out.push_back("2: deep_woken e" + n(e) + " at site " + n(sid) + " depth " + n(d));
            continue;
        }
        const Row& s = *si->second;
        std::int64_t got = 0, last = 0;
        for (const std::int64_t y : per_site[sid]) {
            if (y < e) {
                got = std::max(got, x.val(y, 0));
                last = std::max(last, y);
            }
        }
        if (got != d) out.push_back("2: deep_woken e" + n(e) + " depth " + n(d) + ", the mine's " + n(got));
        const auto mi = rw.monster.find(mid);
        if (mi == rw.monster.end() || rw.deep_kinds.count(at(*mi->second, 2)) == 0U || at(*mi->second, 5) != e) {
            out.push_back("2: deep_woken e" + n(e) + " monster " + n(mid) + " not a deep kind born of it");
        }
        const auto li = rw.site.find(lid);
        if (li == rw.site.end() || at(*li->second, 2) != p507api::SITE_LAIR || at(*li->second, 4) != at(s, 4) ||
            at(*li->second, 5) != at(s, 5) || at(*li->second, 10) != e || at(*li->second, 8) != mid) {
            out.push_back("2: deep_woken e" + n(e) + " lair " + n(lid) + " not at the mine's cell");
        }
        if (c != at(s, 7)) out.push_back("2: deep_woken e" + n(e) + " civilization");
        if (x.val(e, 1) != p507_f::telling_woken(mid)) out.push_back("2, 15: e" + n(e) + " telling");
        if (x.cause_list(e) != std::vector<std::int64_t>{last}) out.push_back("2: deep_woken e" + n(e) + " causes");
        // 3: the first later event of the year naming the monster among 95, 96 and 99, caused by it and its aids,
        // none of another monster between
        for (const std::string& v : outcome_violations(x, rw, e, mid, lid, aids_of[e], t, false)) out.push_back(v);
    }
    for (const auto& [sid, k] : wakings_of_site) {
        if (k > 1) out.push_back("2: mine " + n(sid) + " woke " + std::to_string(k) + " times");
    }
    for (const auto& [tid, k] : wakings_of_stl) {
        if (k > 1) out.push_back("2: warren " + n(tid) + " woke " + std::to_string(k) + " times");
    }
    // 4
    for (const auto& [mid, se] : sealed_of_m) {
        const auto mi = rw.monster.find(mid);
        if (mi == rw.monster.end() || at(*mi->second, 9) != 2) out.push_back("4: sealed monster " + n(mid) + " not asleep");
        for (auto it = x.ev.upper_bound(se); it != x.ev.end(); ++it) {
            const std::int64_t k = at(*it->second, 3);
            const bool listed = k == 46 || k == 48 || k == 50 || k == 51 || k == 88 || k == 89 || k == 96;
            if (listed && names_monster(x, it->first, mid)) {
                out.push_back("4: sealed monster " + n(mid) + " in e" + n(it->first) + " " + x.kind_of(it->first));
            }
        }
        if (x.val(se, 0) != p507_f::telling_sealed(mid)) out.push_back("4: e" + n(se) + " telling");
        if (x.kind_of(se) == "warren_sealed") {   // r2: the warren never deepens again
            for (const std::int64_t y : per_stl[at(*x.ev.at(se), 6)]) {
                if (y > se) out.push_back("4: warren delved e" + n(y) + " after its sealing");
            }
        }
    }
    // 5 and 9
    std::map<std::int64_t, int> takes;
    for (const std::int64_t e : x.events_of("hold_lost")) {
        const std::int64_t mid = x.part(e, 1), c = x.part(e, 2), sid = at(*x.ev.at(e), 6);
        const int k = ++takes[mid];
        const auto st = x.stl.find(sid);
        if (st == x.stl.end() || at(*st->second, 10) != 3 || at(*st->second, 13) != e ||
            (st->second->size() > 14U && at(*st->second, 14) != p507api::SETTLEMENT_ENDING_OVERRUN)) {
            out.push_back("5: hold_lost e" + n(e) + " settlement " + n(sid) + " not ruined by it with ending 2");
        }
        if (realm.count(mid) == 0U || realm[mid] != c) out.push_back("5: hold_lost e" + n(e) + " not the waking realm");
        const std::int64_t dead = x.val(e, 0), V = x.val(e, 1);
        if (p507_f::take_toll(dead + V, t.toll) != std::make_pair(dead, V)) out.push_back("5: hold_lost e" + n(e) + " toll");
        if (x.val(e, 2) != p507_f::telling_lost(mid, sid) + (warren_m.count(mid) != 0U ? 3 : 0)) {
            out.push_back("5, 15: e" + n(e) + " telling");
        }
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (k == 1) {
            std::vector<std::int64_t> want;
            if (woken_of_m.count(mid) != 0U) {
                want.push_back(woken_of_m[mid]);
                for (const std::int64_t a : aids_of[woken_of_m[mid]]) want.push_back(a);
            }
            if (want.empty() || cs != want) out.push_back("5: first hold_lost e" + n(e) + " causes");
        } else {
            const std::int64_t raid = cs.size() == 1U ? cs[0] : 0;
            if (raid == 0 || x.kind_of(raid) != "monster_raid" || x.part(raid, 1) != mid ||
                at(*x.ev.at(raid), 6) != sid || year(raid) != year(e)) {
                out.push_back("5: later hold_lost e" + n(e) + " causes");
            }
            const auto mi = rw.monster.find(mid);
            const auto li = mi == rw.monster.end() ? rw.site.end() : rw.site.find(at(*mi->second, 3));
            if (li != rw.site.end()) {
                const auto hops = geo_hops(w, at(*li->second, 3));
                const auto h = hops.find(at(*x.ev.at(e), 5));
                if (h == hops.end() || h->second > t.reach_hops) out.push_back("5: later hold_lost e" + n(e) + " out of reach");
            }
        }
        for (const Row& s : w.rows.t.site) {   // 9
            if (at(s, 2) != p507api::SITE_MINE || at(s, 6) != sid || at(s, 10) > e) continue;
            if (at(s, 13) != 0 && at(s, 12) != e && (at(s, 11) < year(e) || at(s, 12) < e)) continue;
            if (at(s, 13) != p507api::ENDING_LOST || at(s, 12) != e || at(s, 11) != year(e)) {
                out.push_back("9: mine " + n(at(s, 0)) + " at the lost hold " + n(sid) + " does not end with it");
            }
            if (x.part(e, 4) != at(s, 0)) out.push_back("5: hold_lost e" + n(e) + " does not name the open mine");
        }
    }
    for (const auto& [mid, k] : takes) {
        if (k > 1 + t.takes_max) out.push_back("5: monster " + n(mid) + " took " + std::to_string(k) + " holds");
    }
    // 6 (reading D-R12)
    for (const std::int64_t e : x.events_of("refugees_settled")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.size() != 1U || x.kind_of(cs[0]) != "hold_lost") continue;
        if (year(cs[0]) != year(e) || x.part(e, 2) != x.part(cs[0], 2) || x.part(e, 3) != at(*x.ev.at(cs[0]), 6)) {
            out.push_back("6: refugees_settled e" + n(e) + " from hold_lost e" + n(cs[0]));
        }
    }
    std::set<std::int64_t> alive, had;
    for (const auto& [c, r] : x.civ) {
        had.insert(at(*r, 2));
        if (null_at(*r, 8)) alive.insert(at(*r, 2));
    }
    for (const std::int64_t r : had) {
        if (alive.count(r) == 0U) out.push_back("6: race " + n(r) + " has no civilization alive at E");
    }
    // 7
    for (const std::int64_t e : x.events_of("civ_ended")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.size() != 1U || x.kind_of(cs[0]) != "hold_lost") continue;
        const std::int64_t c = x.part(e, 1);
        bool holds = false;
        for (const auto& [sid, s] : x.stl) holds = holds || at(*s, 8) == c;
        if (year(e) <= year(cs[0]) || holds) out.push_back("7: civ_ended e" + n(e) + " after hold_lost e" + n(cs[0]));
    }
    // r2 12: per warren settlement
    std::map<std::int64_t, std::vector<std::pair<std::int64_t, std::int64_t>>> lives;   // civ -> (first delving, end) event ids
    for (const auto& [tid, es] : per_stl) {
        const std::int64_t c = x.part(es.front(), 1);
        if (mining_of_civ(c) != 0 || delving_of_civ(c) <= 0) out.push_back("12: warren " + n(tid) + " delved by c" + n(c));
        const std::vector<std::int64_t> root_cs = x.cause_list(es.front());
        const std::int64_t root = root_cs.size() == 1U ? root_cs[0] : 0;
        if (root == 0) out.push_back("12: first warren_delved e" + n(es.front()) + " causes");
        for (std::size_t i = 0; i < es.size(); ++i) {
            const std::int64_t e = es[i], d = x.val(e, 0);
            if (d != static_cast<std::int64_t>(i) + 1 || i >= 3U) out.push_back("12: warren " + n(tid) + " e" + n(e) + " depth " + n(d));
            if (x.part(e, 1) != c) out.push_back("12: warren " + n(tid) + " delved by two realms");
            if (x.val(e, 1) != p507_f::telling_warren_delved(tid, d)) out.push_back("12: e" + n(e) + " telling");
            if (i > 0 && x.cause_list(e) != std::vector<std::int64_t>{es[i - 1]}) out.push_back("12: e" + n(e) + " causes");
            if (root != 0 && x.ev.count(root) != 0U && d >= 1 && d <= 3 &&
                year(e) - year(root) < t.deep_years[static_cast<std::size_t>(d - 1)]) {
                out.push_back("12: warren " + n(tid) + " depth " + n(d) + " too early");
            }
        }
        // the warren's life for its delver ends at its waking, its ruin, its capture, cession or secession, a cession of
        // its region or its delver's conquest (D-R20)
        std::int64_t end = 0;
        const auto st = x.stl.find(tid);
        if (st != x.stl.end() && at(*st->second, 13) > es.front()) end = at(*st->second, 13);
        for (auto it = x.ev.upper_bound(es.front()); it != x.ev.end(); ++it) {
            const std::string k = x.kind_of(it->first);
            bool hit = (k == "deep_woken" && at(*it->second, 6) == tid);
            if (k == "settlement_captured" || k == "settlements_ceded" || k == "civ_seceded") {
                hit = at(*it->second, 6) == tid;
                const auto pit = x.parts.find(it->first);
                if (pit != x.parts.end()) {
                    for (const Row* p : pit->second) hit = hit || (at(*p, 3) == 4 && at(*p, 4) == tid);
                }
            }
            // integration: a cession of the warren's region hands its settlements over; so does the delver's conquest
            if (k == "region_ceded" && st != x.stl.end() && at(*it->second, 5) == at(*st->second, 4)) hit = true;
            if (k == "civ_conquered") {
                const auto pit = x.parts.find(it->first);
                if (pit != x.parts.end()) {
                    for (const Row* p : pit->second) hit = hit || (at(*p, 3) == 3 && at(*p, 4) == c);
                }
            }
            if (hit && (end == 0 || it->first < end)) end = it->first;
            if (hit) break;
        }
        for (const std::int64_t e : es) {
            if (end != 0 && e > end && (x.kind_of(end) == "deep_woken" || (st != x.stl.end() && end == at(*st->second, 13)))) {
                out.push_back("12: warren " + n(tid) + " delved e" + n(e) + " after its waking or ruin");
            }
        }
        lives[c].push_back({es.front(), end == 0 ? (std::int64_t{1} << 40) : end});
    }
    // Invariant 12 as restated by the lead (integration): a realm starts no new warren while it holds a live one
    // (a warren lost and won back, or taken from another realm, may leave it with two). By event ids: no warren's
    // first warren_delved lies inside the life of an earlier warren of the same realm.
    for (const auto& [c, ls] : lives) {
        for (std::size_t i = 0; i < ls.size(); ++i) {
            for (std::size_t j = 0; j < ls.size(); ++j) {
                if (i != j && ls[i].first < ls[j].first && ls[j].first < ls[i].second) {
                    out.push_back("12: c" + n(c) + " started a warren while it held a live one");
                }
            }
        }
    }
    // r2 13: no kind 3 site and no arms_sold seller of a race with mining_permille 0
    for (const Row& s : w.rows.t.site) {
        if (at(s, 2) == p507api::SITE_MINE && mining_of_civ(at(s, 7)) <= 0) out.push_back("13: mine " + n(at(s, 0)) + " of a race that does not mine");
    }
    const std::int64_t k_arms = x.kind_id("arms_sold");
    const std::int64_t seller = k_arms < 0 ? 0 : role_seq(w, k_arms, "seller");
    for (const std::int64_t e : x.events_of("arms_sold")) {
        const std::int64_t c = seller == 0 ? 0 : x.part(e, seller);
        if (c != 0 && mining_of_civ(c) <= 0) out.push_back("13: arms_sold e" + n(e) + " by a race that does not mine");
    }
    // r2 14: every ally_aided_hold
    std::set<std::int64_t> forbidden;
    for (const char* k : {"war_declared", "ally_joined_war", "ally_refused", "claim_settlements"}) {
        if (x.kind_id(k) >= 0) forbidden.insert(x.kind_id(k));
    }
    std::map<std::int64_t, std::vector<std::int64_t>> cited;
    for (const auto& [e, cs] : x.causes) {
        for (const std::int64_t c : cs) cited[c].push_back(e);
    }
    for (const auto& [w0, aids] : aids_of) {
        for (const std::int64_t a : aids) {
            const std::int64_t ally = x.part(a, 1), partner = x.part(a, 2), mid = x.part(a, 3), q = x.part(a, 4);
            if (x.kind_of(w0) != "deep_woken" || x.part(w0, 2) != partner || x.part(w0, 1) != mid ||
                year(w0) != year(a) || ally == partner) {
                out.push_back("14: ally_aided_hold e" + n(a) + " does not answer its waking");
                continue;
            }
            bool allied = false;
            for (const Row& r : w.rows.t.relation) {
                if (r.at(4).bytes != "alliance") continue;
                const bool pair = (at(r, 1) == ally && at(r, 3) == partner) || (at(r, 1) == partner && at(r, 3) == ally);
                const bool open = null_at(r, 6) || at(r, 6) == -1 || at(r, 6) > year(a) || at(r, 8) > a;
                allied = allied || (pair && at(r, 7) < a && open);
            }
            if (!allied) out.push_back("14: ally_aided_hold e" + n(a) + ": not allied");
            const auto st = x.stl.find(q);
            bool ok = st != x.stl.end() && at(*st->second, 6) <= year(a) &&
                      (at(*st->second, 13) == 0 || at(*st->second, 13) > a);
            if (ok && !w.borders.empty()) {
                const auto hops = geo_hops(w, at(*x.ev.at(a), 5));
                const auto h = hops.find(at(*st->second, 4));
                ok = h != hops.end() && h->second <= t.aid_hops;
            }
            if (!ok) out.push_back("14: ally_aided_hold e" + n(a) + " origin " + n(q) + " not standing within reach");
            if (x.val(a, 0) != p507_f::telling_aid(mid, ally)) out.push_back("14: e" + n(a) + " telling");
            for (const std::int64_t b : cited[a]) {
                if (forbidden.count(at(*x.ev.at(b), 3)) != 0U) out.push_back("14: e" + n(b) + " caused by the aid e" + n(a));
            }
            for (const Row& r : w.rows.t.relation) {
                if (at(r, 7) == a || at(r, 8) == a) out.push_back("14: a relation row caused by the aid e" + n(a));
            }
        }
    }
    // 8
    const std::vector<std::string> keys = {"dragon", "giant", "ogre", "troll", "undead", "wyvern", "ymrach", "ysgarth", "zhakul"};
    const auto& mk = w.rows.t.monster_kind;
    bool ok8 = mk.size() >= keys.size();
    for (std::size_t i = 0; ok8 && i < keys.size(); ++i) {
        ok8 = at(mk[i], 0) == static_cast<std::int64_t>(i) + 1 && mk[i][1].bytes == keys[i] &&
              (i < 6U ? at(mk[i], 5) != p507api::WAKE_DEEP : at(mk[i], 5) == p507api::WAKE_DEEP);
    }
    if (!ok8) out.push_back("8: monster_kind ids 1 to 9 are not the spec's");
    return out;
}

// 10 item 7: the [stats] keys and values in the spec's order.
inline std::vector<std::pair<std::string, std::int64_t>> stats(const World& w) {
    const Index x(w);
    const Rows rw(w);
    const std::vector<std::int64_t> dd = x.events_of("delving_deepened");
    std::set<std::int64_t> deepest, lost;
    for (const std::int64_t e : dd) {
        if (x.val(e, 0) == 3) deepest.insert(x.part(e, 2));
    }
    for (const std::int64_t e : x.events_of("hold_lost")) lost.insert(e);
    const auto by_lost = [&](const std::string& k) {
        std::int64_t c = 0;
        for (const std::int64_t e : x.events_of(k)) {
            for (const std::int64_t cs : x.cause_list(e)) c += lost.count(cs) != 0U ? 1 : 0;
        }
        return c;
    };
    std::set<std::int64_t> warrens_deepest, aided;
    for (const std::int64_t e : x.events_of("warren_delved")) {
        if (x.val(e, 0) == 3) warrens_deepest.insert(at(*x.ev.at(e), 6));
    }
    std::int64_t warrens_woken = 0;
    for (const std::int64_t e : x.events_of("deep_woken")) warrens_woken += x.part(e, 3) == 0 ? 1 : 0;
    for (const std::int64_t e : x.events_of("ally_aided_hold")) {
        for (const std::int64_t c : x.cause_list(e)) {
            if (x.kind_of(c) == "deep_woken") aided.insert(c);
        }
    }
    std::int64_t alive = 0;
    for (const auto& [id, m] : rw.monster) {
        alive += rw.deep_kinds.count(at(*m, 2)) != 0U && (at(*m, 9) == 1 || at(*m, 9) == 2) ? 1 : 0;
    }
    return {{"mines_deepened", static_cast<std::int64_t>(dd.size())},
            {"mines_deepest", static_cast<std::int64_t>(deepest.size())},
            {"deep_woken", static_cast<std::int64_t>(x.events_of("deep_woken").size())},
            {"deep_sealed", static_cast<std::int64_t>(x.events_of("deep_sealed").size())},
            {"holds_lost", static_cast<std::int64_t>(lost.size())},
            {"deep_flights", by_lost("refugees_settled")},
            {"realms_lost_to_deep", by_lost("civ_ended")},
            {"deep_alive", alive},
            // r2
            {"warrens_delved", static_cast<std::int64_t>(x.events_of("warren_delved").size())},
            {"warrens_deepest", static_cast<std::int64_t>(warrens_deepest.size())},
            {"warrens_woken", warrens_woken},
            {"warrens_sealed", static_cast<std::int64_t>(x.events_of("warren_sealed").size())},
            {"deep_aided", static_cast<std::int64_t>(x.events_of("ally_aided_hold").size())},
            {"wakings_aided", static_cast<std::int64_t>(aided.size())}};
}

// 9's derivation: the depth of a mine at year Y (0 none, or a site of another kind).
inline std::int64_t mine_depth_at(const World& w, std::int64_t site, std::int64_t year) {
    const Index x(w);
    std::int64_t d = 0;
    for (const std::int64_t e : x.events_of("delving_deepened")) {
        if (x.part(e, 2) == site && at(*x.ev.at(e), 1) <= year) d = std::max(d, x.val(e, 0));
    }
    return d;
}

// r2 9's derivation: the depth of a warren at year Y (0 none).
inline std::int64_t warren_depth_at(const World& w, std::int64_t settlement, std::int64_t year) {
    const Index x(w);
    std::int64_t d = 0;
    for (const std::int64_t e : x.events_of("warren_delved")) {
        if (at(*x.ev.at(e), 6) == settlement && at(*x.ev.at(e), 1) <= year) d = std::max(d, x.val(e, 0));
    }
    return d;
}

}  // namespace p507_check
