#pragma once
// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1): the formulas of 6.2 to 6.9 (p508_f, hand-checkable), and the
// invariants of 13 checked from the rows of a world file (p508_check), ported from the independent reference
// (worldgen/tests/reference/p5_08_pantheon_reference.py: violations, stats_of_tables, faith_at, cults_in_at,
// cult_status_at, divine_effect_at). Written from the spec alone; the only library names are the generic row reader
// (WorldReader::table_rows) and the content set, so this header compiles on 0.29.0.
//
// What rows cannot show is left out and named at each check (a pestilence's losses, the force and growth factors, a
// stir): the reference checks those inside its replay.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p5_08_api.hpp"
#include "worldgen/db/reader.hpp"

namespace p508_f {

// 4.3: kin and foe. A creator (god_race != 0) is kin to its race; a pantheon god to the realms of its camp; a foe is
// of the opposed camp (good 0 against evil 2; neutral has none).
inline bool opposed(std::int64_t a, std::int64_t b) { return (a == 0 && b == 2) || (a == 2 && b == 0); }
inline bool kin(std::int64_t god_race, std::int64_t god_camp, std::int64_t civ_race, std::int64_t civ_camp) {
    return god_race != 0 ? civ_race == god_race : civ_camp == god_camp;
}
inline bool foe(std::int64_t god_camp, std::int64_t civ_camp) { return opposed(god_camp, civ_camp); }

// 6.2: F(t) = fade x t div 100; the fade over a step [Y, Y + L) is F(Y + L) - F(Y); R moves toward 0.
inline std::int64_t fade_F(std::int64_t per_century, std::int64_t t) { return per_century * t / 100; }
inline std::int64_t fade_amount(std::int64_t per_century, std::int64_t Y, std::int64_t L) {
    return fade_F(per_century, Y + L) - fade_F(per_century, Y);
}
inline std::int64_t faded(std::int64_t R, std::int64_t amount) {
    const std::int64_t d = std::min(R < 0 ? -R : R, amount);
    return R > 0 ? R - d : R + d;
}
struct Contribution {
    std::int64_t event = 0, year = 0, points = 0;
    friend bool operator==(const Contribution&, const Contribution&) = default;
};
// 6.2 add on one pair.
inline std::pair<std::int64_t, std::vector<Contribution>> add(std::int64_t R, std::vector<Contribution> con,
                                                              std::int64_t p, std::int64_t e, std::int64_t y,
                                                              std::int64_t regard_max, std::int64_t memory,
                                                              std::int64_t cmax) {
    R = std::clamp(R + p, -regard_max, regard_max);
    con.push_back({e, y, p});
    std::vector<Contribution> kept;
    for (const Contribution& c : con) {
        if (c.year >= y - memory) kept.push_back(c);
    }
    while (static_cast<std::int64_t>(kept.size()) > cmax) kept.erase(kept.begin());
    return {R, kept};
}
// 6.3: p = min(1000, act_permille x A div 1000).
inline std::int64_t act_permille(std::int64_t act, std::int64_t A) { return std::min<std::int64_t>(1000, act * A / 1000); }
// 6.3 the causes (reading P-R5 of the reference): the contributions of R's sign by (|points| descending, id
// descending), distinct events, the first three, listed ascending; the lead deed is the first of the ranking.
inline std::pair<std::int64_t, std::vector<std::int64_t>> causes(std::vector<Contribution> con, std::int64_t sign) {
    std::vector<Contribution> xs;
    for (const Contribution& c : con) {
        if (c.points * sign > 0) xs.push_back(c);
    }
    std::stable_sort(xs.begin(), xs.end(), [](const Contribution& a, const Contribution& b) {
        const std::int64_t pa = a.points < 0 ? -a.points : a.points;
        const std::int64_t pb = b.points < 0 ? -b.points : b.points;
        return pa != pb ? pa > pb : a.event > b.event;
    });
    std::vector<std::int64_t> order;
    for (const Contribution& c : xs) {
        if (std::find(order.begin(), order.end(), c.event) == order.end()) order.push_back(c.event);
    }
    if (order.empty()) return {0, {}};
    std::vector<std::int64_t> out(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(3U, order.size())));
    std::sort(out.begin(), out.end());
    return {order[0], out};
}
inline std::int64_t act_telling(std::int64_t c, std::int64_t acts) { return (c + acts) % 3; }
inline std::int64_t cult_telling(std::int64_t k) { return k % 3; }
// 6.4: a pestilence's toll per settlement.
inline std::int64_t pestilence_dead(std::int64_t P, std::int64_t toll) { return P * toll / 1000; }
struct Effect {
    std::string kind;   // plenty, withering, valour, defeat
    std::int64_t start = 0, years = 0;
};
// 6.4 "Effects in force" and 7.1's clamps.
inline std::int64_t growth(const std::vector<Effect>& es, std::int64_t y, std::int64_t plenty, std::int64_t withering,
                           std::int64_t sacred = 0) {
    std::int64_t g = sacred;
    for (const Effect& e : es) {
        if (e.start <= y && y < e.start + e.years) g += e.kind == "plenty" ? plenty : e.kind == "withering" ? -withering : 0;
    }
    return std::clamp<std::int64_t>(g, -900, 1000);
}
inline std::int64_t force(const std::vector<Effect>& es, std::int64_t y, std::int64_t valour, std::int64_t defeat) {
    std::int64_t f = 0;
    for (const Effect& e : es) {
        if (e.start <= y && y < e.start + e.years) f += e.kind == "valour" ? valour : e.kind == "defeat" ? -defeat : 0;
    }
    return std::clamp<std::int64_t>(f, -500, 500);
}
// 7.1: peoples' positive growth term (reading P-R2: after its floor of 1) and conflict's force.
inline std::int64_t growth_term(std::int64_t d, std::int64_t g) { return d > 0 ? d * (1000 + g) / 1000 : d; }
inline std::int64_t force_term(std::int64_t f, std::int64_t fp) { return f * (1000 + fp) / 1000; }
// 6.7 to 6.9.
inline std::int64_t found_permille(std::int64_t found, std::int64_t fear, bool favourable) {
    return found * (favourable ? 1000 : fear) / 1000;
}
inline std::int64_t spread_permille(std::int64_t spread, std::int64_t affinity, std::int64_t L = 0) {
    const std::int64_t p = spread * affinity / 1000;
    return L == 0 ? p : std::min<std::int64_t>(1000, p * L);
}
inline std::int64_t temper_permille(std::int64_t base, std::int64_t factor, std::int64_t L = 0) {
    const std::int64_t p = base * factor / 1000;
    return L == 0 ? p : std::min<std::int64_t>(1000, p * L);
}
// 12.2 the stir: min(10^6, deep_wake_ppm[d - 1] x M div 1000 x L x deep_stir_permille div 1000), a warren's also
// x deep_warren_wake_permille div 1000.
inline std::int64_t stir_ppm(const std::vector<std::int64_t>& ppm, std::int64_t d, std::int64_t M, std::int64_t L,
                             std::int64_t stir, std::int64_t warren = -1) {
    std::int64_t p = ppm[static_cast<std::size_t>(d - 1)] * M / 1000 * L * stir / 1000;
    if (warren >= 0) p = p * warren / 1000;
    return std::min<std::int64_t>(1000000, p);
}
// 12.3 the tellings of kinds 93 and 98.
inline std::int64_t telling_delve(std::int64_t site, std::int64_t depth) { return (site + 4 * depth) % 9; }
inline std::int64_t telling_warren(std::int64_t settlement, std::int64_t depth) { return (settlement + 4 * depth) % 9; }

}  // namespace p508_f

namespace p508_check {

namespace db = worldgen::db;
namespace wd = worldgen::data;
using Row = std::vector<db::ColumnValue>;

inline std::int64_t I(const Row& r, std::size_t i) { return r.at(i).integer; }
inline bool null_at(const Row& r, std::size_t i) { return r.at(i).type == db::ValueType::null; }
inline std::string S(const Row& r, std::size_t i) { return r.at(i).bytes; }
inline std::string n(std::int64_t v) { return std::to_string(v); }

struct Ev {
    std::int64_t id = 0, year = 0, kind = 0, phase = 0, region = 0, settlement = 0;
    std::vector<std::pair<std::int64_t, std::int64_t>> parts;   // (seq, entity id)
    std::map<std::int64_t, std::int64_t> vals;                  // seq -> value
    std::vector<std::int64_t> causes;                           // ascending
};

// The rows of a world file (every table by name) and their indexes.
struct World {
    std::map<std::string, std::vector<Row>> t;
    std::map<std::int64_t, std::string> key;
    std::map<std::string, std::int64_t> kid;
    std::map<std::int64_t, std::map<std::string, std::int64_t>> role_seq, value_seq;
    std::map<std::int64_t, Ev> ev;
    std::map<std::int64_t, const Row*> civ, stl, site, figure, faction, god, monster, phase;
    std::int64_t end_year = 0;

    [[nodiscard]] const std::vector<Row>& rows(const std::string& name) const {
        static const std::vector<Row> none;
        const auto it = t.find(name);
        return it == t.end() ? none : it->second;
    }
    [[nodiscard]] std::string kind_of(std::int64_t e) const {
        const auto it = ev.find(e);
        if (it == ev.end()) return "";
        const auto k = key.find(it->second.kind);
        return k == key.end() ? "" : k->second;
    }
    [[nodiscard]] std::vector<std::int64_t> role(std::int64_t e, const std::string& r) const {
        std::vector<std::int64_t> out;
        const auto it = ev.find(e);
        if (it == ev.end()) return out;
        const auto rs = role_seq.find(it->second.kind);
        if (rs == role_seq.end()) return out;
        const auto s = rs->second.find(r);
        if (s == rs->second.end()) return out;
        for (const auto& [seq, id] : it->second.parts) {
            if (seq == s->second) out.push_back(id);
        }
        return out;
    }
    [[nodiscard]] std::int64_t one(std::int64_t e, const std::string& r) const {
        const std::vector<std::int64_t> v = role(e, r);
        return v.empty() ? 0 : v[0];
    }
    [[nodiscard]] std::optional<std::int64_t> val(std::int64_t e, const std::string& k) const {
        const auto it = ev.find(e);
        if (it == ev.end()) return std::nullopt;
        const auto vs = value_seq.find(it->second.kind);
        if (vs == value_seq.end()) return std::nullopt;
        const auto s = vs->second.find(k);
        if (s == vs->second.end()) return std::nullopt;
        const auto v = it->second.vals.find(s->second);
        return v == it->second.vals.end() ? std::nullopt : std::optional<std::int64_t>(v->second);
    }
    [[nodiscard]] std::int64_t v(std::int64_t e, const std::string& k) const { return val(e, k).value_or(-999999); }
    [[nodiscard]] std::vector<std::int64_t> of(const std::string& k) const {
        std::vector<std::int64_t> out;
        const auto it = kid.find(k);
        if (it == kid.end()) return out;
        for (const auto& [id, e] : ev) {
            if (e.kind == it->second) out.push_back(id);
        }
        return out;
    }
    [[nodiscard]] std::int64_t year(std::int64_t e) const {
        const auto it = ev.find(e);
        return it == ev.end() ? 0 : it->second.year;
    }
    [[nodiscard]] bool quiet(std::int64_t e) const {
        const auto it = ev.find(e);
        if (it == ev.end()) return false;
        const auto p = phase.find(it->second.phase);
        return p != phase.end() && I(*p->second, 2) == 2;
    }
};

inline const std::vector<std::string>& tables() {
    static const std::vector<std::string> v = {"event_kind", "event_kind_role", "event_kind_value", "phase", "event",
                                               "event_participant", "event_cause", "event_value", "civilization",
                                               "settlement", "relation", "site", "figure", "faction", "god",
                                               "monster", "race", "region", "age", "region_border"};
    return v;
}

inline World world_of_file(const db::WorldReader& r) {
    World w;
    for (const std::string& name : tables()) {
        auto rows = r.table_rows(name);
        REQUIRE_MESSAGE(rows.ok(), "cannot read table " << name);
        w.t[name] = std::move(rows).value();
    }
    for (const Row& k : w.t["event_kind"]) {
        w.key[I(k, 0)] = S(k, 1);
        w.kid[S(k, 1)] = I(k, 0);
    }
    for (const Row& x : w.t["event_kind_role"]) w.role_seq[I(x, 0)][S(x, 2)] = I(x, 1);
    for (const Row& x : w.t["event_kind_value"]) w.value_seq[I(x, 0)][S(x, 2)] = I(x, 1);
    for (const Row& e : w.t["event"]) {
        Ev x;
        x.id = I(e, 0);
        x.year = I(e, 1);
        x.kind = I(e, 3);
        x.phase = I(e, 4);
        x.region = I(e, 5);
        x.settlement = I(e, 6);
        w.ev[x.id] = x;
    }
    for (const Row& p : w.t["event_participant"]) w.ev[I(p, 0)].parts.emplace_back(I(p, 2), I(p, 4));
    for (const Row& v : w.t["event_value"]) w.ev[I(v, 0)].vals[I(v, 1)] = I(v, 2);
    for (const Row& c : w.t["event_cause"]) w.ev[I(c, 0)].causes.push_back(I(c, 1));
    for (auto& [id, e] : w.ev) std::sort(e.causes.begin(), e.causes.end());
    for (const Row& x : w.t["civilization"]) w.civ[I(x, 0)] = &x;
    for (const Row& x : w.t["settlement"]) w.stl[I(x, 0)] = &x;
    for (const Row& x : w.t["site"]) w.site[I(x, 0)] = &x;
    for (const Row& x : w.t["figure"]) w.figure[I(x, 0)] = &x;
    for (const Row& x : w.t["faction"]) w.faction[I(x, 0)] = &x;
    for (const Row& x : w.t["god"]) w.god[I(x, 0)] = &x;
    for (const Row& x : w.t["monster"]) w.monster[I(x, 0)] = &x;
    for (const Row& x : w.t["phase"]) w.phase[I(x, 0)] = &x;
    w.end_year = std::stoll(r.meta("history_end_year").value_or("0"));
    return w;
}

// The data the checks read: history.pantheon's rests and memory, the god definitions' deeds (canonical id order)
// and the creator record's deeds.
struct Deed {
    std::string kind, role, who, against;
    std::int64_t points = 0;
};
struct Tuning {
    std::int64_t memory = 300, god_rest = 30, pair_rest = 100, tower_lapse = 0;
    std::vector<std::vector<Deed>> god_deeds;   // pantheon gods in canonical id order
    std::vector<Deed> creator_deeds;
};
inline std::vector<Deed> deeds_of(const wd::Value& v) {
    std::vector<Deed> out;
    for (const wd::Value& x : v.as_array()) {
        const wd::Record& r = x.as_record();
        out.push_back({r.get_string("kind"), r.get_string("role"), r.get_string("who"), r.get_string("against"),
                       r.get_int("points")});
    }
    return out;
}
inline Tuning tuning_of(const wd::ContentSet& c) {
    Tuning t;
    t.memory = p508api::tuning_or(c, "pantheon", "regard_memory_years", t.memory);
    t.god_rest = p508api::tuning_or(c, "pantheon", "god_rest_years", t.god_rest);
    t.pair_rest = p508api::tuning_or(c, "pantheon", "pair_rest_years", t.pair_rest);
    t.tower_lapse = p508api::tuning_or(c, "figures", "tower_lapse_years", 0);
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind == "god") {
            for (const wd::Definition& d : k.definitions) {
                const wd::Value* dv = d.record.find("deeds");
                t.god_deeds.push_back(dv == nullptr ? std::vector<Deed>{} : deeds_of(*dv));
            }
        }
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != "pantheon") continue;
            const wd::Value* cr = d.record.find("creator");
            if (cr == nullptr) continue;
            const wd::Value* dv = cr->as_record().find("deeds");
            if (dv != nullptr) t.creator_deeds = deeds_of(*dv);
        }
    }
    return t;
}

inline std::int64_t act_sign(const World& w, std::int64_t e) {
    const std::string k = w.kind_of(e);
    if (k == "divine_bless" || k == "divine_champion") return 1;
    if (k == "divine_curse" || k == "divine_monster") return -1;
    if (k == "divine_mark") return w.v(e, "profane") == 1 ? -1 : 1;
    return 0;
}
inline const std::vector<std::string>& act_keys() {
    static const std::vector<std::string> v = {"divine_bless", "divine_curse", "divine_champion", "divine_monster",
                                               "divine_mark"};
    return v;
}
inline std::vector<std::int64_t> acts_of(const World& w) {
    std::vector<std::int64_t> out;
    for (const std::string& k : act_keys()) {
        for (const std::int64_t e : w.of(k)) out.push_back(e);
    }
    std::sort(out.begin(), out.end());
    return out;
}
// A relation row in force at event e (opened at or before e, not closed before it).
inline bool in_force(const Row& q, std::int64_t e) { return I(q, 7) <= e && (I(q, 8) == 0 || I(q, 8) > e); }
inline std::vector<const Row*> rel_rows(const World& w, const std::string& kind) {
    std::vector<const Row*> out;
    for (const Row& q : w.rows("relation")) {
        if (S(q, 4) == kind) out.push_back(&q);
    }
    return out;
}
inline std::int64_t temple_cult(const World& w, const Row& s) { return w.one(I(s, 10), "faction"); }

// 13 items 1 to 9, 12 and 13 on rows (10 and 11 are the replay's; 14 the earlier checks').
inline std::vector<std::string> violations(const World& w, const Tuning& t) {
    std::vector<std::string> out;
    std::map<std::int64_t, std::int64_t> race_camp;
    for (const Row& r : w.rows("race")) race_camp[I(r, 0)] = I(r, 5);
    const std::int64_t nraces = static_cast<std::int64_t>(w.rows("race").size());
    std::map<std::int64_t, std::int64_t> god_camp, god_race;
    std::set<std::string> god_names, other_names;
    for (const Row& r : w.rows("region")) other_names.insert(S(r, 1));
    for (const Row& a : w.rows("age")) other_names.insert(S(a, 4));
    std::int64_t expect = 1;
    // 1: the god rows
    for (const auto& [id, g] : w.god) {
        if (id != expect++) out.push_back("1: god ids not dense at " + n(id));
        god_camp[id] = I(*g, 2);
        god_race[id] = I(*g, 5);
        if (id <= nraces && (I(*g, 5) != id || I(*g, 3) != 1 || I(*g, 2) != race_camp[id])) {
            out.push_back("1: creator god " + n(id));
        }
        if (id > nraces && (I(*g, 5) != 0 || I(*g, 3) < 2 || I(*g, 3) > 9)) out.push_back("1: pantheon god " + n(id));
        if (!god_names.insert(S(*g, 1)).second || other_names.count(S(*g, 1)) != 0U) {
            out.push_back("1: god name " + S(*g, 1) + " not distinct");
        }
    }
    auto civ_race = [&](std::int64_t c) { return w.civ.count(c) != 0U ? I(*w.civ.at(c), 2) : 0; };
    auto is_kin = [&](std::int64_t g, std::int64_t c) {
        return p508_f::kin(god_race[g], god_camp[g], civ_race(c), race_camp[civ_race(c)]);
    };
    auto is_foe = [&](std::int64_t g, std::int64_t c) { return p508_f::foe(god_camp[g], race_camp[civ_race(c)]); };
    auto deeds = [&](std::int64_t g) -> const std::vector<Deed>& {
        static const std::vector<Deed> none;
        if (g <= nraces) return t.creator_deeds;
        const std::size_t i = static_cast<std::size_t>(g - nraces - 1);
        return i < t.god_deeds.size() ? t.god_deeds[i] : none;
    };
    const std::vector<const Row*> faith = rel_rows(w, "faith_of");
    const std::vector<const Row*> cult_in = rel_rows(w, "cult_in");
    // 2, 3, 4: the acts
    std::map<std::int64_t, std::int64_t> per_year, last_g;
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> last_gc;
    std::map<std::int64_t, std::vector<std::int64_t>> shrines_at;
    for (const std::int64_t e : acts_of(w)) {
        const std::int64_t y = w.year(e), g = w.one(e, "actor"), c = w.one(e, "civilization");
        const std::int64_t sign = act_sign(w, e);
        const std::string k = w.kind_of(e);
        if (w.quiet(e)) out.push_back("2: act e" + n(e) + " in a quiet step");
        if (w.v(e, "divine_script") != 0) out.push_back("2: act e" + n(e) + " divine_script");
        const std::vector<std::int64_t>& cs = w.ev.at(e).causes;
        if (cs.empty() || cs.size() > 3U) out.push_back("2: act e" + n(e) + " has " + n(static_cast<std::int64_t>(cs.size())) + " causes");
        for (const std::int64_t cc : cs) {
            const std::string ck = w.kind_of(cc);
            bool ok = false;
            if ((ck == "cult_adopted" || ck == "cult_suppressed") && w.one(cc, "civilization") == c) {
                const std::int64_t fg = w.one(cc, "god");
                ok = (ck == "cult_suppressed" && fg == g && sign < 0) ||
                     (ck == "cult_adopted" && ((fg == g && sign > 0) || (fg != g && sign < 0)));
            }
            for (const Deed& d : deeds(g)) {
                if (d.kind != ck || d.points * sign <= 0) continue;
                const std::vector<std::int64_t> rs = w.role(cc, d.role);
                const bool who = d.who == "kin" ? is_kin(g, c) : d.who == "foe" ? is_foe(g, c) : true;
                if (std::find(rs.begin(), rs.end(), c) != rs.end() && who) ok = true;
            }
            if (!ok) out.push_back("2: act e" + n(e) + " cause e" + n(cc) + " (" + ck + ") is no deed of god " + n(g));
            if (y - w.year(cc) > t.memory) out.push_back("2: act e" + n(e) + " cause e" + n(cc) + " beyond memory");
        }
        if (++per_year[y] > 1) out.push_back("2: two acts in year " + n(y));
        if (last_g.count(g) != 0U && y - last_g[g] < t.god_rest) out.push_back("2: god " + n(g) + " rests too little at " + n(y));
        if (last_gc.count({g, c}) != 0U && y - last_gc[{g, c}] < t.pair_rest) {
            out.push_back("2: pair " + n(g) + "," + n(c) + " rests too little at " + n(y));
        }
        last_g[g] = y;
        last_gc[{g, c}] = y;
        if (sign > 0 && is_foe(g, c)) out.push_back("3: favourable act e" + n(e) + " on a foe");
        if (sign < 0) {
            for (const Row* q : faith) {
                if (I(*q, 3) == c && in_force(*q, e) && w.faction.count(I(*q, 1)) != 0U &&
                    I(*w.faction.at(I(*q, 1)), 4) == g) {
                    out.push_back("3: wrathful act e" + n(e) + " on a realm of its god's faith");
                }
            }
        }
        if (k == "divine_curse" && w.v(e, "dead") < 0) out.push_back("4: curse e" + n(e) + " dead below 0");
        if (k == "divine_monster") {
            const std::int64_t m = w.one(e, "monster"), lair = w.one(e, "lair");
            if (w.monster.count(m) == 0U || I(*w.monster.at(m), 5) != e) out.push_back("4: divine_monster e" + n(e) + " monster");
            if (w.site.count(lair) == 0U || I(*w.site.at(lair), 2) != p508api::SITE_LAIR || I(*w.site.at(lair), 10) != e) {
                out.push_back("4: divine_monster e" + n(e) + " lair");
            }
        }
        if (k == "divine_champion") {
            const std::int64_t f = w.one(e, "figure");
            if (w.figure.count(f) == 0U || I(*w.figure.at(f), 3) != p508api::ROLE_CHAMPION || I(*w.figure.at(f), 6) != c ||
                I(*w.figure.at(f), 9) != e) {
                out.push_back("4: divine_champion e" + n(e) + " figure");
            }
        }
        if (k == "divine_mark") {
            const std::int64_t s = w.one(e, "site");
            if (w.site.count(s) == 0U || I(*w.site.at(s), 2) != p508api::SITE_SHRINE || I(*w.site.at(s), 10) != e) {
                out.push_back("4: divine_mark e" + n(e) + " site");
            } else if (I(*w.site.at(s), 6) != 0) {
                shrines_at[I(*w.site.at(s), 6)].push_back(s);
            }
        }
    }
    for (const auto& [u, ss] : shrines_at) {
        if (ss.size() > 1U) out.push_back("4: settlement " + n(u) + " has two shrines");
    }
    // 5: cults
    std::map<std::int64_t, std::vector<std::pair<std::int64_t, std::optional<std::int64_t>>>> live;
    for (const auto& [k, f] : w.faction) {
        const std::int64_t ev = I(*f, 8);
        if (w.kind_of(ev) != "cult_founded" || w.one(ev, "faction") != k) {
            out.push_back("5: faction " + n(k) + " founding event");
            continue;
        }
        const std::vector<std::int64_t>& a = w.ev.at(ev).causes;
        const std::vector<std::string>& ak = act_keys();
        if (a.size() != 1U || std::find(ak.begin(), ak.end(), w.kind_of(a[0])) == ak.end() ||
            w.one(a[0], "actor") != I(*f, 4) || w.one(a[0], "civilization") != I(*f, 3) || w.year(ev) < w.year(a[0]) + 1) {
            out.push_back("5: faction " + n(k) + " does not cite an earlier act of its god on its realm");
        }
        const auto fig = w.figure.find(I(*f, 5));
        if (fig == w.figure.end()) {
            out.push_back("5: faction " + n(k) + " founder is no figure");
        } else {
            const Row& fr = *fig->second;
            const bool dead_before = !null_at(fr, 10) && I(fr, 10) < w.year(ev);
            if ((I(fr, 3) == p508api::ROLE_PROPHET && I(fr, 9) != ev) || I(fr, 7) > w.year(ev) || dead_before) {
                out.push_back("5: faction " + n(k) + " founder not alive at the founding");
            }
        }
        const auto seat = w.site.find(I(*f, 6));
        if (seat == w.site.end() || I(*seat->second, 2) != p508api::SITE_TEMPLE || I(*seat->second, 10) != ev ||
            I(*seat->second, 7) != I(*f, 3)) {
            out.push_back("5: faction " + n(k) + " seat");
        }
        const std::optional<std::int64_t> end =
            I(*f, 12) == p508api::STATUS_ENDED ? std::optional<std::int64_t>(I(*f, 9)) : std::nullopt;
        for (const auto& [y0, y1] : live[I(*f, 4)]) {
            if (!y1.has_value() || *y1 > w.year(ev)) out.push_back("5: god " + n(I(*f, 4)) + " has two live cults");
            (void)y0;
        }
        live[I(*f, 4)].emplace_back(w.year(ev), end);
    }
    // 6: cult_in and spread
    for (const Row* q : cult_in) {
        const std::string k = w.kind_of(I(*q, 7));
        if (k != "cult_founded" && k != "cult_spread") out.push_back("6: cult_in opened by " + k);
    }
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> steps;
    for (const std::int64_t e : w.of("cult_spread")) {
        const std::int64_t k = w.one(e, "faction"), a = w.one(e, "civilization"), from = w.one(e, "from");
        const std::vector<std::int64_t>& cs = w.ev.at(e).causes;
        if (w.faction.count(k) == 0U || std::find(cs.begin(), cs.end(), I(*w.faction.at(k), 8)) == cs.end()) {
            out.push_back("6: cult_spread e" + n(e) + " does not cite its founding");
        }
        bool had = false, linked = false;
        for (const Row* q : cult_in) had = had || (I(*q, 1) == k && I(*q, 3) == from && in_force(*q, e));
        for (const Row& q : w.rows("relation")) {
            const std::string rk = S(q, 4);
            if ((rk == "alliance" || rk == "trade") && in_force(q, e) &&
                ((I(q, 1) == a && I(q, 3) == from) || (I(q, 1) == from && I(q, 3) == a))) {
                linked = true;
            }
        }
        if (!had) out.push_back("6: cult_spread e" + n(e) + " from a realm without the cult");
        if (!linked) out.push_back("6: cult_spread e" + n(e) + " without alliance or trade");
        if (++steps[{k, w.ev.at(e).phase * 100000 + w.year(e)}] > 1) out.push_back("6: two spreads of a cult in a step");
    }
    // 7: faith
    std::map<std::int64_t, std::vector<const Row*>> per_civ;
    for (const Row* q : faith) {
        per_civ[I(*q, 3)].push_back(q);
        bool with_cult = false;
        for (const Row* c : cult_in) {
            with_cult = with_cult || (I(*c, 1) == I(*q, 1) && I(*c, 3) == I(*q, 3) && I(*c, 7) <= I(*q, 7) &&
                                      (I(*c, 8) == 0 || (I(*q, 8) != 0 && I(*c, 8) >= I(*q, 8))));
        }
        if (!with_cult) out.push_back("7: faith_of without its cult_in");
    }
    for (auto& [c, qs] : per_civ) {
        std::sort(qs.begin(), qs.end(), [](const Row* a, const Row* b) { return I(*a, 7) < I(*b, 7); });
        for (std::size_t i = 1; i < qs.size(); ++i) {
            if (I(*qs[i - 1], 8) == 0 || I(*qs[i - 1], 8) > I(*qs[i], 7)) out.push_back("7: c" + n(c) + " has two faiths");
        }
    }
    for (const std::int64_t e : w.of("cult_suppressed")) {
        const std::int64_t k = w.one(e, "faction"), c = w.one(e, "civilization"), g = w.one(e, "god");
        std::int64_t fgod = 0;
        for (const Row* q : faith) {
            if (I(*q, 3) != c || !in_force(*q, e)) continue;
            if (I(*q, 1) == k) out.push_back("7: cult_suppressed e" + n(e) + " of the realm's own faith");
            if (w.faction.count(I(*q, 1)) != 0U) fgod = I(*w.faction.at(I(*q, 1)), 4);
        }
        if (!(is_foe(g, c) || (fgod != 0 && p508_f::opposed(god_camp[fgod], god_camp[g])))) {
            out.push_back("7: cult_suppressed e" + n(e) + " by a realm not hostile");
        }
    }
    // 8, 9: endings and temples at E
    for (const auto& [k, f] : w.faction) {
        const std::int64_t st = I(*f, 12);
        if (st == p508api::STATUS_HIDDEN) {
            std::int64_t sanct = 0;
            for (const auto& [sid, s] : w.site) {
                if (I(*s, 2) == p508api::SITE_SANCTUM && w.kind_of(I(*s, 10)) == "cult_suppressed" &&
                    w.one(I(*s, 10), "faction") == k) {
                    ++sanct;
                }
            }
            bool open = false;
            for (const Row* q : cult_in) open = open || (I(*q, 1) == k && I(*q, 8) == 0);
            if (sanct != 1 || open) out.push_back("8: hidden faction " + n(k));
        }
        if (st == p508api::STATUS_ENDED) {
            if (I(*f, 11) != p508api::CULT_SUPPRESSED && I(*f, 11) != p508api::CULT_FELL) out.push_back("8: faction ending");
            for (const auto& [sid, s] : w.site) {
                if (I(*s, 2) == p508api::SITE_TEMPLE && temple_cult(w, *s) == k &&
                    (I(*s, 13) == 0 || I(*s, 11) > I(*f, 9))) {
                    out.push_back("8: temple " + n(sid) + " of ended faction " + n(k) + " open");
                }
            }
        }
        if ((st < p508api::STATUS_ENDED) != (I(*f, 11) == 0 && null_at(*f, 9))) out.push_back("9: faction " + n(k) + " status");
    }
    for (const auto& [sid, s] : w.site) {
        if (I(*s, 2) != p508api::SITE_TEMPLE || I(*s, 13) != 0) continue;
        const auto st = w.stl.find(I(*s, 6));
        if (st == w.stl.end() || (I(*st->second, 10) != 1 && I(*st->second, 10) != 2)) {
            out.push_back("9: open temple " + n(sid) + " on a settlement not standing");
            continue;
        }
        const std::int64_t o = I(*st->second, 8), k = temple_cult(w, *s);
        bool has = o == I(*s, 7);
        for (const Row* q : cult_in) has = has || (I(*q, 1) == k && I(*q, 3) == o && I(*q, 8) == 0);
        if (!has) out.push_back("9: open temple " + n(sid) + " without its cult");
    }
    // 12 (12.4): aid after the outcome
    for (const std::int64_t e : w.of("ally_aided_hold")) {
        const std::vector<std::int64_t>& cs = w.ev.at(e).causes;
        const std::string k1 = cs.size() == 2U ? w.kind_of(cs[1]) : "";
        if (cs.size() != 2U || w.kind_of(cs[0]) != "deep_woken" ||
            (k1 != "deep_sealed" && k1 != "warren_sealed" && k1 != "hold_lost")) {
            out.push_back("12: ally_aided_hold e" + n(e) + " causes");
        }
    }
    for (const std::int64_t e : w.of("deep_woken")) {
        if (w.quiet(e)) out.push_back("11: waking e" + n(e) + " in a quiet step");
        const std::int64_t m = w.one(e, "monster");
        for (const auto& [y, x] : w.ev) {
            const std::string k = w.kind_of(y);
            if (y <= e || (k != "deep_sealed" && k != "warren_sealed" && k != "hold_lost") || w.one(y, "monster") != m) continue;
            if (x.year == w.year(e) && x.causes != std::vector<std::int64_t>{e}) out.push_back("12: outcome of e" + n(e));
            break;
        }
    }
    // 13 (12.6): towers
    for (const std::int64_t e : w.of("tower_abandoned")) {
        const std::int64_t s = w.one(e, "site"), f = w.one(e, "figure");
        if (w.site.count(s) == 0U || I(*w.site.at(s), 2) != p508api::SITE_TOWER ||
            I(*w.site.at(s), 13) != p508api::ENDING_ABANDONED || I(*w.site.at(s), 12) != e) {
            out.push_back("13: tower_abandoned e" + n(e) + " site");
        }
        if (w.figure.count(f) != 0U && !null_at(*w.figure.at(f), 10) && t.tower_lapse > 0 &&
            w.year(e) < I(*w.figure.at(f), 10) + t.tower_lapse) {
            out.push_back("13: tower_abandoned e" + n(e) + " before the lapse");
        }
    }
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<const Row*>> towers;
    for (const auto& [sid, s] : w.site) {
        if (I(*s, 2) == p508api::SITE_TOWER && I(*s, 6) == 0) towers[{I(*s, 7), I(*s, 3)}].push_back(s);
    }
    for (const auto& [key, ss] : towers) {
        for (const Row* a : ss) {
            for (const Row* b : ss) {
                if (I(*a, 0) < I(*b, 0) && (I(*a, 13) == 0 || I(*a, 12) > I(*b, 10))) {
                    out.push_back("13: c" + n(key.first) + " has two open towers in region " + n(key.second));
                }
            }
        }
    }
    return out;
}

// 10 item 8's statistics from rows (renewals_capped and deep_stirred are not in rows: 0 here, reading F3 of the notes).
inline std::vector<std::pair<std::string, std::int64_t>> stats(const World& w) {
    std::vector<std::pair<std::string, std::int64_t>> st;
    const std::vector<std::int64_t> acts = acts_of(w);
    st.emplace_back("divine_acts", static_cast<std::int64_t>(acts.size()));
    for (const std::string& k : act_keys()) st.emplace_back(k, static_cast<std::int64_t>(w.of(k).size()));
    std::int64_t camps[3] = {0, 0, 0}, creators = 0, dead = 0;
    const std::int64_t nraces = static_cast<std::int64_t>(w.rows("race").size());
    for (const std::int64_t e : acts) {
        const std::int64_t g = w.one(e, "actor");
        if (w.god.count(g) != 0U) camps[static_cast<std::size_t>(I(*w.god.at(g), 2))] += 1;
        if (g <= nraces) ++creators;
    }
    for (const std::int64_t e : w.of("divine_curse")) dead += w.v(e, "dead");
    std::int64_t flares = 0;
    for (const auto& [pid, p] : w.phase) {
        if (I(*p, 2) != 3 || I(*p, 5) == 0) continue;
        for (const std::int64_t e : acts) {
            const std::vector<std::int64_t>& cs = w.ev.at(e).causes;
            if (w.year(e) == I(*p, 3) && std::find(cs.begin(), cs.end(), I(*p, 5)) != cs.end()) {
                ++flares;
                break;
            }
        }
    }
    st.emplace_back("divine_good", camps[0]);
    st.emplace_back("divine_neutral", camps[1]);
    st.emplace_back("divine_evil", camps[2]);
    st.emplace_back("divine_creators", creators);
    st.emplace_back("divine_flares", flares);
    st.emplace_back("divine_dead", dead);
    std::int64_t hidden = 0, present = 0, prophets = 0, sacred = 0, profane = 0, temples = 0;
    for (const auto& [k, f] : w.faction) {
        hidden += I(*f, 12) == p508api::STATUS_HIDDEN ? 1 : 0;
        present += I(*f, 12) == p508api::STATUS_PRESENT ? 1 : 0;
        const auto fig = w.figure.find(I(*f, 5));
        prophets += fig != w.figure.end() && I(*fig->second, 3) == p508api::ROLE_PROPHET ? 1 : 0;
    }
    for (const auto& [sid, s] : w.site) {
        if (I(*s, 2) == p508api::SITE_SHRINE) (w.v(I(*s, 10), "profane") == 1 ? profane : sacred) += 1;
        temples += I(*s, 2) == p508api::SITE_TEMPLE ? 1 : 0;
    }
    st.emplace_back("cults_founded", static_cast<std::int64_t>(w.faction.size()));
    st.emplace_back("cults_spread", static_cast<std::int64_t>(w.of("cult_spread").size()));
    st.emplace_back("cults_adopted", static_cast<std::int64_t>(w.of("cult_adopted").size()));
    st.emplace_back("cults_suppressed", static_cast<std::int64_t>(w.of("cult_suppressed").size()));
    st.emplace_back("cults_hidden", hidden);
    st.emplace_back("cults_present", present);
    st.emplace_back("cult_founders_prophets", prophets);
    st.emplace_back("shrines_sacred", sacred);
    st.emplace_back("shrines_profane", profane);
    st.emplace_back("temples", temples);
    st.emplace_back("towers_taken_up", static_cast<std::int64_t>(w.of("tower_taken_up").size()));
    st.emplace_back("towers_abandoned", static_cast<std::int64_t>(w.of("tower_abandoned").size()));
    return st;
}

// The paths of 6 a world's rows show.
inline std::set<std::string> paths_of(const World& w) {
    std::set<std::string> out;
    const char* words[5] = {"bless", "curse", "champion", "monster", "mark"};
    for (std::size_t i = 0; i < 5U; ++i) {
        if (!w.of(act_keys()[i]).empty()) out.insert(words[i]);
    }
    for (const std::int64_t e : w.of("divine_mark")) out.insert(w.v(e, "profane") == 1 ? "profane mark" : "sacred mark");
    if (!w.faction.empty()) out.insert("founded");
    for (const auto& [k, f] : w.faction) {
        const auto fig = w.figure.find(I(*f, 5));
        if (fig != w.figure.end() && I(*fig->second, 3) == p508api::ROLE_PROPHET) out.insert("prophet founder");
        if (I(*f, 12) == p508api::STATUS_HIDDEN) out.insert("hidden");
        if (I(*f, 12) == p508api::STATUS_ENDED) out.insert(I(*f, 11) == p508api::CULT_SUPPRESSED ? "ended suppressed" : "ended fell");
    }
    for (const char* k : {"cult_spread", "cult_adopted", "cult_suppressed", "tower_taken_up", "tower_abandoned"}) {
        if (!w.of(k).empty()) out.insert(k);
    }
    return out;
}

// 11.1 from rows, as the reference derives them (the timeline must agree).
inline std::int64_t faith_at(const World& w, std::int64_t civ, std::int64_t year) {
    for (const Row* q : rel_rows(w, "faith_of")) {
        if (I(*q, 3) == civ && I(*q, 5) <= year && (null_at(*q, 6) || I(*q, 6) > year)) return I(*q, 1);
    }
    return 0;
}
inline std::int64_t divine_effect_at(const World& w, std::int64_t civ, std::int64_t year) {
    std::int64_t bits = 0;
    for (const char* k : {"divine_bless", "divine_curse"}) {
        for (const std::int64_t e : w.of(k)) {
            if (w.one(e, "civilization") != civ) continue;
            const std::int64_t y0 = w.year(e), years = w.v(e, "years"), eff = w.v(e, "effect");
            if (!(y0 <= year && year < y0 + years)) continue;
            if (std::string(k) == "divine_bless") {
                bits |= eff == 1 ? 1 : 4;
            } else {
                bits |= eff == 2 ? 2 : eff == 3 ? 8 : 0;
            }
        }
    }
    return bits;
}

}  // namespace p508_check
