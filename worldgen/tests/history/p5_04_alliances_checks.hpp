#pragma once

// The invariants of spec P5-04 13 that rows show (items 1, 2, 3 (origin), 4, 5, 7, 8 as AL9's hard bound, 10's
// shelter rule, 11 to 14), on the rows of a run or a world file, as the reference's invariant_violations
// (reference/p5_04_alliances_reference.py). Messages name the item first. No library name beyond P5-03's tests'.

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "p5_03_relations_checks.hpp"

namespace p504check {

namespace wd = worldgen::data;
using p503check::at;
using p503check::Index;
using p503check::World;
using Row = eng::Row;

inline constexpr std::int64_t ISLET_MAX_DRY_CELLS = 1172;   // P3-07's islet rule (digest.hpp)

struct Geo {
    std::map<std::int64_t, std::int64_t> ore;   // region -> ore_reach_cells
    std::set<std::int64_t> land;                // non-islet regions
};
inline Geo geo_of_inputs(const eng::HistoryInputs& in) {
    Geo g;
    for (const auto& r : in.regions) {
        g.ore[r.id] = r.ore_reach_cells;
        if (r.cells - r.lake_cells >= ISLET_MAX_DRY_CELLS) g.land.insert(r.id);
    }
    return g;
}
inline Geo geo_of_file(const worldgen::db::WorldReader& r) {
    Geo g;
    for (const auto& x : chrono_test::rows_of(r, "region")) {
        g.ore[x.at(0).integer] = x.at(27).integer;
        if (x.at(10).integer - x.at(11).integer >= ISLET_MAX_DRY_CELLS) g.land.insert(x.at(0).integer);
    }
    return g;
}

inline bool is_null(const Row& r, std::size_t i) { return r.at(i).type == worldgen::db::ValueType::null; }
inline std::int64_t until_or(const Row& r, std::int64_t v) { return is_null(r, 6) ? v : at(r, 6); }

// The tuning and race data the checks read.
struct Data {
    std::int64_t allies_max = 0, trade_max = 0, term_years = 0, withheld_to_break = 0, plunder_permille = 0,
                 cap_permille = 0, ore_min = 0, settlement_min = 0, shelter_years = 0;
    std::vector<std::string> forms;
    std::map<std::string, std::vector<std::string>> origin;   // race key -> origin_allies
    std::map<std::string, std::int64_t> mining;
};
inline Data data_of(const wd::ContentSet& c) {
    Data d;
    const auto ti = [&](const char* f) { return p503api::tuning_int(c, "alliances", f); };
    d.allies_max = ti("allies_max");
    d.trade_max = ti("trade_max");
    d.term_years = ti("term_years");
    d.withheld_to_break = ti("withheld_to_break");
    d.plunder_permille = ti("spoils_plunder_permille");
    d.cap_permille = ti("spoils_cap_permille");
    d.ore_min = ti("mine_ore_min");
    d.settlement_min = ti("mine_settlement_min");
    d.shelter_years = p503api::tuning_int(c, "conflict", "shelter_years");
    const wd::Definition* al = p503check::definition(c, "history", "alliances");
    REQUIRE_MESSAGE(al != nullptr, "the content has no history.alliances (P5-04 4.2)");
    const wd::Value* forms = al->record.find("mine_forms");
    REQUIRE(forms != nullptr);
    for (const wd::Value& v : forms->as_array()) d.forms.push_back(v.as_string());
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (const wd::Definition& r : k.definitions) {
            const wd::Value* o = r.record.find("origin_allies");
            REQUIRE_MESSAGE(o != nullptr, "race." << r.id << " has no origin_allies (P5-04 4.1)");
            for (const wd::Value& v : o->as_array()) d.origin[r.id].push_back(v.as_string());
            d.mining[r.id] = r.record.get_int("mining_permille");
        }
    }
    return d;
}

// 6.12: mine_forms[(id - 1) mod n] with {settlement}, a leading "The " of the town lowered (reading P8).
inline std::string mine_name(const std::vector<std::string>& forms, std::int64_t id, std::string town) {
    const std::string f = forms[static_cast<std::size_t>((id - 1) % static_cast<std::int64_t>(forms.size()))];
    const std::size_t p = f.find("{settlement}");
    if (p > 0 && town.rfind("The ", 0) == 0) town = "the " + town.substr(4);
    return f.substr(0, p) + town + f.substr(p + 12);
}

class Rows {
public:
    explicit Rows(const World& w) : w_(w), x_(w) {
        for (const Row& r : w.rows.t.relation) rel_[r.at(4).bytes].push_back(&r);
        for (const auto& [id, key] : x_.key) {
            const auto& t = w.rows.t;
            for (const Row& r : t.event_kind_role) {
                if (at(r, 0) == id) roles_[key][r.at(2).bytes] = at(r, 1);
            }
            for (const Row& r : t.event_kind_value) {
                if (at(r, 0) == id) vals_[key][r.at(2).bytes] = at(r, 1);
            }
        }
        for (const Row& c : w.rows.t.event_cause) kids_[at(c, 1)].push_back(at(c, 0));
        for (const Row& s : w.rows.t.site) site_[at(s, 0)] = &s;
        for (const Row& c : w.rows.t.settlement_census) census_[at(c, 0)].push_back(&c);
        for (const Row& r : w.rows.t.region_control) rc_[at(r, 0)].push_back(&r);
    }
    [[nodiscard]] const Index& x() const { return x_; }
    [[nodiscard]] std::string kind(std::int64_t e) const { return x_.kind_of(e); }
    [[nodiscard]] std::int64_t year(std::int64_t e) const { return at(*x_.ev.at(e), 1); }
    [[nodiscard]] std::int64_t region(std::int64_t e) const { return at(*x_.ev.at(e), 5); }
    [[nodiscard]] std::vector<std::int64_t> role(std::int64_t e, const std::string& r) const {
        std::vector<std::int64_t> out;
        const auto k = roles_.find(kind(e));
        if (k == roles_.end() || k->second.count(r) == 0U) return out;
        const std::int64_t seq = k->second.at(r);
        const auto it = x_.parts.find(e);
        if (it == x_.parts.end()) return out;
        std::vector<const Row*> ps = it->second;
        std::sort(ps.begin(), ps.end(), [](const Row* a, const Row* b) { return at(*a, 1) < at(*b, 1); });
        for (const Row* p : ps) {
            if (at(*p, 2) == seq) out.push_back(at(*p, 4));
        }
        return out;
    }
    [[nodiscard]] std::int64_t one(std::int64_t e, const std::string& r) const {
        const auto v = role(e, r);
        return v.empty() ? 0 : v[0];
    }
    [[nodiscard]] std::int64_t value(std::int64_t e, const std::string& v) const {
        const auto k = vals_.find(kind(e));
        if (k == vals_.end() || k->second.count(v) == 0U) return -1;
        return x_.val(e, static_cast<std::size_t>(k->second.at(v) - 1));
    }
    [[nodiscard]] std::vector<std::int64_t> of(const std::string& k) const { return x_.events_of(k); }
    [[nodiscard]] const std::vector<const Row*>& rows(const std::string& kind) const {
        static const std::vector<const Row*> none;
        const auto it = rel_.find(kind);
        return it == rel_.end() ? none : it->second;
    }
    [[nodiscard]] std::vector<const Row*> pair_rows(const std::string& kind, std::int64_t a, std::int64_t b) const {
        std::vector<const Row*> out;
        for (const Row* r : rows(kind)) {
            if (at(*r, 1) == std::min(a, b) && at(*r, 3) == std::max(a, b)) out.push_back(r);
        }
        return out;
    }
    [[nodiscard]] static bool open_at_event(const Row& r, std::int64_t e) {
        return at(r, 7) < e && (is_null(r, 6) || at(r, 8) > e);
    }
    [[nodiscard]] bool allied_at_event(std::int64_t a, std::int64_t b, std::int64_t e) const {
        for (const Row* r : pair_rows("alliance", a, b)) {
            if (open_at_event(*r, e)) return true;
        }
        return false;
    }
    [[nodiscard]] const std::vector<std::int64_t>& kids(std::int64_t e) const {
        static const std::vector<std::int64_t> none;
        const auto it = kids_.find(e);
        return it == kids_.end() ? none : it->second;
    }
    [[nodiscard]] std::vector<std::int64_t> war_events(std::int64_t d) const {
        std::vector<std::int64_t> out;
        for (const std::int64_t b : kids(d)) {
            const std::string k = kind(b);
            if (k != "battle" && k != "battle_at_pass" && k != "allied_battle") continue;
            out.push_back(b);
            for (const std::int64_t s : kids(b)) {
                if (kind(s) != "siege") continue;
                out.push_back(s);
                for (const std::int64_t f : kids(s)) {
                    if (kind(f) == "sacked" || kind(f) == "settlement_captured") out.push_back(f);
                }
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    [[nodiscard]] std::pair<std::int64_t, std::int64_t> winner_loser(std::int64_t e) const {
        const std::string k = kind(e);
        if (k == "peace" || k == "war_won") return {one(e, "winner"), one(e, "loser")};
        if (k == "civ_conquered") return {one(e, "conqueror"), one(e, "civilization")};
        return {0, 0};
    }
    [[nodiscard]] const Row* site(std::int64_t id) const {
        const auto it = site_.find(id);
        return it == site_.end() ? nullptr : it->second;
    }
    [[nodiscard]] const Row* census_at(std::int64_t s, std::int64_t y) const {
        const Row* best = nullptr;
        const auto it = census_.find(s);
        if (it == census_.end()) return nullptr;
        for (const Row* r : it->second) {
            if (at(*r, 1) <= y) best = r;
        }
        return best;
    }
    [[nodiscard]] std::int64_t holder_at(std::int64_t region, std::int64_t y) const {
        const auto it = rc_.find(region);
        if (it == rc_.end()) return 0;
        for (const Row* r : it->second) {
            if (at(*r, 1) <= y && (is_null(*r, 2) || y < at(*r, 2))) return at(*r, 3);
        }
        return 0;
    }
    // The longest run of years in which some bloc's share (6.2, the islet rule) is above `limit`.
    [[nodiscard]] std::int64_t longest_bloc_run(const p504check::Geo& g, std::int64_t limit) const {
        std::set<std::int64_t> ys{0, w_.end_year};
        for (const Row& r : w_.rows.t.region_control) {
            ys.insert(at(r, 1));
            if (!is_null(r, 2)) ys.insert(at(r, 2));
        }
        for (const Row* r : rows("alliance")) {
            ys.insert(at(*r, 5));
            if (!is_null(*r, 6)) ys.insert(at(*r, 6));
        }
        const std::int64_t N = static_cast<std::int64_t>(g.land.size());
        if (N == 0) return 0;
        std::vector<std::int64_t> yv(ys.begin(), ys.end());
        std::int64_t run = 0, best = 0, last_end = -1;
        for (std::size_t i = 0; i + 1 < yv.size(); ++i) {
            const std::int64_t y0 = yv[i], y1 = yv[i + 1];
            if (y0 < 0 || y1 > w_.end_year) continue;
            std::map<std::int64_t, std::int64_t> held;
            for (const Row& r : w_.rows.t.region_control) {
                if (g.land.count(at(r, 0)) != 0U && at(r, 1) <= y0 && (is_null(r, 2) || y0 < at(r, 2))) ++held[at(r, 3)];
            }
            std::map<std::int64_t, std::vector<std::int64_t>> allies;
            for (const Row* r : rows("alliance")) {
                if (at(*r, 5) <= y0 && until_or(*r, w_.end_year + 1) > y0) {
                    allies[at(*r, 1)].push_back(at(*r, 3));
                    allies[at(*r, 3)].push_back(at(*r, 1));
                }
            }
            std::int64_t mx = 0;
            for (const auto& [c, zs] : allies) {
                std::int64_t bs = held[c] * 1000 / N;
                for (const std::int64_t z : zs) bs += held[z] * 1000 / N;
                mx = std::max(mx, std::min<std::int64_t>(1000, bs));
            }
            if (mx > limit) {
                run = (last_end == y0 ? run : 0) + (y1 - y0);
                last_end = y1;
                best = std::max(best, run);
            } else {
                run = 0;
                last_end = -1;
            }
        }
        return best;
    }

private:
    const World& w_;
    Index x_;
    std::map<std::string, std::vector<const Row*>> rel_;
    std::map<std::string, std::map<std::string, std::int64_t>> roles_, vals_;
    std::map<std::int64_t, std::vector<std::int64_t>> kids_;
    std::map<std::int64_t, const Row*> site_;
    std::map<std::int64_t, std::vector<const Row*>> census_, rc_;
};

inline std::string s(std::int64_t v) { return std::to_string(v); }

inline std::vector<std::string> violations(const World& w, const Geo& g, const Data& d) {
    std::vector<std::string> out;
    const Rows r(w);
    const Index& x = r.x();
    const auto alive = [&](std::int64_t c, std::int64_t y) {
        const Row& cv = *x.civ.at(c);
        return at(cv, 6) <= y && (is_null(cv, 8) || y < at(cv, 8));
    };
    // 1
    for (const auto& [kind, cap] : std::vector<std::pair<std::string, std::int64_t>>{{"alliance", d.allies_max},
                                                                                  {"trade", d.trade_max}}) {
        std::set<std::int64_t> ys;
        for (const Row* row : r.rows(kind)) {
            ys.insert(at(*row, 5));
            if (!is_null(*row, 6)) ys.insert(at(*row, 6));
            if (!(at(*row, 0) == 3 && at(*row, 2) == 3 && at(*row, 1) < at(*row, 3))) {
                out.push_back("1: " + kind + " row of c" + s(at(*row, 1)) + " c" + s(at(*row, 3)) + " not a < b");
                continue;
            }
            for (const std::int64_t c : {at(*row, 1), at(*row, 3)}) {
                const Row& cv = *x.civ.at(c);
                if (at(*row, 5) < at(cv, 6) || (!is_null(cv, 8) && until_or(*row, 1 << 30) > at(cv, 8))) {
                    out.push_back("1: " + kind + " row since " + s(at(*row, 5)) + " outlives c" + s(c));
                }
            }
        }
        for (const std::int64_t y : ys) {
            std::map<std::int64_t, std::int64_t> n;
            std::set<std::pair<std::int64_t, std::int64_t>> pairs;
            for (const Row* row : r.rows(kind)) {
                if (!(at(*row, 5) <= y && until_or(*row, 1 << 30) > y)) continue;
                ++n[at(*row, 1)];
                ++n[at(*row, 3)];
                if (!pairs.insert({at(*row, 1), at(*row, 3)}).second) {
                    out.push_back("1: two " + kind + " rows of c" + s(at(*row, 1)) + " c" + s(at(*row, 3)) + " at " + s(y));
                }
            }
            for (const auto& [c, k] : n) {
                if (k > cap) out.push_back("1: c" + s(c) + " has " + s(k) + " " + kind + " rows at " + s(y));
            }
        }
    }
    // 2, 7
    for (const std::int64_t b : r.of("alliance_betrayed")) {
        const std::int64_t bx = r.one(b, "betrayer"), by = r.one(b, "betrayed");
        for (const Row* row : r.pair_rows("alliance", bx, by)) {
            if (at(*row, 7) > b) out.push_back("2: alliance of betrayed pair opens at e" + s(at(*row, 7)));
        }
        bool cited = false;
        for (const std::int64_t c : x.cause_list(b)) {
            cited = cited || (r.kind(c) == "war_declared" && r.year(c) == r.year(b) && r.one(c, "attacker") == bx &&
                              r.one(c, "defender") == by);
        }
        if (!cited) out.push_back("7: alliance_betrayed e" + s(b) + " cites no declaration of its pair that year");
    }
    // 3
    for (const std::int64_t e : r.of("alliance_formed")) {
        const std::int64_t a = r.one(e, "ally_a"), b = r.one(e, "ally_b");
        if (!(a < b)) out.push_back("3: alliance_formed e" + s(e) + ": ally_a not below ally_b");
        const std::int64_t basis = r.value(e, "basis");
        if (basis == 0) {
            const std::string ka = x.race_key.at(x.civ_race(a)), kb = x.race_key.at(x.civ_race(b));
            const auto& oa = d.origin.at(ka);
            const auto& ob = d.origin.at(kb);
            if (std::find(oa.begin(), oa.end(), kb) == oa.end() || std::find(ob.begin(), ob.end(), ka) == ob.end()) {
                out.push_back("3: origin alliance e" + s(e) + " of " + ka + " and " + kb);
            }
            if (!r.role(e, "foe").empty()) out.push_back("3: origin alliance e" + s(e) + " with a foe");
        } else if ((basis == 1 || basis == 2) && r.role(e, "foe").empty()) {
            out.push_back("3: alliance_formed e" + s(e) + " without its foe");
        }
    }
    // 4 and 10
    std::map<std::int64_t, std::int64_t> war_of_join;
    for (const std::int64_t j : r.of("ally_joined_war")) {
        const std::int64_t z = r.one(j, "ally"), p = r.one(j, "partner"), en = r.one(j, "enemy");
        std::vector<std::int64_t> decl;
        for (const std::int64_t c : x.cause_list(j)) {
            if (r.kind(c) == "war_declared") decl.push_back(c);
        }
        if (decl.size() != 1U) {
            out.push_back("4: ally_joined_war e" + s(j) + " cites " + s(static_cast<std::int64_t>(decl.size())) +
                          " declarations");
            continue;
        }
        const std::int64_t dd = decl[0];
        war_of_join[j] = dd;
        const std::set<std::int64_t> sides{r.one(dd, "attacker"), r.one(dd, "defender")};
        if (r.year(dd) != r.year(j) || sides != std::set<std::int64_t>{p, en}) {
            out.push_back("4: ally_joined_war e" + s(j) + ": its declaration's year or sides");
        }
        if ((r.value(j, "side") == 0) != (r.one(dd, "defender") == p)) out.push_back("4: e" + s(j) + ": side");
        if (!r.allied_at_event(z, p, j)) out.push_back("4: e" + s(j) + ": the ally not allied with its partner");
        if (r.allied_at_event(z, en, j)) out.push_back("4: e" + s(j) + ": the ally allied with the enemy");
        std::int64_t rows = 0;
        for (const Row* row : r.rows("war")) rows += (at(*row, 7) == j && at(*row, 1) == z && at(*row, 3) == en) ? 1 : 0;
        if (rows != 1) out.push_back("4: e" + s(j) + ": no joined war row");
        if (r.year(j) < at(*x.civ.at(en), 6) + d.shelter_years) out.push_back("10: e" + s(j) + ": a sheltered enemy");
    }
    // 5
    for (const std::int64_t b : r.of("allied_battle")) {
        const std::int64_t W = r.one(b, "winner"), L = r.one(b, "loser");
        const auto wa = r.role(b, "winner_ally"), la = r.role(b, "loser_ally");
        if (wa.empty() && la.empty()) out.push_back("5: allied_battle e" + s(b) + " without an ally");
        std::vector<std::pair<std::int64_t, std::int64_t>> need;
        for (const std::int64_t z : wa) need.emplace_back(z, L);
        for (const std::int64_t z : la) need.emplace_back(z, W);
        for (const auto& [z, en] : need) {
            bool ok = false;
            for (const Row* row : r.rows("war")) {
                ok = ok || (at(*row, 1) == z && at(*row, 3) == en && r.kind(at(*row, 7)) == "ally_joined_war" &&
                            Rows::open_at_event(*row, b));
            }
            if (!ok) out.push_back("5: allied_battle e" + s(b) + ": c" + s(z) + " has no open joined row");
        }
        if (!std::is_sorted(wa.begin(), wa.end()) || !std::is_sorted(la.begin(), la.end())) {
            out.push_back("5: allied_battle e" + s(b) + ": allies not ascending");
        }
    }
    // 8 (AL9's hard bound; reading P7)
    if (const std::int64_t run = r.longest_bloc_run(g, 800); run > 1000) {
        out.push_back("8: a bloc above 800 permille for " + s(run) + " years");
    }
    // 11
    std::set<std::pair<std::int64_t, std::int64_t>> seen;
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> per_war;   // V, total
    std::vector<std::int64_t> spoils = r.of("spoils_shared");
    for (const std::int64_t e : r.of("spoils_withheld")) spoils.push_back(e);
    std::sort(spoils.begin(), spoils.end());
    for (const std::int64_t e : spoils) {
        const std::int64_t P = r.one(e, "principal"), z = r.one(e, "ally"), en = r.one(e, "enemy");
        std::vector<std::int64_t> js, ends;
        for (const std::int64_t c : x.cause_list(e)) {
            const std::string k = r.kind(c);
            if (k == "ally_joined_war" && r.one(c, "ally") == z) js.push_back(c);
            if (k == "peace" || k == "war_won" || k == "civ_conquered") ends.push_back(c);
        }
        if (js.size() != 1U || ends.size() != 1U) {
            out.push_back("11: spoils e" + s(e) + " without its war");
            continue;
        }
        if (r.year(e) < r.year(ends[0]) || r.winner_loser(ends[0]) != std::make_pair(P, en)) {
            out.push_back("11: spoils e" + s(e) + ": its end's winner or year");
        }
        const auto wj = war_of_join.find(js[0]);
        if (wj == war_of_join.end()) continue;
        const std::int64_t dd = wj->second;
        if (r.region(e) != r.region(dd)) out.push_back("11: spoils e" + s(e) + " not in the war's goal region");
        bool fought = false;
        std::int64_t tribute = 0, plunder = 0;
        for (const std::int64_t b : r.war_events(dd)) {
            if (r.kind(b) == "allied_battle") {
                const auto wa = r.role(b, "winner_ally"), la = r.role(b, "loser_ally");
                fought = fought || std::find(wa.begin(), wa.end(), z) != wa.end() ||
                         std::find(la.begin(), la.end(), z) != la.end();
            }
            if (r.kind(b) == "sacked" && r.one(b, "attacker") == P) plunder += r.value(b, "dead") + r.value(b, "survivors");
        }
        for (const std::int64_t t : r.kids(ends[0])) {
            if (r.kind(t) == "tribute" && r.one(t, "payee") == P) tribute += r.value(t, "people");
        }
        if (!fought) out.push_back("11: spoils e" + s(e) + ": its ally fought in no battle of the war");
        for (const std::int64_t wd_ : r.of("ally_withdrew")) {
            if (js[0] < wd_ && wd_ < ends[0] && r.one(wd_, "ally") == z && r.one(wd_, "partner") == P &&
                r.one(wd_, "enemy") == en) {
                out.push_back("11: spoils e" + s(e) + ": its ally withdrew");
            }
        }
        if (!seen.insert({dd, z}).second) out.push_back("11: two spoils events of one war and ally");
        const std::int64_t sh = r.value(e, "share");
        const std::int64_t V = tribute + plunder * d.plunder_permille / 1000;
        if (sh < 1 || sh > V * d.cap_permille / 1000) out.push_back("11: spoils e" + s(e) + " share " + s(sh) + " of V " + s(V));
        per_war[dd].first = V;
        per_war[dd].second += sh;
    }
    for (const auto& [dd, vt] : per_war) {
        if (vt.second > vt.first) out.push_back("11: war e" + s(dd) + ": shares above V");
    }
    const std::set<std::int64_t> spoils_set(spoils.begin(), spoils.end());
    for (const Row& rc : w.rows.t.region_control) {
        if (spoils_set.count(at(rc, 4)) != 0U) out.push_back("11: a region changes hands by a share");
    }
    for (const Row& rl : w.rows.t.relation) {
        if (spoils_set.count(at(rl, 7)) != 0U || (!is_null(rl, 6) && spoils_set.count(at(rl, 8)) != 0U)) {
            out.push_back("11: a relation row by a share");
        }
    }
    // 12
    for (const std::int64_t e : r.of("alliance_spurned")) {
        const std::int64_t z = r.one(e, "ally"), P = r.one(e, "principal");
        const Row* closed = nullptr;
        for (const Row* row : r.pair_rows("alliance", z, P)) {
            if (!is_null(*row, 6) && at(*row, 8) == e) closed = row;
        }
        if (closed == nullptr) {
            out.push_back("12: alliance_spurned e" + s(e) + " closes no alliance");
            continue;
        }
        if (r.value(at(*closed, 7), "basis") == 0) out.push_back("12: e" + s(e) + " spurns an origin alliance");
        std::int64_t last = at(*closed, 7);
        for (const std::int64_t n : r.of("alliance_renewed")) {
            const std::set<std::int64_t> pr{r.one(n, "ally_a"), r.one(n, "ally_b")};
            if (n > last && n < e && pr == std::set<std::int64_t>{z, P}) last = n;
        }
        std::vector<std::int64_t> wh;
        for (const std::int64_t c : x.cause_list(e)) {
            if (r.kind(c) == "spoils_withheld") wh.push_back(c);
        }
        if (wh.size() != 1U || r.one(wh[0], "ally") != z || r.one(wh[0], "principal") != P || wh[0] < last) {
            out.push_back("12: e" + s(e) + " cites no withheld share of its pair after its last renewal");
        } else if (r.region(e) != r.region(wh[0])) {
            out.push_back("12: e" + s(e) + " not in its withheld share's region");
        }
        if (r.value(e, "reason") == 0) {
            std::int64_t n = 0;
            for (const std::int64_t c : r.of("spoils_withheld")) {
                const std::set<std::int64_t> pr{r.one(c, "ally"), r.one(c, "principal")};
                n += (c > last && c < e && pr == std::set<std::int64_t>{z, P}) ? 1 : 0;
            }
            if (n < d.withheld_to_break) out.push_back("12: e" + s(e) + " reason 0 with " + s(n) + " withheld");
        }
    }
    // 13
    std::map<std::int64_t, std::int64_t> per_settlement;
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<std::pair<std::int64_t, std::int64_t>>> spans;
    for (const Row& m : w.rows.t.site) {
        if (at(m, 2) != 3) continue;
        const std::int64_t id = at(m, 0), R = at(m, 3), st = at(m, 6), c = at(m, 7), y0 = at(m, 9);
        if (++per_settlement[st] > 1) out.push_back("13: settlement s" + s(st) + " has two mines");
        spans[{c, R}].emplace_back(y0, is_null(m, 11) ? w.end_year + 1 : at(m, 11));
        const auto sit = x.stl.find(st);
        if (sit == x.stl.end() || at(*sit->second, 4) != R) {
            out.push_back("13: mine si" + s(id) + ": its settlement not in its region");
            continue;
        }
        const Row* cen = r.census_at(st, y0);
        if ((cen != nullptr && at(*cen, 2) != c) || r.holder_at(R, y0) != c) {
            out.push_back("13: mine si" + s(id) + ": town or region not its founder's at " + s(y0));
        }
        const std::int64_t pop = cen != nullptr ? at(*cen, 5) : at(*sit->second, 11);
        if (pop < d.settlement_min) out.push_back("13: mine si" + s(id) + ": a town of " + s(pop));
        if (g.ore.count(R) == 0U || g.ore.at(R) < d.ore_min) out.push_back("13: mine si" + s(id) + ": no ore");
        if (d.mining.at(x.race_key.at(x.civ_race(c))) <= 0) out.push_back("13: mine si" + s(id) + " of a race that does not mine");
        if (at(m, 8) != 0) out.push_back("13: mine si" + s(id) + " with a monster");
        const std::int64_t oe = at(m, 10);
        if (x.ev.count(oe) == 0U || r.kind(oe) != "mine_opened" || r.region(oe) != R || r.one(oe, "civilization") != c ||
            r.one(oe, "site") != id || r.year(oe) != y0) {
            out.push_back("13: mine si" + s(id) + ": its origin event");
        }
        if (m.at(1).bytes != mine_name(d.forms, id, sit->second->at(1).bytes)) {
            out.push_back("13: mine si" + s(id) + " named " + m.at(1).bytes);
        }
        if (at(m, 4) != at(*sit->second, 2) || at(m, 5) != at(*sit->second, 3)) out.push_back("13: mine si" + s(id) + " cell");
        if (is_null(m, 11)) {
            if (at(m, 13) != 0) out.push_back("13: open mine si" + s(id) + " with an ending");
        } else {
            if (at(m, 13) != 4 && at(m, 13) != 5) out.push_back("13: mine si" + s(id) + " ending " + s(at(m, 13)));
            if (x.ev.count(at(m, 12)) == 0U || r.year(at(m, 12)) != at(m, 11)) {
                out.push_back("13: mine si" + s(id) + ": its ending event's year");
            }
        }
    }
    for (auto& [k, iv] : spans) {
        std::sort(iv.begin(), iv.end());
        for (std::size_t i = 1; i < iv.size(); ++i) {
            if (iv[i].first < iv[i - 1].second) out.push_back("13: c" + s(k.first) + " two open mines in r" + s(k.second));
        }
    }
    // 14
    std::map<std::int64_t, std::int64_t> sold;
    for (const std::int64_t a : r.of("arms_sold")) {
        const std::int64_t sl = r.one(a, "seller"), b = r.one(a, "buyer"), si = r.one(a, "site");
        const Row* m = r.site(si);
        if (m == nullptr || at(*m, 2) != 3 || at(*m, 7) != sl || !(at(*m, 10) < a && (is_null(*m, 11) || at(*m, 12) > a))) {
            out.push_back("14: arms_sold e" + s(a) + ": not an open mine of the seller");
        }
        std::vector<const Row*> tr;
        for (const Row* row : r.pair_rows("trade", sl, b)) {
            if (Rows::open_at_event(*row, a)) tr.push_back(row);
        }
        if (tr.size() != 1U) {
            out.push_back("14: arms_sold e" + s(a) + ": no open trade");
        } else if (++sold[at(*tr[0], 7)] > 1) {
            out.push_back("14: two arms sales on one trade");
        }
        for (const Row& mm : w.rows.t.site) {
            if (at(mm, 2) == 3 && at(mm, 7) == b && at(mm, 10) < a && (is_null(mm, 11) || at(mm, 12) > a)) {
                out.push_back("14: arms_sold e" + s(a) + ": the buyer has an open mine");
            }
        }
    }
    return out;
}

// The causes and closings of 6.5 to 6.13 and 8.3's cause list (W6), as rows show them.
inline std::vector<std::string> cause_violations(const World& w) {
    std::vector<std::string> out;
    const Rows r(w);
    const Index& x = r.x();
    const auto cites = [&](std::int64_t e, const std::string& k) {
        std::vector<std::int64_t> v;
        for (const std::int64_t c : x.cause_list(e)) {
            if (r.kind(c) == k) v.push_back(c);
        }
        return v;
    };
    const auto pair_of = [&](std::int64_t e, const char* a, const char* b) {
        return std::set<std::int64_t>{r.one(e, a), r.one(e, b)};
    };
    // 6.5: a refusal answers a defence call, never of an origin alliance; it cites the declaration and the alliance
    for (const std::int64_t e : r.of("ally_refused")) {
        const auto d = cites(e, "war_declared");
        const auto f = cites(e, "alliance_formed"), n = cites(e, "alliance_renewed");
        if (d.size() != 1U || r.one(d[0], "defender") != r.one(e, "partner") || r.one(d[0], "attacker") != r.one(e, "enemy")) {
            out.push_back("6.5: ally_refused e" + s(e) + " is not a refused defence call");
        }
        if (f.size() + n.size() != 1U) out.push_back("6.5: ally_refused e" + s(e) + " cites no alliance");
        else if (!f.empty() && r.value(f[0], "basis") == 0) out.push_back("6.5: ally_refused e" + s(e) + " of an origin ally");
    }
    // 6.5, 6.7: breaks and lapses
    for (const std::int64_t e : r.of("alliance_broken")) {
        const std::int64_t reason = r.value(e, "reason");
        if (reason == 0) {
            const auto rf = cites(e, "ally_refused");
            if (rf.size() != 1U || std::set<std::int64_t>{r.one(rf[0], "ally"), r.one(rf[0], "partner")} !=
                                       pair_of(e, "ally_a", "ally_b")) {
                out.push_back("6.5: alliance_broken e" + s(e) + " by desertion cites no refusal of its pair");
            }
        } else if (cites(e, "alliance_formed").size() + cites(e, "alliance_renewed").size() != 1U) {
            out.push_back("6.7: alliance_broken e" + s(e) + " by cooling cites no formation or renewal");
        }
    }
    for (const std::int64_t e : r.of("alliance_lapsed")) {
        if (cites(e, "alliance_formed").size() + cites(e, "alliance_renewed").size() != 1U) {
            out.push_back("6.7: alliance_lapsed e" + s(e) + " cites no formation or renewal");
        }
    }
    // every closed alliance row is closed by one of the four kinds naming the pair or by a side's ending event
    for (const Row* row : r.rows("alliance")) {
        if (is_null(*row, 6)) continue;
        const std::int64_t u = at(*row, 8);
        const std::string k = r.kind(u);
        const bool four = k == "alliance_lapsed" || k == "alliance_broken" || k == "alliance_spurned" ||
                          k == "alliance_betrayed";
        const bool side = u == at(*x.civ.at(at(*row, 1)), 9) || u == at(*x.civ.at(at(*row, 3)), 9);
        if (!four && !side) out.push_back("9.1: alliance row since e" + s(at(*row, 7)) + " closed by e" + s(u));
    }
    // 6.8: trade breaks: by war the declaration of the pair; by cooling or reach its opening
    for (const std::int64_t e : r.of("trade_broken")) {
        const std::int64_t reason = r.value(e, "reason");
        if (reason == 0) {
            const auto d = cites(e, "war_declared");
            if (d.size() != 1U || pair_of(d[0], "attacker", "defender") != pair_of(e, "partner_a", "partner_b")) {
                out.push_back("6.8: trade_broken e" + s(e) + " by war cites no declaration of its pair");
            }
        } else if (cites(e, "trade_opened").size() != 1U) {
            out.push_back("6.8: trade_broken e" + s(e) + " cites no opening");
        }
    }
    // 6.13: an arms sale cites its trade's opening and its forge's mine_opened
    for (const std::int64_t e : r.of("arms_sold")) {
        const auto t = cites(e, "trade_opened"), m = cites(e, "mine_opened");
        if (t.size() != 1U || m.size() != 1U || pair_of(t[0], "partner_a", "partner_b") != pair_of(e, "seller", "buyer") ||
            r.one(m[0], "site") != r.one(e, "site")) {
            out.push_back("6.13: arms_sold e" + s(e) + " causes");
        }
    }
    // 6.12: a mine's cause is the larger of its region's gain and its town's founding (one cause)
    for (const std::int64_t e : r.of("mine_opened")) {
        if (x.cause_list(e).size() != 1U) out.push_back("6.12: mine_opened e" + s(e) + " has not one cause");
    }
    // 7.9, 7.10, 7.11: a joined war row closes with the war's end, the ally's withdrawal or a side's ending
    for (const Row* row : r.rows("war")) {
        if (r.kind(at(*row, 7)) != "ally_joined_war" || is_null(*row, 6)) continue;
        const std::int64_t u = at(*row, 8);
        const std::string k = r.kind(u);
        const bool ok = k == "ally_withdrew" || k == "peace" || k == "war_won" || k == "truce" ||
                        k == "civ_conquered" || k == "civ_ended" || k == "alliance_betrayed";
        if (!ok) out.push_back("7.10: joined war row of e" + s(at(*row, 7)) + " closed by " + k + " e" + s(u));
        if (k == "ally_withdrew" && (r.one(u, "ally") != at(*row, 1) || r.one(u, "enemy") != at(*row, 3))) {
            out.push_back("7.9: ally_withdrew e" + s(u) + " closes another ally's row");
        }
    }
    // 6.6: a betrayal's calls are made without the betrayer: no join of the betrayer in the declaration
    for (const std::int64_t b : r.of("alliance_betrayed")) {
        for (const std::int64_t d : cites(b, "war_declared")) {
            for (const std::int64_t j : r.kids(d)) {
                if (r.kind(j) == "ally_joined_war" && r.one(j, "ally") == r.one(b, "betrayed") &&
                    r.one(j, "enemy") == r.one(b, "betrayer") && r.one(j, "partner") == r.one(b, "betrayer")) {
                    out.push_back("6.6: e" + s(j) + " joins the betrayer's war");
                }
            }
        }
    }
    return out;
}

// Counts of the new kinds, for messages and the "every kind happens" checks.
inline std::map<std::string, std::int64_t> kind_counts(const World& w) {
    const Index x(w);
    std::map<std::string, std::int64_t> out;
    for (const char* k : {"alliance_formed", "alliance_renewed", "alliance_lapsed", "alliance_broken",
                          "alliance_betrayed", "ally_joined_war", "ally_refused", "ally_withdrew", "allied_battle",
                          "trade_opened", "trade_broken", "spoils_shared", "spoils_withheld", "alliance_spurned",
                          "mine_opened", "arms_sold"}) {
        out[k] = static_cast<std::int64_t>(x.events_of(k).size());
    }
    return out;
}

}  // namespace p504check
