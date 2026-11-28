#pragma once
// Spec P5-11 (docs/specs/p5-11-artifacts.md r1): the formulas of 6.3 to 6.5, 7.1, 7.2 and 7.6 (p511_f,
// hand-checkable), and the invariants of 13 checked from the rows of a world file (p511_check), ported from the
// independent reference (worldgen/tests/reference/p5_11_artifacts_reference.py: violations, stats_of_tables,
// paths_of, artifact_at, artifacts_held, artifacts_placed). Written from the spec alone; the only library names are the
// generic row reader (WorldReader::table_rows, through p508_check::World) and the content set, so this header compiles
// on 0.30.0.
//
// What rows cannot show is left out and named at each check (7's win chances and hunt strengths, the strain factor,
// cult_returns_barred): the reference checks those inside its replay.
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p5_08_checks.hpp"
#include "p5_11_api.hpp"

namespace p511_f {

// 7.6 the form kind: "a" or "an" (an before a noun starting with a, e, i, o or u), a space and the noun, then
// " of deep iron" when deep is 1.
inline std::string kind_form(const std::string& noun, std::int64_t deep) {
    const char c = noun.empty() ? ' ' : noun[0];
    const bool vowel = c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
    return std::string(vowel ? "an " : "a ") + noun + (deep == 1 ? " of deep iron" : "");
}
// 6.3: p = min(1000, chance x (F != 0 ? forge_chance : 1000) div 1000).
inline std::int64_t make_permille(std::int64_t chance, std::int64_t forge, std::int64_t forge_chance) {
    return std::min<std::int64_t>(1000, chance * (forge != 0 ? forge_chance : 1000) / 1000);
}
// 6.3: W in the order arms, armour, regalia, relic, staff; relic 0 without a god participant, staff 0 unless the
// realm's caster masters an open tower.
inline std::vector<std::int64_t> class_weights(std::vector<std::int64_t> w, bool has_god, bool caster_with_tower) {
    if (!has_god) w.at(3) = 0;
    if (!caster_with_tower) w.at(4) = 0;
    return w;
}
// 6.4 forge(c): c when depth(c) >= 1; else the partner (ascending) with the largest depth >= 1, then the smaller id.
inline std::int64_t forge_of(std::int64_t c, const std::map<std::int64_t, std::int64_t>& depth,
                             std::vector<std::int64_t> partners) {
    const auto d = [&](std::int64_t x) {
        const auto it = depth.find(x);
        return it == depth.end() ? std::int64_t{0} : it->second;
    };
    if (d(c) >= 1) return c;
    std::sort(partners.begin(), partners.end());
    std::int64_t best = 0;
    for (const std::int64_t p : partners) {
        if (d(p) >= 1 && (best == 0 || d(p) > d(best))) best = p;
    }
    return best;
}
// 6.3 eligibility (pure; no draw when it fails).
inline bool eligible(bool alive, bool visible, std::int64_t Y, std::int64_t last_made, std::int64_t rest,
                     std::int64_t made, std::int64_t world_max, bool quiet, std::int64_t quiet_left) {
    return alive && visible && Y - last_made >= rest && made < world_max && (!quiet || quiet_left > 0);
}
struct Borne {
    std::int64_t battle = 0, hunt = 0;
    bool deep = false;
};
// 7.1: min(cap, sum of (field + deep bonus when deep and field > 0)).
inline std::int64_t battle_sum(const std::vector<Borne>& xs, std::int64_t deep_bonus, std::int64_t cap) {
    std::int64_t s = 0;
    for (const Borne& b : xs) s += b.battle + (b.deep && b.battle > 0 ? deep_bonus : 0);
    return std::min(cap, s);
}
inline std::int64_t hunt_sum(const std::vector<Borne>& xs, std::int64_t deep_bonus, std::int64_t cap) {
    std::int64_t s = 0;
    for (const Borne& b : xs) s += b.hunt + (b.deep && b.hunt > 0 ? deep_bonus : 0);
    return std::min(cap, s);
}
// 7.2: the win chance with the generals' skill and borne arms.
inline std::int64_t win_chance(std::int64_t A, std::int64_t D, std::int64_t dv, std::int64_t skill_x,
                               std::int64_t skill_y, std::int64_t arms_x, std::int64_t arms_y) {
    const std::int64_t a1 = A * (1000 + skill_x + arms_x) / 1000;
    const std::int64_t d1 = D * dv / 1000 * (1000 + skill_y + arms_y) / 1000;
    return a1 + d1 > 0 ? a1 * 1000 / (a1 + d1) : 500;
}
// 7.2: the strain rate times (1000 + claim) div 1000.
inline std::int64_t strain_rate(std::int64_t rate, std::int64_t claim) { return rate * (1000 + claim) / 1000; }
// 7.2: (p_hunt, S_c) of P5-06 7.7 with the borne hunt and the hoard's motive.
inline std::pair<std::int64_t, std::int64_t> hunt_values(std::int64_t hunt, std::int64_t hunt_max, std::int64_t raid,
                                                         std::int64_t R, std::int64_t H, std::int64_t prowess,
                                                         std::int64_t hero_hunt, std::int64_t hero_might,
                                                         std::int64_t borne_hunt, std::int64_t motive) {
    const std::int64_t p = std::min(hunt_max + hero_hunt + motive,
                                    hunt + (std::max<std::int64_t>(1, R) - 1) * raid + hero_hunt + motive);
    const std::int64_t S = H * prowess / 1000 * (1000 + hero_might + borne_hunt) / 1000;
    return {p, S};
}

}  // namespace p511_f

namespace p511_check {

namespace db = worldgen::db;
namespace wd = worldgen::data;
using Row = p508_check::Row;
using p508_check::I;
using p508_check::null_at;
using p508_check::S;
using p508_check::n;

inline constexpr std::int64_t NONE = -999999;

// The rows of a world file with the schema 15 tables, and the artifact indexes.
struct World {
    p508_check::World base;
    std::map<std::int64_t, const Row*> kind, art;
    std::map<std::int64_t, std::vector<const Row*>> hold;   // artifact -> holdings by seq
    std::map<std::int64_t, std::int64_t> region_ok;          // region ids

    [[nodiscard]] std::string key(std::int64_t e) const { return base.kind_of(e); }
    [[nodiscard]] std::int64_t one(std::int64_t e, const std::string& r) const { return base.one(e, r); }
    [[nodiscard]] std::vector<std::int64_t> role(std::int64_t e, const std::string& r) const { return base.role(e, r); }
    [[nodiscard]] std::int64_t val(std::int64_t e, const std::string& k) const { return base.v(e, k); }
    [[nodiscard]] std::vector<std::int64_t> of(const std::string& k) const { return base.of(k); }
    [[nodiscard]] std::int64_t year(std::int64_t e) const { return base.year(e); }
    [[nodiscard]] std::vector<std::int64_t> causes(std::int64_t e) const {
        const auto it = base.ev.find(e);
        return it == base.ev.end() ? std::vector<std::int64_t>{} : it->second.causes;
    }
    [[nodiscard]] std::int64_t cls(std::int64_t a) const { return I(*kind.at(I(*art.at(a), 2)), 4); }
    [[nodiscard]] const Row& open(std::int64_t a) const { return *hold.at(a).back(); }
    [[nodiscard]] std::int64_t fig_civ(std::int64_t f) const {
        const auto it = base.figure.find(f);
        return it == base.figure.end() ? 0 : I(*it->second, 6);
    }
    // The holding of a in force just before event x is applied (opened before x, not closed before x).
    [[nodiscard]] const Row* holding_at_event(std::int64_t a, std::int64_t x) const {
        const auto it = hold.find(a);
        if (it == hold.end()) return nullptr;
        for (const Row* h : it->second) {
            if (I(*h, 12) < x && (I(*h, 13) == 0 || I(*h, 13) >= x)) return h;
        }
        return nullptr;
    }
};

inline World world_of_file(const db::WorldReader& r) {
    World w;
    w.base = p508_check::world_of_file(r);
    for (const char* t : {"artifact_kind", "artifact", "artifact_holding"}) {
        auto rows = r.table_rows(t);
        REQUIRE_MESSAGE(rows.ok(), "cannot read table " << t);
        w.base.t[t] = std::move(rows).value();
    }
    for (const Row& x : w.base.t["artifact_kind"]) w.kind[I(x, 0)] = &x;
    for (const Row& x : w.base.t["artifact"]) w.art[I(x, 0)] = &x;
    for (const Row& x : w.base.t["artifact_holding"]) w.hold[I(x, 0)].push_back(&x);
    for (auto& [a, hs] : w.hold) {
        std::sort(hs.begin(), hs.end(), [](const Row* p, const Row* q) { return I(*p, 1) < I(*q, 1); });
    }
    for (const Row& x : w.base.rows("region")) w.region_ok[I(x, 0)] = 1;
    return w;
}

// The data the checks read: history.artifacts' occasions (kind, role) and limits, history.pantheon.cult_return_years.
struct Tuning {
    std::set<std::pair<std::string, std::string>> occasions;
    std::int64_t rest = 50, world_max = 40, deep_depth = 3, claim_max = 1000, cult_return = 0;
};
inline Tuning tuning_of(const wd::ContentSet& c) {
    Tuning t;
    t.rest = p511api::tuning_or(c, "artifacts", "realm_rest_years", t.rest);
    t.world_max = p511api::tuning_or(c, "artifacts", "world_max", t.world_max);
    t.deep_depth = p511api::tuning_or(c, "artifacts", "deep_metal_depth", t.deep_depth);
    t.claim_max = p511api::tuning_or(c, "artifacts", "claim_max_permille", t.claim_max);
    t.cult_return = p511api::tuning_or(c, "pantheon", "cult_return_years", 0);
    if (const wd::Definition* d = c.find("history", "artifacts"); d != nullptr) {
        if (const wd::Value* o = d->record.find("occasions"); o != nullptr) {
            for (const wd::Value& x : o->as_array()) {
                t.occasions.insert({x.as_record().get_string("kind"), x.as_record().get_string("role")});
            }
        }
    }
    return t;
}

inline const std::vector<std::string>& artifact_keys() {
    static const std::vector<std::string> v = {"artifact_made", "artifact_taken", "artifact_lost", "artifact_hoarded",
                                               "artifact_recovered", "artifact_buried"};
    return v;
}
inline bool is_artifact_key(const std::string& k) {
    const auto& v = artifact_keys();
    return std::find(v.begin(), v.end(), k) != v.end();
}
inline bool is_one_of(const std::string& k, std::initializer_list<const char*> xs) {
    for (const char* x : xs) {
        if (k == x) return true;
    }
    return false;
}
// 6.7: the rows M1 to M14 and the quiet-move rows M1, M2, M6, M14.
inline bool is_move_key(const std::string& k) {
    return is_one_of(k, {"general_appointed", "figure_died", "fell_in_battle", "hero_slain", "monster_slain_by_hero",
                         "monster_slain", "monster_died", "monster_raid", "hold_lost", "sacked", "settlement_captured",
                         "civ_conquered", "civ_ended", "peace", "war_won", "cult_suppressed"});
}
inline bool is_quiet_move_key(const std::string& k) {
    return is_one_of(k, {"general_appointed", "figure_died", "monster_died", "cult_suppressed"});
}
// 13 item 5: the (before, after) states each trigger may move between.
inline bool move_states_ok(const std::string& k, std::int64_t a, std::int64_t b) {
    using namespace p511api;
    const auto is = [&](std::int64_t x, std::int64_t y) { return a == x && b == y; };
    if (k == "general_appointed") return is(KEPT, BORNE);
    if (k == "figure_died") return is(BORNE, ENSHRINED) || is(BORNE, BURIED) || is(BORNE, KEPT);
    if (k == "fell_in_battle") return is(BORNE, KEPT) || is(BORNE, LOST);
    if (k == "hero_slain") return is(BORNE, HOARDED);
    if (k == "monster_slain_by_hero") return is(HOARDED, BORNE) || is(HOARDED, KEPT);
    if (k == "monster_slain") return is(HOARDED, KEPT);
    if (k == "monster_died") return is(HOARDED, LOST);
    if (k == "monster_raid" || k == "hold_lost") return is(KEPT, HOARDED);
    if (k == "sacked" || k == "civ_conquered" || k == "civ_ended") return is(KEPT, KEPT) || is(KEPT, LOST);
    if (k == "settlement_captured") return is(KEPT, KEPT);
    if (k == "peace" || k == "war_won") return is(KEPT, KEPT) || is(BORNE, KEPT);
    if (k == "cult_suppressed") return is(ENSHRINED, ENSHRINED) || is(ENSHRINED, KEPT);
    return false;
}

// P5-07 9: the depth of mine sid before event x (the largest depth of its delving_deepened, 0 none).
inline std::int64_t mine_depth_at(const World& w, std::int64_t sid, std::int64_t x) {
    std::int64_t d = 0;
    for (const std::int64_t e : w.of("delving_deepened")) {
        if (e < x && w.one(e, "site") == sid) d = std::max(d, w.val(e, "depth"));
    }
    return d;
}
// The forges of c at event x (reference reading A-R19): mines of founder c opened before x with depth >= 1, open
// after x (strict) or ended in x's year or later (lenient: the alliances' closing step may follow the making).
inline std::vector<std::int64_t> forges_at(const World& w, std::int64_t c, std::int64_t x, bool lenient) {
    std::vector<std::int64_t> out;
    const std::int64_t y = w.year(x);
    for (const auto& [sid, s] : w.base.site) {
        if (I(*s, 2) != p511api::SITE_MINE || I(*s, 7) != c || I(*s, 10) >= x || mine_depth_at(w, sid, x) < 1) continue;
        const bool open = I(*s, 12) == 0 || I(*s, 12) > x || (lenient && !null_at(*s, 11) && I(*s, 11) >= y);
        if (open) out.push_back(sid);
    }
    return out;
}
inline std::vector<std::int64_t> partners_at(const World& w, std::int64_t c, std::int64_t x) {
    std::set<std::int64_t> out;
    for (const Row& q : w.base.rows("relation")) {
        const std::string k = S(q, 4);
        if ((k != "alliance" && k != "trade") || I(q, 0) != 3 || I(q, 2) != 3) continue;
        if (I(q, 1) != c && I(q, 3) != c) continue;
        if (I(q, 7) < x && (I(q, 8) == 0 || I(q, 8) > x)) out.insert(I(q, 1) == c ? I(q, 3) : I(q, 1));
    }
    return {out.begin(), out.end()};
}
inline std::int64_t max_depth(const World& w, const std::vector<std::int64_t>& mines, std::int64_t x) {
    std::int64_t d = 0;
    for (const std::int64_t s : mines) d = std::max(d, mine_depth_at(w, s, x));
    return d;
}

// 13 items 1 to 6, 7 (the declaration's cause) and 8 on rows, and the holding checks of 9; the win chances of item 7
// are the reference's replay; item 9 is the earlier checks'.
inline std::vector<std::string> violations(const World& w, const Tuning& t) {
    using namespace p511api;
    std::vector<std::string> out;
    const auto& civ = w.base.civ;
    const auto& fig = w.base.figure;
    const auto& mon = w.base.monster;
    if (static_cast<std::int64_t>(w.art.size()) > t.world_max) out.push_back("2: above world_max");
    std::map<std::int64_t, std::int64_t> per_occasion;
    std::map<std::int64_t, std::vector<std::int64_t>> made_years;
    std::int64_t expect_id = 1;
    for (const auto& [aid, ap] : w.art) {
        const Row& a = *ap;
        const std::string pre = "artifact " + n(aid) + ": ";
        if (aid != expect_id++) out.push_back(pre + "9: ids not dense");
        if (w.kind.count(I(a, 2)) == 0U) {
            out.push_back(pre + "9: kind not a row of artifact_kind");
            continue;
        }
        const std::int64_t me = I(a, 10), oc = I(a, 11), c = I(a, 3), mk = I(a, 4);
        const auto hit = w.hold.find(aid);
        if (hit == w.hold.end()) {
            out.push_back(pre + "9: no holding");
            continue;
        }
        const auto& hs = hit->second;
        // 9: the chain
        for (std::size_t k = 0; k < hs.size(); ++k) {
            const Row& h = *hs[k];
            const std::string hp = pre + "holding " + n(I(h, 1)) + ": ";
            if (I(h, 1) != static_cast<std::int64_t>(k) + 1) out.push_back(hp + "9: seq not dense");
            if (k == 0U && (I(h, 2) != I(a, 9) || I(h, 12) != me)) out.push_back(hp + "9: the first is not the making");
            if (k + 1U < hs.size()) {
                const Row& nx = *hs[k + 1U];
                if (null_at(h, 3) || I(h, 3) != I(nx, 2) || I(h, 13) != I(nx, 12)) {
                    out.push_back(hp + "9: does not close where the next opens");
                }
            } else if (!null_at(h, 3) || I(h, 13) != 0) {
                out.push_back(hp + "9: the last holding is closed");
            }
            if (!null_at(h, 3) && I(h, 2) > I(h, 3)) out.push_back(hp + "9: from_year above to_year");
            if (w.year(I(h, 12)) != I(h, 2)) out.push_back(hp + "9: event not of year from_year");
            const std::int64_t st = I(h, 4), ht = I(h, 5);
            const bool type_ok = (st == KEPT && ht == 3) || (st == BORNE && ht == 6) || (st == HOARDED && ht == 8) ||
                                 (st == LOST && (ht == 4 || ht == 5 || ht == 9)) || (st == BURIED && ht == 6) ||
                                 (st == ENSHRINED && ht == 9);
            if (!type_ok) out.push_back(hp + "9: state " + n(st) + " with holder type " + n(ht));
            const bool place0 = I(h, 7) == 0 && I(h, 8) == 0 && I(h, 9) == 0 && I(h, 10) == 0 && I(h, 11) == 0;
            if (st <= HOARDED && !place0) out.push_back(hp + "9: place columns not 0");
            if (st >= LOST) {
                if (w.region_ok.count(I(h, 7)) == 0U) out.push_back(hp + "9: region " + n(I(h, 7)));
                if (ht == 9 && I(h, 9) != I(h, 6)) out.push_back(hp + "9: site is not its holder");
                if (ht == 4 && I(h, 8) != I(h, 6)) out.push_back(hp + "9: settlement is not its holder");
            }
            // 1: a kept, borne or hoarded holding closes in the year of its holder's end
            std::optional<std::int64_t> end;
            const std::int64_t hd = I(h, 6);
            if (st == KEPT && civ.count(hd) != 0U && !null_at(*civ.at(hd), 8)) end = I(*civ.at(hd), 8);
            if (st == BORNE && fig.count(hd) != 0U && !null_at(*fig.at(hd), 10)) end = I(*fig.at(hd), 10);
            if (st == HOARDED && mon.count(hd) != 0U && !null_at(*mon.at(hd), 6)) end = I(*mon.at(hd), 6);
            if (end.has_value() && I(h, 2) <= *end && (null_at(h, 3) || I(h, 3) > *end)) {
                out.push_back(hp + "1: open past its holder's end " + n(*end));
            }
            // 4: only arms, armour and staffs are borne
            const std::int64_t cl = w.cls(aid);
            if (st == BORNE && cl != ARMS && cl != ARMOUR && cl != STAFF) out.push_back(hp + "4: a regalia or relic borne");
            // 5, 6: the opening event and the move
            if (k == 0U) continue;
            const Row& prev = *hs[k - 1U];
            const std::int64_t x = I(h, 12);
            const std::string xk = w.key(x);
            std::string trig_k = xk;
            std::int64_t trig = x;
            if (is_artifact_key(xk) && xk != "artifact_made") {
                if (w.one(x, "artifact") != aid) out.push_back(hp + "5: opened by an event of another artifact");
                const auto cs = w.causes(x);
                trig = cs.empty() ? 0 : cs[0];
                trig_k = w.key(trig);
            } else if (!is_quiet_move_key(xk) && !(w.base.quiet(x) && is_move_key(xk))) {
                out.push_back(hp + "5: opened by " + xk + ", no quiet move");
            }
            if (!move_states_ok(trig_k, I(prev, 4), st)) {
                out.push_back(hp + "5: " + trig_k + " from " + n(I(prev, 4)) + " to " + n(st));
            }
            if (is_one_of(trig_k, {"hero_slain", "fell_in_battle", "figure_died"}) && I(prev, 6) != w.one(trig, "figure")) {
                out.push_back(hp + "5: " + trig_k + " of another bearer");
            }
            if (is_one_of(trig_k, {"monster_slain_by_hero", "monster_slain", "monster_died"}) &&
                I(prev, 6) != w.one(trig, "monster")) {
                out.push_back(hp + "5: " + trig_k + " of another keeper");
            }
            if (I(prev, 4) == LOST || I(prev, 4) == BURIED) out.push_back(hp + "6: left state " + n(I(prev, 4)));
            if (I(prev, 4) == ENSHRINED && trig_k != "cult_suppressed") out.push_back(hp + "6: left a shrine");
        }
        std::int64_t open_n = 0;
        for (const Row* h : hs) open_n += null_at(*h, 3) ? 1 : 0;
        if (open_n != 1) out.push_back(pre + "1: open holdings " + n(open_n));
        if (I(a, 13) != I(*hs.back(), 4)) out.push_back(pre + "9: status is not the last holding's state");
        // 9: the making event
        if (w.key(me) != "artifact_made" || w.one(me, "artifact") != aid || w.year(me) != I(a, 9)) {
            out.push_back(pre + "9: made_event is not its artifact_made");
        } else {
            if (w.one(me, "civilization") != c || w.one(me, "maker") != (mk <= 3 ? I(a, 5) : 0) ||
                w.one(me, "god") != (mk == 4 ? I(a, 6) : 0) || w.one(me, "maker_realm") != (mk == 2 ? I(a, 7) : 0) ||
                w.one(me, "site") != I(a, 8) || w.val(me, "maker_kind") != mk || w.val(me, "deep") != I(a, 12)) {
                out.push_back(pre + "9: made_event's participants or values against the row");
            }
            const auto cs = w.causes(me);
            // reference reading A-R1: occasion_event is the occasion (a cause, not the delving_deepened)
            if (std::find(cs.begin(), cs.end(), oc) == cs.end() || w.key(oc) == "delving_deepened") {
                out.push_back(pre + "9: occasion_event is not its occasion");
            }
        }
        // 2: the occasion
        bool matched = false;
        const std::string ok = w.key(oc);
        for (const auto& [kk, rr] : t.occasions) {
            if (kk != ok) continue;
            const auto cs = w.role(oc, rr);
            if (std::find(cs.begin(), cs.end(), c) != cs.end()) matched = true;
        }
        if (!matched) out.push_back(pre + "2: occasion " + ok + " matches no occasion with its realm");
        per_occasion[oc] += 1;
        made_years[c].push_back(I(a, 9));
        // 3: makers
        if (mk == SMITH_MADE || mk == COMMISSION) {
            const auto f = fig.find(I(a, 5));
            if (f == fig.end() || I(*f->second, 3) != ROLE_SMITH || I(*f->second, 9) != me || I(*f->second, 6) != I(a, 7)) {
                out.push_back(pre + "3: maker is not a smith of the maker realm made by it");
            }
            if (mk == COMMISSION) {
                if (!forges_at(w, c, me, false).empty()) out.push_back(pre + "3: a commission while home had a forge");
                const auto ps = partners_at(w, c, me);
                if (std::find(ps.begin(), ps.end(), I(a, 7)) == ps.end() || forges_at(w, I(a, 7), me, true).empty()) {
                    out.push_back(pre + "3: commission by a realm that is no partner with a forge");
                }
            }
            const std::int64_t dl = max_depth(w, forges_at(w, I(a, 7), me, true), me);
            const std::int64_t ds = max_depth(w, forges_at(w, I(a, 7), me, false), me);
            if ((I(a, 12) == 1 && dl < t.deep_depth) || (I(a, 12) == 0 && ds >= t.deep_depth)) {
                out.push_back(pre + "3: deep against the forge's depth");
            }
        } else if (mk == CASTER_MADE) {
            const auto f = fig.find(I(a, 5));
            const auto s = w.base.site.find(I(a, 8));
            const bool ok3 = f != fig.end() && I(*f->second, 3) == ROLE_CASTER && I(*f->second, 6) == c &&
                             s != w.base.site.end() && I(*s->second, 2) == SITE_TOWER && I(*s->second, 10) < me &&
                             (I(*s->second, 12) == 0 || I(*s->second, 12) > me);
            if (!ok3) out.push_back(pre + "3: caster of the home realm without an open tower");
            if (I(a, 12) != 0) out.push_back(pre + "9: deep for a caster");
        } else if (mk == GOD_GIVEN) {
            if (w.one(oc, "actor") != I(a, 6) && w.one(oc, "god") != I(a, 6)) out.push_back(pre + "3: god not named");
            if (I(a, 12) != 0) out.push_back(pre + "9: deep for a god");
        } else {
            out.push_back(pre + "9: maker_kind " + n(mk));
        }
    }
    for (const auto& [oc, k] : per_occasion) {
        if (k > 1) out.push_back("2: e" + n(oc) + " occasions " + n(k) + " artifacts");
    }
    for (auto& [c, ys] : made_years) {
        std::sort(ys.begin(), ys.end());
        for (std::size_t i = 1; i < ys.size(); ++i) {
            if (ys[i] - ys[i - 1U] < t.rest) out.push_back("2: c" + n(c) + " made two within realm_rest_years");
        }
    }
    // 4: a figure bears at most one artifact of each class at every year's end
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<std::pair<std::int64_t, std::int64_t>>> per;
    for (const auto& [aid, hs] : w.hold) {
        for (const Row* h : hs) {
            if (I(*h, 4) == BORNE) {
                per[{I(*h, 6), w.cls(aid)}].push_back({I(*h, 2), null_at(*h, 3) ? NONE : I(*h, 3)});
            }
        }
    }
    for (const auto& [fc, iv] : per) {
        for (const auto& [y0, unused] : iv) {
            (void)unused;
            std::int64_t k = 0;
            for (const auto& [a0, b0] : iv) k += (a0 <= y0 && (b0 == NONE || b0 > y0)) ? 1 : 0;
            if (k > 1) {
                out.push_back("4: f" + n(fc.first) + " bears two of class " + n(fc.second) + " at the end of " + n(y0));
                break;
            }
        }
    }
    // 7: a war_declared cites claim_event when the claim was above 0 (from the holdings before the declaration)
    for (const std::int64_t e : w.of("war_declared")) {
        const std::int64_t a_ = w.one(e, "attacker"), b_ = w.one(e, "defender");
        std::int64_t cl = 0, ce = 0;
        for (const auto& [aid, ap] : w.art) {
            if (I(*ap, 3) != a_) continue;
            const Row* h = w.holding_at_event(aid, e);
            if (h == nullptr) continue;
            if ((I(*h, 4) == KEPT && I(*h, 6) == b_) || (I(*h, 4) == BORNE && w.fig_civ(I(*h, 6)) == b_)) {
                cl += I(*w.kind.at(I(*ap, 2)), 7);
                ce = std::max(ce, I(*h, 12));
            }
        }
        const auto cs = w.causes(e);
        if (std::min(cl, t.claim_max) > 0 && std::find(cs.begin(), cs.end(), ce) == cs.end()) {
            out.push_back("7: war_declared e" + n(e) + " does not cite its claim's event e" + n(ce));
        }
    }
    // 8: 12.1 every cited act in force for its battle's side; 12.2 no spread back within cult_return_years
    for (const char* key : {"battle", "battle_at_pass", "allied_battle", "battle_led"}) {
        for (const std::int64_t e : w.of(key)) {
            const std::int64_t W = w.one(e, "winner"), L = w.one(e, "loser"), y = w.year(e);
            for (const std::int64_t x : w.causes(e)) {
                const std::string xk = w.key(x);
                const bool valour = xk == "divine_bless" && w.val(x, "effect") == 2;
                const bool defeat = xk == "divine_curse" && w.val(x, "effect") == 3;
                if (!valour && !defeat) continue;
                const bool in_force = w.year(x) <= y && y < w.year(x) + w.val(x, "years");
                if (!in_force || w.one(x, "civilization") != (valour ? W : L)) {
                    out.push_back("8: " + std::string(key) + " e" + n(e) + " cites e" + n(x) + " not in force for its side");
                }
            }
        }
    }
    if (t.cult_return > 0) {
        for (const std::int64_t e : w.of("cult_spread")) {
            const std::int64_t k = w.one(e, "faction"), a_ = w.one(e, "civilization"), y = w.year(e);
            for (const std::int64_t s : w.of("cult_suppressed")) {
                if (s < e && w.one(s, "faction") == k && w.one(s, "civilization") == a_ && y - t.cult_return < w.year(s) &&
                    w.year(s) <= y) {
                    out.push_back("8: cult_spread e" + n(e) + " within cult_return_years of e" + n(s));
                }
            }
        }
    }
    return out;
}

// 10 item 6 from rows (cult_returns_barred is not in rows: left out, reference reading A-R14).
inline std::vector<std::pair<std::string, std::int64_t>> stats(const World& w) {
    using namespace p511api;
    std::vector<std::pair<std::string, std::int64_t>> st;
    std::int64_t mk[5] = {0, 0, 0, 0, 0}, deep = 0, cl[6] = {0, 0, 0, 0, 0, 0}, holdings = 0, lost_end = 0, others = 0;
    for (const auto& [aid, ap] : w.art) {
        mk[std::clamp<std::int64_t>(I(*ap, 4), 0, 4)] += 1;
        deep += I(*ap, 12);
        cl[std::clamp<std::int64_t>(w.cls(aid), 0, 5)] += 1;
        holdings += static_cast<std::int64_t>(w.hold.at(aid).size());
        const Row& h = w.open(aid);
        const std::int64_t s = I(h, 4);
        if (s == HOARDED || s == LOST || s == BURIED) lost_end += 1;
        if (s == ENSHRINED) {
            const Row& site = *w.base.site.at(I(h, 6));
            if (I(site, 2) == SITE_SANCTUM || I(site, 12) != 0) lost_end += 1;
        }
        const auto home = w.base.civ.find(I(*ap, 3));
        const bool home_alive = home != w.base.civ.end() && null_at(*home->second, 8);
        if (home_alive && ((s == KEPT && I(h, 6) != I(*ap, 3)) || (s == BORNE && w.fig_civ(I(h, 6)) != I(*ap, 3)))) {
            others += 1;
        }
    }
    std::set<std::int64_t> moved;
    for (const char* k : {"artifact_taken", "artifact_lost", "artifact_hoarded", "artifact_recovered"}) {
        for (const std::int64_t e : w.of(k)) moved.insert(w.one(e, "artifact"));
    }
    std::int64_t wars = 0;
    for (const std::int64_t e : w.of("war_declared")) {
        const std::int64_t a_ = w.one(e, "attacker"), b_ = w.one(e, "defender");
        const auto cs = w.causes(e);
        for (const auto& [aid, ap] : w.art) {
            const Row* h = w.holding_at_event(aid, e);
            if (I(*ap, 3) != a_ || h == nullptr || I(*w.kind.at(I(*ap, 2)), 7) <= 0) continue;
            if (std::find(cs.begin(), cs.end(), I(*h, 12)) == cs.end()) continue;
            if ((I(*h, 4) == KEPT && I(*h, 6) == b_) || (I(*h, 4) == BORNE && w.fig_civ(I(*h, 6)) == b_)) {
                ++wars;
                break;
            }
        }
    }
    std::int64_t with_arts = 0, under_acts = 0, smiths = 0;
    for (const std::int64_t e : w.of("battle_led")) {
        for (const std::int64_t x : w.causes(e)) {
            if (w.key(x) == "artifact_made") {
                ++with_arts;
                break;
            }
        }
    }
    for (const char* key : {"battle", "battle_at_pass", "allied_battle", "battle_led"}) {
        for (const std::int64_t e : w.of(key)) {
            for (const std::int64_t x : w.causes(e)) {
                if (w.key(x) == "divine_bless" || w.key(x) == "divine_curse") {
                    ++under_acts;
                    break;
                }
            }
        }
    }
    for (const auto& [f, r] : w.base.figure) smiths += I(*r, 3) == ROLE_SMITH ? 1 : 0;
    const std::int64_t made = static_cast<std::int64_t>(w.art.size());
    st = {{"artifacts_made", made},
          {"artifacts_smiths", mk[1] + mk[2]},
          {"artifacts_commissioned", mk[2]},
          {"artifacts_casters", mk[3]},
          {"artifacts_gods", mk[4]},
          {"artifacts_deep", deep},
          {"artifacts_arms", cl[1]},
          {"artifacts_armour", cl[2]},
          {"artifacts_regalia", cl[3]},
          {"artifacts_relic", cl[4]},
          {"artifacts_staff", cl[5]},
          {"artifacts_taken", static_cast<std::int64_t>(w.of("artifact_taken").size())},
          {"artifacts_lost", static_cast<std::int64_t>(w.of("artifact_lost").size())},
          {"artifacts_hoarded", static_cast<std::int64_t>(w.of("artifact_hoarded").size())},
          {"artifacts_recovered", static_cast<std::int64_t>(w.of("artifact_recovered").size())},
          {"artifacts_buried", static_cast<std::int64_t>(w.of("artifact_buried").size())},
          {"artifact_holdings", holdings},
          {"artifacts_changed_hands", static_cast<std::int64_t>(moved.size())},
          {"artifacts_lost_at_end", lost_end},
          {"artifacts_held_by_others", others},
          {"smiths", smiths},
          {"wars_citing_artifacts", wars},
          {"battles_with_artifacts", with_arts},
          {"battles_under_acts", under_acts}};
    return st;
}
inline std::int64_t stat(const World& w, const std::string& key) {
    for (const auto& [k, v] : stats(w)) {
        if (k == key) return v;
    }
    return NONE;
}

// The paths of 6 a world shows (the reference's paths_of, the parts these tests require).
inline std::set<std::string> paths_of(const World& w) {
    static const char* const words[7] = {"", "kept", "borne", "hoarded", "lost", "buried", "enshrined"};
    static const char* const classes[6] = {"", "arms", "armour", "regalia", "relic", "staff"};
    std::set<std::string> seen;
    for (const auto& [aid, ap] : w.art) {
        seen.insert("maker " + n(I(*ap, 4)));
        seen.insert(std::string("class ") + classes[w.cls(aid)]);
        if (I(*ap, 12) == 1) seen.insert("deep");
        const auto& hs = w.hold.at(aid);
        seen.insert(std::string("first ") + words[I(*hs.front(), 4)]);
        for (std::size_t k = 1; k < hs.size(); ++k) {
            const std::int64_t x = I(*hs[k], 12);
            const std::string xk = w.key(x);
            const bool written = is_artifact_key(xk);
            const auto cs = w.causes(x);
            const std::string trig = written ? (cs.empty() ? std::string() : w.key(cs[0])) : xk;
            seen.insert("move " + trig + " " + words[I(*hs[k - 1U], 4)] + ">" + words[I(*hs[k], 4)] +
                        (written ? "" : " quiet"));
        }
    }
    for (const std::string& k : artifact_keys()) {
        if (!w.of(k).empty()) seen.insert(k);
    }
    return seen;
}

// 11.1 from rows: artifact_at (reference reading A-R16 for a hoard: the monster's lair at the year), artifacts_held,
// artifacts_placed.
inline p511api::Place artifact_at(const World& w, std::int64_t a, std::int64_t year) {
    using namespace p511api;
    const Row* h = nullptr;
    for (const Row* x : w.hold.at(a)) {
        if (I(*x, 2) <= year) h = x;
    }
    if (h == nullptr) return Place{};
    Place p{I(*h, 4), I(*h, 5), I(*h, 6), 0, 0, 0, 0, 0, I(*h, 2), I(*h, 12)};
    if (p.state == HOARDED) {
        const Row* best = nullptr;
        for (const auto& [sid, s] : w.base.site) {
            if (I(*s, 2) == SITE_LAIR && I(*s, 8) == p.holder && I(*s, 9) <= year &&
                (best == nullptr || I(*s, 9) > I(*best, 9) || (I(*s, 9) == I(*best, 9) && sid > I(*best, 0)))) {
                best = s;
            }
        }
        if (best != nullptr) {
            p.region = I(*best, 3);
            p.site = I(*best, 0);
            p.x = I(*best, 4);
            p.y = I(*best, 5);
        }
    } else if (p.state >= LOST) {
        p.region = I(*h, 7);
        p.settlement = I(*h, 8);
        p.site = I(*h, 9);
        p.x = I(*h, 10);
        p.y = I(*h, 11);
    }
    return p;
}
inline std::vector<std::int64_t> artifacts_held(const World& w, std::int64_t holder_type, std::int64_t holder,
                                                std::int64_t year) {
    std::vector<std::int64_t> out;
    for (const auto& [aid, ap] : w.art) {
        const p511api::Place p = artifact_at(w, aid, year);
        if (p.state != 0 && p.holder_type == holder_type && p.holder == holder) out.push_back(aid);
    }
    return out;
}
inline std::vector<std::int64_t> artifacts_placed(const World& w, std::int64_t year) {
    std::vector<std::int64_t> out;
    for (const auto& [aid, ap] : w.art) {
        if (artifact_at(w, aid, year).state >= 3) out.push_back(aid);
    }
    return out;
}

}  // namespace p511_check
