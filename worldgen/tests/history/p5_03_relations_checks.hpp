#pragma once

// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) section 19's invariants checked from the rows of a world (a
// stored world file or a run's rows), and the count lines of the golden file p5-03-relations.txt. Written from the
// spec alone; the only library names are the adapter's (p5_03_relations_api.hpp: RTables) and the row tables.
//
// What rows cannot show is left out and named at each check (a host's room, D between rows, a population before
// and after a step): the reference (p5_03_relations_reference.py) checks those inside its replay.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p5_03_relations_api.hpp"
#include "p5_03_relations_formulas.hpp"

namespace p503check {

namespace wd = worldgen::data;
namespace db = worldgen::db;
using eng::Row;
using eng::Tables;
using p503api::RTables;

inline std::int64_t at(const Row& r, std::size_t i) { return r.at(i).integer; }
inline bool is_null(const eng::Cell& c) { return c.type == db::ValueType::null; }
inline std::string n(std::int64_t v) { return std::to_string(v); }

struct RaceInfo {
    std::int64_t id = 0;
    std::string key;
    std::int64_t camp = 0;
    bool present = false;
};
struct Border {
    std::int64_t a = 0, b = 0;
    bool open = false;
};

// What the checks read besides the history rows.
struct World {
    RTables rows;
    std::vector<RaceInfo> races;
    std::vector<Border> borders;
    std::int64_t end_year = 0;
    std::vector<std::string> region_names;   // table region (the geographic names)
    const wd::ContentSet* content = nullptr;
};

inline World world_of_file(const db::WorldReader& r, const wd::ContentSet& content) {
    World w;
    w.rows = p503api::tables_of_world(r);
    for (const auto& x : chrono_test::rows_of(r, "race")) {
        w.races.push_back({x.at(0).integer, x.at(1).bytes, x.at(5).integer, x.at(9).integer != 0});
    }
    for (const auto& x : chrono_test::rows_of(r, "region_border")) {
        w.borders.push_back({x.at(0).integer, x.at(1).integer, x.at(6).integer > 0 || x.at(9).integer > 0});
    }
    w.end_year = std::stoll(r.meta("history_end_year").value_or("0"));
    for (const auto& x : chrono_test::rows_of(r, "region")) w.region_names.push_back(x.at(1).bytes);
    w.content = &content;
    return w;
}

inline World world_of_run(RTables t, const eng::HistoryInputs& in, const wd::ContentSet& content) {
    World w;
    w.rows = std::move(t);
    for (const db::RaceRow& x : in.races) w.races.push_back({x.id, x.key, x.camp, x.present != 0});
    for (const db::RegionBorderRow& b : in.region_borders) w.borders.push_back({b.a, b.b, b.walk_pairs > 0 || b.pass_id > 0});
    w.end_year = in.params.span;
    for (const db::RegionRow& x : in.regions) w.region_names.push_back(x.name);
    w.content = &content;
    return w;
}

// --- Data ------------------------------------------------------------------------------------------------

inline const wd::Definition* definition(const wd::ContentSet& c, const std::string& kind, const std::string& id) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != kind) continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == id) return &d;
        }
    }
    return nullptr;
}

// 6.2.1's B from the data: history.conflict.camp and the races' stances.
inline std::int64_t camp_value(const wd::ContentSet& c, std::int64_t ka, std::int64_t kb) {
    const wd::Definition* d = definition(c, "history", "conflict");
    REQUIRE(d != nullptr);
    const wd::Record& camp = d->record.value_at(0).as_record();   // history.conflict.camp is the first field
    static const char* names[3][3] = {{"good_good", "good_neutral", "good_evil"},
                                      {"good_neutral", "neutral_neutral", "neutral_evil"},
                                      {"good_evil", "neutral_evil", "evil_evil"}};
    return camp.get_int(names[std::min(ka, kb)][std::max(ka, kb)]);
}
inline std::int64_t stance(const wd::ContentSet& c, const std::string& from, const std::string& to) {
    const wd::Definition* d = definition(c, "race", from);
    REQUIRE(d != nullptr);
    for (std::size_t i = 0; i < d->record.size(); ++i) {
        if (d->record.name_at(i) != "stances") continue;
        for (const wd::Value& v : d->record.value_at(i).as_array()) {
            const wd::Record& r = v.as_record();
            if (r.get_string("race") == to) return r.get_int("value");
        }
    }
    return 0;
}

// --- An index of the rows --------------------------------------------------------------------------------

struct Index {
    std::map<std::string, std::int64_t> kid;
    std::map<std::int64_t, std::string> key;
    std::map<std::int64_t, const Row*> ev, civ, stl;
    std::map<std::int64_t, std::vector<const Row*>> parts;
    std::map<std::int64_t, std::vector<std::int64_t>> vals, causes;
    std::map<std::int64_t, std::int64_t> phase_kind, race_camp;
    std::map<std::int64_t, std::string> race_key;
    std::map<std::int64_t, std::vector<std::int64_t>> nb;   // open borders

    explicit Index(const World& w) {
        const Tables& t = w.rows.t;
        for (const Row& k : t.event_kind) {
            kid[k[1].bytes] = at(k, 0);
            key[at(k, 0)] = k[1].bytes;
        }
        for (const Row& e : t.event) ev[at(e, 0)] = &e;
        for (const Row& p : t.event_participant) parts[at(p, 0)].push_back(&p);
        for (const Row& v : t.event_value) vals[at(v, 0)].push_back(at(v, 2));
        for (const Row& c : t.event_cause) causes[at(c, 0)].push_back(at(c, 1));
        for (const Row& c : t.civilization) civ[at(c, 0)] = &c;
        for (const Row& s : t.settlement) stl[at(s, 0)] = &s;
        for (const Row& p : t.phase) phase_kind[at(p, 0)] = at(p, 2);
        for (const RaceInfo& r : w.races) {
            race_camp[r.id] = r.camp;
            race_key[r.id] = r.key;
        }
        for (const Border& b : w.borders) {
            if (!b.open) continue;
            nb[b.a].push_back(b.b);
            nb[b.b].push_back(b.a);
        }
    }
    [[nodiscard]] std::int64_t kind_id(const std::string& k) const {
        const auto it = kid.find(k);
        return it == kid.end() ? -1 : it->second;
    }
    [[nodiscard]] std::string kind_of(std::int64_t e) const {
        const auto it = ev.find(e);
        if (it == ev.end()) return "";
        const auto k = key.find(at(*it->second, 3));
        return k == key.end() ? "" : k->second;
    }
    [[nodiscard]] std::int64_t part(std::int64_t e, std::int64_t role) const {
        const auto it = parts.find(e);
        if (it == parts.end()) return 0;
        for (const Row* p : it->second) {
            if (at(*p, 2) == role) return at(*p, 4);
        }
        return 0;
    }
    [[nodiscard]] std::int64_t val(std::int64_t e, std::size_t i) const {
        const auto it = vals.find(e);
        return it == vals.end() || it->second.size() <= i ? 0 : it->second[i];
    }
    [[nodiscard]] std::vector<std::int64_t> cause_list(std::int64_t e) const {
        const auto it = causes.find(e);
        std::vector<std::int64_t> v = it == causes.end() ? std::vector<std::int64_t>{} : it->second;
        std::sort(v.begin(), v.end());
        return v;
    }
    [[nodiscard]] std::int64_t civ_race(std::int64_t c) const {
        const auto it = civ.find(c);
        return it == civ.end() ? 0 : at(*it->second, 2);
    }
    [[nodiscard]] std::vector<std::int64_t> events_of(const std::string& k) const {
        std::vector<std::int64_t> out;
        const std::int64_t id = kind_id(k);
        for (const auto& [e, r] : ev) {
            if (at(*r, 3) == id) out.push_back(e);
        }
        return out;
    }
    // PeoplesView::hops (P3-05 4.4: open borders), from region r0.
    [[nodiscard]] std::map<std::int64_t, std::int64_t> hops(std::int64_t r0) const {
        std::map<std::int64_t, std::int64_t> d{{r0, 0}};
        std::vector<std::int64_t> fifo{r0};
        for (std::size_t i = 0; i < fifo.size(); ++i) {
            const auto it = nb.find(fifo[i]);
            if (it == nb.end()) continue;
            std::vector<std::int64_t> ns = it->second;
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
};

inline std::int64_t base_of(const World& w, const Index& x, std::int64_t a, std::int64_t b) {
    return rel_f::base(camp_value(*w.content, x.race_camp.at(a), x.race_camp.at(b)),
                       stance(*w.content, x.race_key.at(a), x.race_key.at(b)),
                       stance(*w.content, x.race_key.at(b), x.race_key.at(a)));
}

using Pair = std::pair<std::int64_t, std::int64_t>;

// The race_relation rows by pair, ascending year.
inline std::map<Pair, std::vector<const Row*>> pair_rows(const World& w) {
    std::map<Pair, std::vector<const Row*>> out;
    for (const Row& r : w.rows.race_relation) out[{at(r, 0), at(r, 1)}].push_back(&r);
    return out;
}

// The band of a pair in force before year y (its last row with year < y; unmet: the plain band of B).
inline std::int64_t band_before(const World& w, const Index& x, const std::map<Pair, std::vector<const Row*>>& rows,
                                std::int64_t a, std::int64_t b, std::int64_t y) {
    const Pair p{std::min(a, b), std::max(a, b)};
    std::optional<std::int64_t> band;
    if (const auto it = rows.find(p); it != rows.end()) {
        for (const Row* r : it->second) {
            if (at(*r, 2) < y) band = at(*r, 4);
        }
    }
    if (band) return *band;
    const std::int64_t h = p503api::tuning_int(*w.content, "relations", "hatred_max");
    const std::int64_t f = p503api::tuning_int(*w.content, "relations", "friend_min");
    return rel_f::plain_band(base_of(w, x, p.first, p.second), h, f);
}

// settlement_race rows by settlement, in key order (race, year).
inline std::map<std::int64_t, std::vector<const Row*>> share_rows(const World& w) {
    std::map<std::int64_t, std::vector<const Row*>> out;
    for (const Row& r : w.rows.settlement_race) out[at(r, 0)].push_back(&r);
    return out;
}

// The entries in force at year y (per race the last row with year <= y and share > 0): race -> (share, standing).
inline std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> in_force(const std::vector<const Row*>& rows,
                                                                               std::int64_t y) {
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> last;
    for (const Row* r : rows) {
        if (at(*r, 2) <= y) last[at(*r, 1)] = {at(*r, 3), at(*r, 4)};
    }
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> out;
    for (const auto& [g, v] : last) {
        if (v.first > 0) out[g] = v;
    }
    return out;
}

// --- The invariants of 19 --------------------------------------------------------------------------------

struct Options {
    bool ghost_fade_zero = true;   // invariant 10 needs ghost_fade_years 0
};

inline std::vector<std::string> violations(const World& w, const Options& o = {}) {
    std::vector<std::string> out;
    const Index x(w);
    const Tables& t = w.rows.t;
    const wd::ContentSet& c = *w.content;
    auto rel = [&](const char* f) { return p503api::tuning_int(c, "relations", f); };
    auto mon = [&](const char* f) { return p503api::tuning_int(c, "monsters", f); };
    const std::int64_t h = rel("hatred_max"), f = rel("friend_min"), m = rel("band_margin");
    const std::int64_t mem_min = rel("memory_min"), mem_max = rel("memory_max");
    const auto prows = pair_rows(w);
    const auto srows = share_rows(w);

    // 1, 2: meetings, rows and turns of each pair
    std::map<Pair, std::int64_t> met;
    for (const std::int64_t e : x.events_of("peoples_met")) {
        const std::int64_t a = x.part(e, 1), b = x.part(e, 2);
        if (!(a < b)) out.push_back("1: e" + n(e) + " race_a " + n(a) + " is not below race_b " + n(b));
        const Pair p{a, b};
        if (met.count(p) != 0U) out.push_back("1: " + n(a) + "-" + n(b) + " met twice");
        met[p] = e;
        const std::int64_t B = base_of(w, x, a, b);
        if (x.val(e, 0) != B) out.push_back("2: e" + n(e) + " meeting disposition is not B " + n(B));
        if (x.val(e, 1) != rel_f::plain_band(B, h, f)) out.push_back("1: e" + n(e) + " meeting band is not plain");
        if (x.ev.at(e)->at(6).integer != 0) out.push_back("8.1: e" + n(e) + " has a settlement");
    }
    for (const auto& [p, rs] : prows) {
        const auto mt = met.find(p);
        if (mt == met.end()) {
            out.push_back("1: rows of a pair that never met " + n(p.first) + "-" + n(p.second));
            continue;
        }
        if (at(*rs.front(), 2) != at(*x.ev.at(mt->second), 1)) out.push_back("1: first row of " + n(p.first) + "-" +
                                                                               n(p.second) + " is not at its meeting");
        const std::int64_t B = base_of(w, x, p.first, p.second);
        for (const Row* r : rs) {
            const std::int64_t D = at(*r, 3);
            if (D < B + mem_min || D > B + mem_max) {
                out.push_back("2: " + n(p.first) + "-" + n(p.second) + " at " + n(at(*r, 2)) + ": D " + n(D) +
                              " outside B + memory bounds");
            }
            if (at(*r, 4) < 0 || at(*r, 4) > 2) out.push_back("16.2: band outside 0 to 2");
        }
        for (std::size_t i = 1; i < rs.size(); ++i) {
            if (at(*rs[i], 3) == at(*rs[i - 1], 3) && at(*rs[i], 4) == at(*rs[i - 1], 4)) {
                out.push_back("6.7: " + n(p.first) + "-" + n(p.second) + " row equals the previous");
            }
        }
    }
    std::map<Pair, std::int64_t> band;
    for (const auto& [p, e] : met) band[p] = x.val(e, 1);
    for (const std::int64_t e : x.events_of("disposition_turned")) {
        const Pair p{x.part(e, 1), x.part(e, 2)};
        const auto mt = met.find(p);
        if (mt == met.end() || mt->second > e) {
            out.push_back("1: e" + n(e) + " turned before its pair met");
            continue;
        }
        const std::int64_t D = x.val(e, 0), nb = x.val(e, 1), ob = band[p];
        if (std::abs(nb - ob) != 1) out.push_back("1: e" + n(e) + " changes the band by more than one step");
        if (rel_f::next_band(ob, D, h, f, m) != nb) {
            out.push_back("6.2.3: e" + n(e) + " band " + n(ob) + " -> " + n(nb) + " at D " + n(D) +
                          " against the hysteresis");
        }
        band[p] = nb;
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.empty() || cs.size() > 8U) out.push_back("6.2.3: e" + n(e) + " has " + n(static_cast<std::int64_t>(cs.size())) + " causes");
        if (x.ev.at(e)->at(5).integer != 0) out.push_back("8.1: e" + n(e) + " has a region");
    }
    for (const auto& [p, e] : met) {
        const auto it = prows.find(p);
        if (it == prows.end()) continue;
        if (at(*it->second.back(), 4) != band[p]) out.push_back("6.7: the last row of " + n(p.first) + "-" +
                                                                n(p.second) + " is not its band at E");
    }

    // 3, 4: shares
    for (const auto& [s, rs] : srows) {
        std::set<std::int64_t> years;
        for (const Row* r : rs) {
            years.insert(at(*r, 2));
            if (at(*r, 3) == 0 && at(*r, 4) != 0) out.push_back("16.2: s" + n(s) + " standing with share 0");
        }
        for (const std::int64_t y : years) {
            const auto f0 = in_force(rs, y);
            std::int64_t sum = 0, ruling = 0;
            for (const auto& [g, v] : f0) {
                sum += v.first;
                ruling += v.second == p503api::RULING ? 1 : 0;
            }
            if (sum != 1000) out.push_back("3: s" + n(s) + " shares at " + n(y) + " sum to " + n(sum));
            if (ruling != 1) out.push_back("3: s" + n(s) + " has " + n(ruling) + " ruling races at " + n(y));
        }
        const auto st = x.stl.find(s);
        if (st == x.stl.end()) {
            out.push_back("16.4: rows of unknown settlement " + n(s));
            continue;
        }
        const Row& S = *st->second;
        const std::int64_t status = at(S, 10);
        if (status == 1 || status == 2) {   // ruling race = the owner's at E
            for (const auto& [g, v] : in_force(rs, w.end_year)) {
                if (v.second == p503api::RULING && g != x.civ_race(at(S, 8))) {
                    out.push_back("3: s" + n(s) + " ruled by race " + n(g) + " at E, owner's race " +
                                  n(x.civ_race(at(S, 8))));
                }
            }
        }
        if (!is_null(S.at(12))) {   // no row after the ruin
            if (*years.rbegin() > S.at(12).integer) out.push_back("6.7: s" + n(s) + " has rows after its ruin");
        }
        // 4: the first rows
        const std::int64_t y0 = *years.begin();
        if (y0 != at(S, 6)) out.push_back("4: s" + n(s) + " first rows at " + n(y0) + ", founded " + n(at(S, 6)));
        bool arrival_first_year = false;
        for (const std::int64_t e : x.events_of("refugees_arrived")) {
            if (x.part(e, 1) == s && at(*x.ev.at(e), 1) == y0) arrival_first_year = true;
        }
        if (arrival_first_year) continue;   // a same-year arrival replaces the founding rows (6.7)
        const auto first = in_force(rs, y0);
        const std::int64_t founding = at(S, 7);
        if (x.kind_of(founding) == "refugees_settled") {
            const std::int64_t origin = x.part(founding, 3);
            const auto org = srows.find(origin);
            if (org == srows.end()) {
                out.push_back("4: refuge s" + n(s) + "'s origin s" + n(origin) + " has no rows");
                continue;
            }
            const auto want = in_force(org->second, y0);
            std::map<std::int64_t, std::int64_t> a1, b1;
            for (const auto& [g, v] : first) a1[g] = v.first;
            for (const auto& [g, v] : want) b1[g] = v.first;
            if (a1 != b1) out.push_back("4: refuge s" + n(s) + "'s first shares are not its origin's");
        } else {
            const std::int64_t r0 = x.civ_race(at(S, 5));
            if (!(first.size() == 1U && first.count(r0) == 1U && first.at(r0).first == 1000 &&
                  first.at(r0).second == p503api::RULING)) {
                out.push_back("4: colony s" + n(s) + "'s first rows are not its founder's race at 1000");
            }
        }
    }
    for (const auto& [s, S] : x.stl) {   // every settlement has rows (6.3.1)
        // ... except one ruined in the step that founded it: rows are written at the end of a step for standing
        // settlements only (6.7; a step is at most a quiet step of 25 years, P3-04's quiet_step_years)
        const bool ruined_at_once = at(*S, 10) >= 3 && S->at(12).type == db::ValueType::integer &&
                                    at(*S, 12) - at(*S, 6) < 25;
        if (srows.count(s) == 0U && !ruined_at_once) out.push_back("6.3.1: s" + n(s) + " has no settlement_race row");
    }

    // 5: arrivals
    const std::int64_t mhops = rel("migrate_hops"), mmin = rel("migrate_min");
    for (const std::int64_t e : x.events_of("refugees_arrived")) {
        const Row& E = *x.ev.at(e);
        const std::int64_t u = x.part(e, 1), cu = x.part(e, 2), from = x.part(e, 3), g = x.part(e, 4);
        const std::int64_t y = at(E, 1);
        if (u == from) out.push_back("5: e" + n(e) + " shelters in its origin");
        if (x.stl.count(u) == 0U || x.stl.count(from) == 0U) {
            out.push_back("5: e" + n(e) + " names an unknown settlement");
            continue;
        }
        const std::int64_t ru = at(*x.stl.at(u), 4), rf = at(*x.stl.at(from), 4);
        const auto hp = x.hops(rf);
        if (hp.count(ru) == 0U || hp.at(ru) > mhops) out.push_back("5: e" + n(e) + " host beyond migrate_hops");
        if (at(E, 5) != ru || at(E, 6) != u) out.push_back("8.1: e" + n(e) + " region or settlement is not the host's");
        if (x.val(e, 0) < mmin) out.push_back("5: e" + n(e) + " people below migrate_min");
        if ((x.val(e, 1) == p503api::RULING) != (g == x.civ_race(cu))) {
            out.push_back("6.4.3: e" + n(e) + " standing 3 iff kin");
        }
        const std::vector<std::int64_t> cs = x.cause_list(e);
        static const std::set<std::string> displacing = {"settlement_abandoned", "settlement_abandoned_to", "sacked",
                                                         "settlement_overrun", "settlement_captured",
                                                         "people_expelled"};
        if (cs.size() != 1U || displacing.count(x.kind_of(cs[0])) == 0U) {
            out.push_back("6.4.3: e" + n(e) + " cause is not the displacing event");
        } else if (at(*x.ev.at(cs[0]), 6) != from) {
            out.push_back("6.4.3: e" + n(e) + " cause is not at its origin");
        }
        // at war at the event (ruined origins only: their owner is fixed at the ruin)
        const Row& O = *x.stl.at(from);
        if (!is_null(O.at(12)) && O.at(12).integer <= y) {
            const std::int64_t own = at(O, 8);
            for (const Row& r : t.relation) {
                if (r[4].bytes != "war") continue;
                const bool pr = (at(r, 1) == own && at(r, 3) == cu) || (at(r, 1) == cu && at(r, 3) == own);
                if (pr && at(r, 5) <= y && (is_null(r.at(6)) || r.at(6).integer > y)) {
                    out.push_back("5: e" + n(e) + " host's realm at war with the origin's");
                }
            }
            if (own == cu) out.push_back("6.4.3: e" + n(e) + " host is the origin's own realm");
        }
    }

    // 6: expulsions
    const std::int64_t share_max = rel("expel_share_max");
    std::set<Pair> expelled_years;
    for (const std::int64_t e : x.events_of("people_expelled")) {
        const Row& E = *x.ev.at(e);
        const std::int64_t s = x.part(e, 1), cs = x.part(e, 2), g = x.part(e, 3), y = at(E, 1);
        const std::int64_t q = x.civ_race(cs);
        if (g == q) out.push_back("6: e" + n(e) + " expels the ruling race");
        if (band_before(w, x, prows, q, g, y) != p503api::HATRED) out.push_back("6: e" + n(e) + " pair not in hatred");
        if (x.val(e, 0) < mmin) out.push_back("6: e" + n(e) + " people below migrate_min");
        if (!expelled_years.insert({s, y}).second) out.push_back("6.5: two expulsions from s" + n(s) + " in " + n(y));
        if (const auto pk = x.phase_kind.find(at(E, 4)); pk != x.phase_kind.end() && pk->second == 2) out.push_back("6.5: e" + n(e) + " in a quiet step");
        const auto it = srows.find(s);
        if (it == srows.end()) continue;
        std::optional<std::int64_t> before;
        bool same_year_other = false;
        for (const Row* r : it->second) {
            if (at(*r, 1) != g) continue;
            if (at(*r, 2) < y) before = at(*r, 3);
            if (at(*r, 2) == y && at(*r, 3) != 0) same_year_other = true;
        }
        bool rearrived = false;
        for (const std::int64_t a : x.events_of("refugees_arrived")) {
            rearrived = rearrived || (x.part(a, 1) == s && x.part(a, 4) == g && at(*x.ev.at(a), 1) == y);
        }
        // The share at the expulsion is that of the last row before y unless s changed earlier in the same year
        // (an owner change or arrival before the expulsion in T6 rescales the shares; same-year rows replace).
        bool changed_before = false;
        for (const auto& [id, row] : x.ev) {
            if (id < e && at(*row, 1) == y && at(*row, 6) == s) changed_before = true;
        }
        if (before && *before > share_max && !changed_before) {
            out.push_back("6: e" + n(e) + " takes a share above expel_share_max");
        }
        if (same_year_other && !rearrived) out.push_back("6.5: e" + n(e) + "'s race keeps a share that year");
    }

    // 6.3.6: mixed once per (settlement, race); 6.3.5 standing changes at phase starts
    // Once per entry: `mixed_told` lives on the entry (6.3.6), so a race whose entry left (a row with share 0)
    // and came back may be told again.
    std::map<Pair, std::int64_t> mixed;   // (settlement, race) -> year of the last settlement_mixed
    const std::int64_t mixed_min = rel("mixed_min_permille");
    for (const std::int64_t e : x.events_of("settlement_mixed")) {
        const Pair p{x.part(e, 1), x.part(e, 3)};
        const std::int64_t ye = at(*x.ev.at(e), 1);
        if (const auto prev = mixed.find(p); prev != mixed.end()) {
            bool left = false;
            if (const auto it = srows.find(p.first); it != srows.end()) {
                for (const Row* r : it->second) {
                    left = left || (at(*r, 1) == p.second && at(*r, 3) == 0 && at(*r, 2) >= prev->second &&
                                    at(*r, 2) <= ye);
                }
            }
            if (!left) out.push_back("6.3.6: s" + n(p.first) + " race " + n(p.second) + " mixed twice");
        }
        mixed[p] = ye;
        if (x.val(e, 0) < mixed_min) out.push_back("6.3.6: e" + n(e) + " share below mixed_min_permille");
        if (x.part(e, 3) == x.part(e, 4)) out.push_back("6.3.6: e" + n(e) + " the ruling race mixed with itself");
    }
    std::set<std::int64_t> phase_starts;
    for (const Row& p : t.phase) phase_starts.insert(at(p, 3));
    for (const std::int64_t e : x.events_of("standing_changed")) {
        const std::int64_t s = x.val(e, 0);
        if (s < 0 || s > 2) out.push_back("6.3.5: e" + n(e) + " standing " + n(s));
        if (phase_starts.count(at(*x.ev.at(e), 1)) == 0U) out.push_back("6.3.5: e" + n(e) + " not at a phase start");
    }

    // 8: epithets and the host names
    const wd::KindContent* mk = nullptr;
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind == "monster") mk = &k;
    }
    if (mk != nullptr) {
        std::set<std::string> place_names;
        for (const Row& s : t.settlement) place_names.insert(s[1].bytes);
        for (const Row& r : t.region_name) place_names.insert(r[2].bytes);
        for (const std::string& nm : w.region_names) place_names.insert(nm);
        std::set<std::string> used;
        for (const Row& mr : t.monster) {
            const std::string& name = mr[1].bytes;
            const std::int64_t kind = at(mr, 2);
            if (kind < 1 || static_cast<std::size_t>(kind) > mk->definitions.size()) continue;
            const wd::Record& kd = mk->definitions[static_cast<std::size_t>(kind - 1)].record;
            std::vector<std::string> forms;
            for (const wd::Value& v : kd.get("name_forms").as_array()) forms.push_back(v.as_string());
            if (kd.get_string("wake") == "dead") {
                bool ok = false;
                for (const std::string& fm : forms) {
                    const std::size_t at_p = fm.find("{place}");
                    if (at_p == std::string::npos) continue;
                    const std::string pre = fm.substr(0, at_p), post = fm.substr(at_p + 7);
                    if (name.size() > pre.size() + post.size() && name.compare(0, pre.size(), pre) == 0) {
                        std::string place = name.substr(pre.size(), name.size() - pre.size() - post.size());
                        if (place.rfind("the ", 0) == 0) place[0] = 'T';
                        const std::size_t sp = place.rfind(' ');
                        const std::string bare = sp == std::string::npos ? place : place.substr(0, sp);
                        ok = ok || place_names.count(place) != 0U || place_names.count(bare) != 0U;
                    }
                }
                if (!ok) out.push_back("8: host " + name + " has no place name");
                continue;
            }
            // the form its name was made by: the matching form with the longest literal text
            std::string best;
            std::size_t best_len = 0;
            bool found = false;
            for (const std::string& fm : forms) {
                const std::size_t r0 = fm.find("{root}");
                if (r0 == std::string::npos) continue;
                const std::string pre = fm.substr(0, r0), post = fm.substr(r0 + 6);
                if (name.size() > pre.size() + post.size() && name.compare(0, pre.size(), pre) == 0 &&
                    name.compare(name.size() - post.size(), post.size(), post) == 0 &&
                    (!found || pre.size() + post.size() > best_len)) {
                    best = rel_f::epithet(fm);
                    best_len = pre.size() + post.size();
                    found = true;
                }
            }
            if (!found || best.empty()) continue;
            if (used.count(best) != 0U) {
                bool all_used = true, bare = false;
                for (const std::string& fm : forms) {
                    const std::string ep = rel_f::epithet(fm);
                    if (ep.empty()) bare = true;
                    else if (used.count(ep) == 0U) all_used = false;
                }
                if (!all_used || bare) out.push_back("8: " + name + " repeats the epithet " + best);
            }
            used.insert(best);
        }
    }

    // 9: raid causes, flights for fear
    const std::int64_t chain = mon("raid_chain_years"), fear_years = mon("fear_years"), fear_raids = mon("fear_raids");
    std::map<std::int64_t, std::vector<std::int64_t>> raids_of;   // monster -> raid events
    std::map<std::int64_t, std::vector<std::int64_t>> wakings;    // monster -> waking events
    for (const auto& [e, r] : x.ev) {
        const std::string k = x.kind_of(e);
        if (k == "monster_raid") raids_of[x.part(e, 1)].push_back(e);
        if (k == "monster_appeared" || k == "undead_rose" || k == "monster_woke") wakings[x.part(e, 1)].push_back(e);
    }
    std::int64_t chained = 0;
    for (const auto& [mid, rs] : raids_of) {
        for (const std::int64_t e : rs) {
            const std::int64_t y = at(*x.ev.at(e), 1), s = at(*x.ev.at(e), 6);
            std::vector<std::int64_t> want;
            for (const std::int64_t wv : wakings[mid]) {
                if (wv < e) want.assign(1, wv);
            }
            std::int64_t last = 0;
            for (const std::int64_t r2 : rs) {
                if (r2 < e && at(*x.ev.at(r2), 6) == s && at(*x.ev.at(r2), 1) > y - chain) last = r2;
            }
            if (last != 0) want.push_back(last);
            std::sort(want.begin(), want.end());
            if (x.cause_list(e) != want) out.push_back("9: raid e" + n(e) + " causes are not its waking and last raid");
            chained += last != 0 ? 1 : 0;
        }
    }
    (void)chained;
    for (const std::int64_t e : x.events_of("settlement_fled")) {
        const Row& E = *x.ev.at(e);
        const std::int64_t mid = x.part(e, 1), s = at(E, 6), y = at(E, 1);
        std::vector<std::int64_t> raids;
        for (const std::int64_t r2 : raids_of[mid]) {
            if (r2 < e && at(*x.ev.at(r2), 6) == s && at(*x.ev.at(r2), 1) > y - fear_years) raids.push_back(r2);
        }
        if (static_cast<std::int64_t>(raids.size()) < fear_raids) out.push_back("9.4: e" + n(e) + " fled after too few raids");
        if (raids.size() > 4U) raids.erase(raids.begin(), raids.end() - 4);
        if (x.cause_list(e) != raids) out.push_back("9.4: e" + n(e) + " causes are not the last raids on it");
        bool followed = false;
        for (const auto& [a, r] : x.ev) {
            const std::string k = x.kind_of(a);
            if ((k == "settlement_abandoned" || k == "settlement_abandoned_to") && a > e && at(*r, 6) == s &&
                at(*r, 1) == y) {
                const auto cs = x.cause_list(a);
                followed = followed || std::find(cs.begin(), cs.end(), e) != cs.end();
            }
        }
        if (!followed) out.push_back("9: e" + n(e) + " is not followed by its abandonment");
        if (x.stl.count(s) != 0U) {
            const Row& S = *x.stl.at(s);
            if (at(S, 9) != 1 && at(S, 9) != 2) out.push_back("9: fled s" + n(s) + " was not a hamlet or village");
            if (at(S, 14) != p503api::ENDING_FLED) out.push_back("11.3: fled s" + n(s) + " ending is not 6");
            const std::int64_t owner = x.part(e, 2);
            bool other = false;
            for (const auto& [s2, R] : x.stl) {
                if (s2 == s) continue;
                const bool ownerish = at(*R, 5) == owner || at(*R, 8) == owner;
                other = other || (ownerish && at(*R, 6) <= y && (is_null(R->at(12)) || R->at(12).integer >= y));
            }
            for (const Row& cr : t.settlement_census) {
                other = other || (at(cr, 0) != s && at(cr, 2) == owner && at(cr, 1) <= y && (at(cr, 4) == 1 || at(cr, 4) == 2));
            }
            if (!other) out.push_back("9: fled s" + n(s) + " was its owner's last");
        }
    }

    // 10: fading
    std::map<std::int64_t, std::vector<const Row*>> sites_at;
    for (const Row& s : t.site) {
        if (at(s, 6) != 0) sites_at[at(s, 6)].push_back(&s);
    }
    for (const Row& S : t.settlement) {
        const std::int64_t s = at(S, 0);
        if (at(S, 10) == 4 && o.ghost_fade_zero && at(S, 14) != 1) {
            out.push_back("10: faded s" + n(s) + " with ending " + n(at(S, 14)));
        }
        if (at(S, 10) == 4) {
            for (const std::int64_t e : x.events_of("ruin_faded")) {
                if (at(*x.ev.at(e), 6) != s) continue;
                const std::int64_t yf = at(*x.ev.at(e), 1);
                for (const Row* si : sites_at[s]) {
                    if (at(*si, 9) <= yf && (is_null(si->at(11)) || si->at(11).integer > yf)) {
                        out.push_back("10: s" + n(s) + " faded while site si" + n(at(*si, 0)) + " was open");
                    }
                }
            }
        }
        // ending 6 (11.3): an abandonment citing a flight
        if (at(S, 10) == 3) {
            const std::string k = x.kind_of(at(S, 13));
            bool fled = false;
            if (k == "settlement_abandoned" || k == "settlement_abandoned_to") {
                for (const std::int64_t cz : x.cause_list(at(S, 13))) fled = fled || x.kind_of(cz) == "settlement_fled";
            }
            if (fled != (at(S, 14) == p503api::ENDING_FLED)) out.push_back("11.3: s" + n(s) + " ending 6 derivation");
        }
    }

    // 11: no race dies out (every race that founded a realm has one alive at E)
    std::map<std::int64_t, bool> alive;
    for (const Row& cv : t.civilization) alive[at(cv, 2)] = alive[at(cv, 2)] || is_null(cv.at(8));
    for (const auto& [r, a] : alive) {
        if (!a) out.push_back("11: race " + n(r) + " died out");
    }
    return out;
}

// --- The count lines of p5-03-relations.txt ------------------------------------------------------------

inline std::string rr_line(const World& w) {
    const Index x(w);
    std::int64_t present = 0;
    for (const RaceInfo& r : w.races) present += r.present ? 1 : 0;
    const auto prows = pair_rows(w);
    std::int64_t bands[3] = {0, 0, 0};
    for (const auto& [p, rs] : prows) {
        const std::int64_t b = at(*rs.back(), 4);
        if (b >= 0 && b <= 2) ++bands[b];
    }
    return "pairs " + n(present * (present - 1) / 2) + " met " +
           n(static_cast<std::int64_t>(x.events_of("peoples_met").size())) + " turned " +
           n(static_cast<std::int64_t>(x.events_of("disposition_turned").size())) + " hatred " + n(bands[0]) +
           " wariness " + n(bands[1]) + " friendship " + n(bands[2]);
}

inline std::string mx_line(const World& w) {
    const Index x(w);
    const std::int64_t mixed_min = p503api::tuning_int(*w.content, "relations", "mixed_min_permille");
    const auto srows = share_rows(w);
    std::int64_t mixed = 0;
    for (const auto& [s, S] : x.stl) {
        if (at(*S, 10) != 1 && at(*S, 10) != 2) continue;
        const auto it = srows.find(s);
        if (it == srows.end()) continue;
        bool any = false;
        for (const auto& [g, v] : in_force(it->second, w.end_year)) any = any || (v.second != 3 && v.first >= mixed_min);
        mixed += any ? 1 : 0;
    }
    std::set<std::int64_t> ever;
    for (const std::int64_t e : x.events_of("settlement_mixed")) ever.insert(x.part(e, 1));
    std::int64_t people = 0, expelled = 0;
    for (const std::int64_t e : x.events_of("refugees_arrived")) people += x.val(e, 0);
    for (const std::int64_t e : x.events_of("people_expelled")) expelled += x.val(e, 0);
    return "mixed " + n(mixed) + " mixed_ever " + n(static_cast<std::int64_t>(ever.size())) + " arrivals " +
           n(static_cast<std::int64_t>(x.events_of("refugees_arrived").size())) + " people " + n(people) +
           " expelled " + n(static_cast<std::int64_t>(x.events_of("people_expelled").size())) + " expelled_people " +
           n(expelled);
}

inline std::string f_line(const World& w) {
    const Index x(w);
    std::int64_t chained = 0, lost31 = 0, old = 0, towns = 0;
    for (const std::int64_t e : x.events_of("monster_raid")) chained += x.cause_list(e).size() == 2U ? 1 : 0;
    for (const std::int64_t e : x.events_of("settlement_abandoned_to")) lost31 += x.val(e, 1) > 0 ? 1 : 0;
    for (const auto& [s, S] : x.stl) {
        if (at(*S, 10) == 3 && !is_null(S->at(12)) && w.end_year - S->at(12).integer > 1500) ++old;
    }
    for (const std::int64_t e : x.events_of("settlement_overrun")) {
        const auto it = x.stl.find(at(*x.ev.at(e), 6));
        towns += it != x.stl.end() && at(*it->second, 9) >= 3 ? 1 : 0;
    }
    auto cnt = [&](const char* k) { return n(static_cast<std::int64_t>(x.events_of(k).size())); };
    return "raids " + cnt("monster_raid") + " chained " + n(chained) + " fled " + cnt("settlement_fled") +
           " abandoned16 " + cnt("settlement_abandoned") + " abandoned31 " + cnt("settlement_abandoned_to") +
           " lost31 " + n(lost31) + " ruins_old " + n(old) + " overruns " + cnt("settlement_overrun") +
           " town_overruns " + n(towns);
}

// The rows of events of kinds 56 to 62, in id order (golden `table events_56_62`).
inline std::vector<Row> new_kind_events(const World& w) {
    std::vector<Row> out;
    for (const Row& e : w.rows.t.event) {
        if (at(e, 3) >= 56 && at(e, 3) <= 62) out.push_back(e);
    }
    return out;
}

}  // namespace p503check
