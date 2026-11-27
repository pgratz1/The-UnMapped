#pragma once

// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2): the formulas of 3.3, 4.2, 5, 6.1, 6.3, 7.1,
// 7.2, 8, 21.4 and 21.5 as functions (p504a_f), and the invariants of 16 items 13 and 19 that rows show (p504a_check),
// as the reference's invariant_violations_028 (reference/p5_04_alliances_reference.py), violations_028
// (p5_03_relations_reference.py) and violations_028 (p5_06_figures_reference.py). Messages name the section first.
// Written from the spec before the implementation; no library name beyond the 0.27.0 tests'.
//
// What rows cannot show is left to the reference's replay and named at each check: a refusal's hidden factors f3
// (the forces), f4 (the warmth) and f6 (claims dropped without an event), the temper of an unrecorded reign (f5), a
// contribution to a disposition (a credit may cite an old event).

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p5_04_alliances_checks.hpp"
#include "p5_04a_api.hpp"

namespace p504a_f {

inline constexpr std::int64_t HATRED = 0, WARINESS = 1, FRIENDSHIP = 2;
inline constexpr std::int64_t GRASPING = 3;   // FigureView::temper's code (2)

// The fields of history.alliances that 4.2 reads.
struct JoinTuning {
    std::int64_t weary_permille = 600, busy_permille = 250, fear_floor = 300, warmth_floor = 400,
                 grasping_permille = 600, claim_permille = 500, soured_permille = 500;
};

// 3.3: large(z).
inline bool large(std::int64_t share, std::int64_t part, std::int64_t large_share, std::int64_t large_part) {
    return share >= large_share || part >= large_part;
}

// 4.2: [f0, ..., f7] of a call to z.
inline std::array<std::int64_t, 8> factors(const JoinTuning& t, std::int64_t join, bool weary, std::int64_t open_wars,
                                           std::int64_t sigma, std::int64_t warmth, std::int64_t temper, bool claim,
                                           bool soured) {
    return {join,
            weary ? t.weary_permille : 1000,
            std::max<std::int64_t>(0, 1000 - t.busy_permille * open_wars),
            t.fear_floor + (1000 - t.fear_floor) * sigma / 1000,
            t.warmth_floor + (1000 - t.warmth_floor) * warmth / 1000,
            temper == GRASPING ? t.grasping_permille : 1000,
            claim ? t.claim_permille : 1000,
            soured ? t.soured_permille : 1000};
}

// 4.2: p = defend_permille, then p = p x f_k div 1000 for k = 0, 1, ..., 7.
inline std::int64_t chance(std::int64_t defend, const std::array<std::int64_t, 8>& f) {
    std::int64_t p = defend;
    for (const std::int64_t x : f) p = p * x / 1000;
    return p;
}

// 4.2: the code k in 1 to 7 with the smallest f_k below 1000 (the smaller code at a tie); 0 when none.
inline std::int64_t motive(const std::array<std::int64_t, 8>& f) {
    std::size_t best = 0;
    for (std::size_t k = 1; k < 8; ++k) {
        if (f[k] < 1000 && (best == 0 || f[k] < f[best])) best = k;
    }
    return static_cast<std::int64_t>(best);
}

// 4.3: a refusal of motive 1 or 2 is excused.
inline bool excused(std::int64_t m) { return m == 1 || m == 2; }

// 5: the betrayal's motive, the first that holds.
inline std::int64_t betrayal_motive(std::int64_t temper, std::int64_t d_ab, std::int64_t force_a, std::int64_t force_b,
                                    std::int64_t cooled, std::int64_t weak) {
    if (temper == GRASPING) return 0;
    if (d_ab < cooled) return 1;
    if (force_b * 1000 < weak * force_a) return 2;
    return 3;
}

// 6.1: the due years s + n x term (n >= 1) up to `upto`.
inline std::vector<std::int64_t> due_years(std::int64_t s, std::int64_t term, std::int64_t upto) {
    std::vector<std::int64_t> out;
    for (std::int64_t d = s + term; d <= upto; d += term) out.push_back(d);
    return out;
}

// 6.3: renewed k = #{n >= 1 : s + n x term <= end - 1}.
inline std::int64_t renewal_count(std::int64_t s, std::int64_t term, std::int64_t end) {
    return end - 1 >= s ? (end - 1 - s) / term : 0;
}

// 7.1: change 0 into hatred, 1 a thaw, 2 a decline, 3 into friendship.
inline std::int64_t turn_change(std::int64_t old_band, std::int64_t new_band) {
    if (new_band == HATRED) return 0;
    if (new_band == FRIENDSHIP) return 3;
    return old_band == HATRED ? 1 : 2;
}

// 7.2: a pair turns inside the cool-down only with a fresh contribution (reading R3: a pair that never turned has
// no cool-down).
inline bool turn_allowed(std::int64_t y, std::int64_t last_turn_year, bool has_turned, std::int64_t cooldown,
                         bool fresh) {
    return !has_turned || y - last_turn_year >= cooldown || fresh;
}

// 8: an entry held at least expel_settled_years is expelled only after a turn into hatred within expel_fresh_years
// (reading R4: none when the pair never turned into hatred).
inline bool expel_allowed(std::int64_t y, std::int64_t since_year, std::optional<std::int64_t> hatred_year,
                          std::int64_t settled, std::int64_t fresh) {
    if (y - since_year < settled) return true;
    return hatred_year.has_value() && *hatred_year >= y - fresh;
}

// 21.4: D = tower_spacing_permille x W div 1000; Chebyshev distance.
inline std::int64_t tower_spacing(std::int64_t permille, std::int64_t width) { return permille * width / 1000; }
inline std::int64_t chebyshev(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) {
    return std::max(x0 > x1 ? x0 - x1 : x1 - x0, y0 > y1 ? y0 - y1 : y1 - y0);
}

// 21.5 item 3: quiet = g when g >= hero_return_years, else 0.
inline std::int64_t hero_quiet(std::int64_t g, std::int64_t return_years) { return g >= return_years ? g : 0; }

// 8.5: the choice of n words (2 to 8): two words w0 for 1, w1 otherwise; else word i for value i (0 <= i < n - 1)
// and the last word for every other value, negative included.
inline std::size_t choice_index(std::size_t n, std::int64_t v) {
    if (n == 2) return v == 1 ? 0U : 1U;
    return v >= 0 && v < static_cast<std::int64_t>(n) - 1 ? static_cast<std::size_t>(v) : n - 1;
}

}  // namespace p504a_f

namespace p504a_check {

using p503check::at;
using p503check::World;
using p504check::is_null;
using p504check::Rows;
using Row = eng::Row;

inline std::string s(std::int64_t v) { return std::to_string(v); }

// The tuning the checks read (history.alliances, history.relations, history.figures; the spec's shipped values when
// a field is absent, as the reference's fallback).
struct Tuning {
    std::int64_t refusals_to_break = 2, weary_years = 50, join_weary_permille = 600, join_busy_permille = 250,
                 soured_join_permille = 500, turn_cooldown_years = 300, expel_settled_years = 800,
                 expel_fresh_years = 150, tower_spacing_permille = 30, hero_return_years = 100;
};
inline Tuning tuning_of(const worldgen::data::ContentSet& c) {
    using p504a_api::tuning_or;
    Tuning t;
    t.refusals_to_break = tuning_or(c, "alliances", "refusals_to_break", t.refusals_to_break);
    t.weary_years = tuning_or(c, "alliances", "weary_years", t.weary_years);
    t.join_weary_permille = tuning_or(c, "alliances", "join_weary_permille", t.join_weary_permille);
    t.join_busy_permille = tuning_or(c, "alliances", "join_busy_permille", t.join_busy_permille);
    t.soured_join_permille = tuning_or(c, "alliances", "soured_join_permille", t.soured_join_permille);
    t.turn_cooldown_years = tuning_or(c, "relations", "turn_cooldown_years", t.turn_cooldown_years);
    t.expel_settled_years = tuning_or(c, "relations", "expel_settled_years", t.expel_settled_years);
    t.expel_fresh_years = tuning_or(c, "relations", "expel_fresh_years", t.expel_fresh_years);
    t.tower_spacing_permille = tuning_or(c, "figures", "tower_spacing_permille", t.tower_spacing_permille);
    t.hero_return_years = tuning_or(c, "figures", "hero_return_years", t.hero_return_years);
    return t;
}

// Whether the world's catalogue gives kind `kind` the value `value` (a 0.28.0 world: 14.4).
inline bool has_value_key(const World& w, const std::string& kind, const std::string& value) {
    std::int64_t id = -1;
    for (const Row& k : w.rows.t.event_kind) {
        if (k.at(1).bytes == kind) id = at(k, 0);
    }
    for (const Row& v : w.rows.t.event_kind_value) {
        if (at(v, 0) == id && v.at(2).bytes == value) return true;
    }
    return false;
}

// A relation row (a_type, a, b_type, b, kind, since, until, since_event, until_event).
inline bool within_row(const Row& r, std::int64_t x) { return at(r, 7) < x && (is_null(r, 6) || x <= at(r, 8)); }

inline const Row* alliance_row_at(const Rows& r, std::int64_t a, std::int64_t b, std::int64_t e) {
    const Row* out = nullptr;
    for (const Row* row : r.pair_rows("alliance", a, b)) {
        if (at(*row, 7) <= e && (is_null(*row, 6) || at(*row, 8) >= e)) out = row;
    }
    return out;
}

inline bool pair_is(const Rows& r, std::int64_t e, const char* ra, const char* rb, std::int64_t a, std::int64_t b) {
    const std::int64_t x = r.one(e, ra), y = r.one(e, rb);
    return (x == a && y == b) || (x == b && y == a);
}

// last_war_end(c) when event e is appended (reading R5): the largest (year, event) of an event that closed a war row
// of c (principal or joined) and is a peace, war_won, truce, ally_withdrew or search_abandoned, before e.
inline std::pair<std::int64_t, std::int64_t> last_war_end_at(const Rows& r, std::int64_t c, std::int64_t e) {
    std::pair<std::int64_t, std::int64_t> best{0, 0};
    for (const Row* row : r.rows("war")) {
        if (at(*row, 1) != c && at(*row, 3) != c) continue;
        if (is_null(*row, 6)) continue;
        const std::int64_t u = at(*row, 8);
        if (u >= e || r.x().ev.count(u) == 0U) continue;
        const std::string k = r.kind(u);
        if (k != "peace" && k != "war_won" && k != "truce" && k != "ally_withdrew" && k != "search_abandoned") continue;
        best = std::max(best, std::make_pair(r.year(u), u));
    }
    return best;
}

// open_wars_of(c) when e is appended (P5-04 7.6: the open wars with c as a side and c's open joined records): the
// open war rows of declarations with c as a side, and the open joined rows whose ally_joined_war has c as the ally (a
// row opened by another realm joining against c is that realm's record; integration, turbulent seed 3 e1678).
inline std::int64_t open_wars_at(const Rows& r, std::int64_t c, std::int64_t e) {
    std::int64_t n = 0;
    for (const Row* row : r.rows("war")) {
        if ((at(*row, 1) != c && at(*row, 3) != c) || !Rows::open_at_event(*row, e)) continue;
        const std::int64_t opened = at(*row, 7);
        if (r.kind(opened) == "ally_joined_war" && r.one(opened, "ally") != c) continue;
        ++n;
    }
    return n;
}

// The first withheld share of grievance 1 of the alliance row's pair with ally z, before `upto` (0 none).
inline std::int64_t soured_share(const Rows& r, const Row& row, std::int64_t z, std::int64_t upto) {
    for (const std::int64_t x : r.of("spoils_withheld")) {
        if (within_row(row, x) && x < upto && r.value(x, "grievance") == 1 && r.one(x, "ally") == z &&
            pair_is(r, x, "ally", "principal", at(row, 1), at(row, 3))) {
            return x;
        }
    }
    return 0;
}

// 3.5: call_due at z's defence call to p at e: soured with z wronged, and no defence call of z to p evaluated since.
inline std::int64_t call_due_at(const Rows& r, const Row& row, std::int64_t z, std::int64_t p, std::int64_t e) {
    const std::int64_t so = soured_share(r, row, z, e);
    if (so == 0) return 0;
    for (const char* k : {"ally_joined_war", "ally_refused"}) {
        for (const std::int64_t x : r.of(k)) {
            if (so < x && x < e && r.one(x, "ally") == z && r.one(x, "partner") == p &&
                (std::string(k) == "ally_refused" || r.value(x, "side") == 0)) {
                return 0;
            }
        }
    }
    return so;
}

inline bool cites(const p503check::Index& x, std::int64_t e, std::int64_t c) {
    const auto cs = x.cause_list(e);
    return std::find(cs.begin(), cs.end(), c) != cs.end();
}

// 16 item 13 (alliances): the replaced item 12 and the new invariants rows show.
inline std::vector<std::string> alliance_violations(const World& w, const Tuning& t) {
    std::vector<std::string> out;
    if (!has_value_key(w, "spoils_withheld", "grievance")) return {"0.28.0: spoils_withheld has no grievance"};
    const Rows r(w);
    const p503check::Index& x = r.x();
    // 12 replaced
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
        std::vector<std::int64_t> wh;
        for (const std::int64_t c : x.cause_list(e)) {
            if (r.kind(c) == "spoils_withheld") wh.push_back(c);
        }
        if (wh.size() != 1U || r.one(wh[0], "ally") != z || r.one(wh[0], "principal") != P || wh[0] < at(*closed, 7) ||
            r.value(wh[0], "grievance") < 1) {
            out.push_back("12: e" + s(e) + " cites no withheld share of its pair with a grievance");
            continue;
        }
        const std::int64_t reason = r.value(e, "reason"), years = r.value(e, "years"), g = r.value(wh[0], "grievance");
        if (years != r.year(e) - r.year(wh[0])) out.push_back("12: e" + s(e) + " years " + s(years));
        if ((reason == 2 && g != 2) || ((reason == 0 || reason == 1) && g != 1)) {
            out.push_back("12: e" + s(e) + " reason " + s(reason) + " for grievance " + s(g));
        }
        if (r.region(e) != r.region(wh[0])) out.push_back("12: e" + s(e) + " not in its share's region");
    }
    // 3.4: grievance against the alliance
    for (const std::int64_t e : r.of("spoils_withheld")) {
        const std::int64_t g = r.value(e, "grievance");
        const Row* row = alliance_row_at(r, r.one(e, "ally"), r.one(e, "principal"), e);
        const bool open_non_origin = row != nullptr && r.value(at(*row, 7), "basis") != 0;
        if ((g == 1 || g == 2) && !open_non_origin) out.push_back("3.4: e" + s(e) + " grievance " + s(g) + " without an alliance");
        if (g == 0 && open_non_origin && is_null(*row, 6)) out.push_back("3.4: e" + s(e) + " grievance 0 on an open alliance");
        if (g == 2 && row != nullptr && is_null(*row, 6)) out.push_back("3.4: e" + s(e) + " grievance 2 ends no alliance");
    }
    // no alliance has three withheld shares with a grievance
    for (const Row* row : r.rows("alliance")) {
        std::int64_t n = 0;
        for (const std::int64_t e : r.of("spoils_withheld")) {
            if (within_row(*row, e) && r.value(e, "grievance") >= 1 &&
                pair_is(r, e, "ally", "principal", at(*row, 1), at(*row, 3))) {
                ++n;
            }
        }
        if (n >= 3) out.push_back("13: alliance e" + s(at(*row, 7)) + " has " + s(n) + " withheld shares with a grievance");
    }
    // refusals
    for (const std::int64_t e : r.of("ally_refused")) {
        const std::int64_t z = r.one(e, "ally"), p = r.one(e, "partner");
        const std::int64_t m = r.value(e, "motive"), cnt = r.value(e, "count");
        const Row* row = alliance_row_at(r, z, p, e);
        if (row == nullptr) {
            out.push_back("4.3: e" + s(e) + " without its alliance");
            continue;
        }
        if ((cnt == 0) != p504a_f::excused(m)) out.push_back("4.3: e" + s(e) + " motive " + s(m) + " count " + s(cnt));
        std::int64_t prior = 0;
        for (const std::int64_t y : r.of("ally_refused")) {
            if (y < e && within_row(*row, y) && pair_is(r, y, "ally", "partner", at(*row, 1), at(*row, 3)) &&
                r.value(y, "count") > 0) {
                ++prior;
            }
        }
        if (cnt > 0 && cnt != prior + 1) out.push_back("4.3: e" + s(e) + " count " + s(cnt) + " after " + s(prior));
        const auto lwe = last_war_end_at(r, z, e);
        const bool weary = lwe.second != 0 && lwe.first >= r.year(e) - t.weary_years;
        const std::int64_t so = call_due_at(r, *row, z, p, e);
        const std::map<std::int64_t, std::int64_t> f{
            {1, weary ? t.join_weary_permille : 1000},
            {2, std::max<std::int64_t>(0, 1000 - t.join_busy_permille * open_wars_at(r, z, e))},
            {7, so != 0 ? t.soured_join_permille : 1000}};
        for (const auto& [k, v] : f) {
            if (m == 0 && v < 1000) out.push_back("4.2: e" + s(e) + " motive 0 with f" + s(k) + " " + s(v));
        }
        if (const auto it = f.find(m); it != f.end()) {
            if (it->second >= 1000) out.push_back("4.2: e" + s(e) + " motive " + s(m) + " with its factor 1000");
            for (const auto& [k, v] : f) {
                if (k != m && v < 1000 && !(it->second < v || (it->second == v && m < k))) {
                    out.push_back("4.2: e" + s(e) + " motive " + s(m) + " against f" + s(k) + " " + s(v));
                }
            }
        }
        if (m == 1 && !cites(x, e, lwe.second)) out.push_back("4.3: e" + s(e) + " motive 1 cites no war's end");
        if (m == 7 && !cites(x, e, so)) out.push_back("4.3: e" + s(e) + " motive 7 cites not the soured share");
        if (m == 6) {
            bool claim = false;
            for (const std::int64_t c : x.cause_list(e)) claim = claim || r.kind(c) == "claim" || r.kind(c) == "claim_settlements";
            if (!claim) out.push_back("4.3: e" + s(e) + " motive 6 cites no claim");
        }
        if (cnt > 0 && cnt >= t.refusals_to_break) {
            bool broke = false;
            for (const std::int64_t k : r.kids(e)) broke = broke || (r.kind(k) == "alliance_broken" && r.value(k, "reason") == 0);
            if (!broke) out.push_back("4.3: e" + s(e) + " count " + s(cnt) + " breaks nothing");
        }
    }
    for (const std::int64_t b : r.of("alliance_broken")) {
        if (r.value(b, "reason") != 0) continue;
        std::vector<std::int64_t> rf;
        for (const std::int64_t c : x.cause_list(b)) {
            if (r.kind(c) == "ally_refused") rf.push_back(c);
        }
        if (rf.size() != 1U || r.value(rf[0], "count") < t.refusals_to_break) {
            out.push_back("4.3: alliance_broken e" + s(b) + " without a counted refusal at the bound");
        }
    }
    // betrayals
    for (const std::int64_t b : r.of("alliance_betrayed")) {
        const std::int64_t m = r.value(b, "motive");
        if (m < 0 || m > 3) out.push_back("5: e" + s(b) + " motive " + s(m));
        for (const Row* row : r.pair_rows("alliance", r.one(b, "betrayer"), r.one(b, "betrayed"))) {
            if (!is_null(*row, 6) && at(*row, 8) == b && r.value(b, "years") != r.year(b) - at(*row, 5)) {
                out.push_back("5: e" + s(b) + " years " + s(r.value(b, "years")));
            }
        }
    }
    // renewals
    for (const std::int64_t n : r.of("alliance_renewed")) {
        const std::int64_t a = r.one(n, "ally_a"), b = r.one(n, "ally_b");
        const Row* row = alliance_row_at(r, a, b, n);
        if (row == nullptr) {
            out.push_back("6: e" + s(n) + " without its alliance");
            continue;
        }
        std::int64_t R = at(*row, 7);
        for (const std::int64_t y : r.of("alliance_renewed")) {
            if (y > R && y < n && pair_is(r, y, "ally_a", "ally_b", a, b)) R = y;
        }
        bool notable = r.value(n, "basis") != r.value(R, "basis");
        for (const std::int64_t y : r.of("ally_joined_war")) notable = notable || (y > R && y < n && pair_is(r, y, "ally", "partner", a, b));
        for (const std::int64_t y : r.of("spoils_shared")) notable = notable || (y > R && y < n && pair_is(r, y, "ally", "principal", a, b));
        if (!notable) out.push_back("6.2: e" + s(n) + " renews with nothing notable since e" + s(R));
        if (!cites(x, n, R)) out.push_back("6.2: e" + s(n) + " does not cite e" + s(R));
        // 6.1: never early. A renewal that waited is caught up one due year per T2 or Q4, so the j-th written
        // renewal of the row needs j due years at or before its year (integration, alliances-small seed 3).
        const std::int64_t term = r.value(at(*row, 7), "term");
        std::size_t j = 1;
        for (const std::int64_t y : r.of("alliance_renewed")) {
            if (y > at(*row, 7) && y < n && pair_is(r, y, "ally_a", "ally_b", a, b)) ++j;
        }
        if (p504a_f::due_years(at(*row, 5), std::max<std::int64_t>(1, term), r.year(n)).size() < j) {
            out.push_back("6.1: e" + s(n) + " is written renewal " + s(static_cast<std::int64_t>(j)) +
                          " of its row with fewer due years");
        }
    }
    for (const std::int64_t e : r.of("alliance_formed")) {
        const std::int64_t term = r.value(e, "term"), band = r.value(e, "band");
        if (term < 1 || band < 0 || band > 2) out.push_back("10: e" + s(e) + " term " + s(term) + " band " + s(band));
        if (r.value(e, "basis") != 0 && x.cause_list(e).size() != 1U) out.push_back("10 item 2: e" + s(e) + " cites more than its basis");
    }
    // 9.2: arms
    std::map<std::int64_t, std::int64_t> cited;
    for (const char* k : {"battle", "battle_at_pass", "allied_battle", "battle_led"}) {
        for (const std::int64_t b : r.of(k)) {
            std::vector<std::int64_t> arms;
            for (const std::int64_t c : x.cause_list(b)) {
                if (r.kind(c) == "arms_sold") arms.push_back(c);
            }
            if (arms.size() > 1U) out.push_back("9.2: e" + s(b) + " cites " + s(static_cast<std::int64_t>(arms.size())) + " sales");
            for (const std::int64_t a : arms) {
                if (r.one(a, "buyer") != r.one(b, "winner")) out.push_back("9.2: e" + s(b) + " cites arms not of its winner");
                if (!cited.emplace(a, b).second) out.push_back("9.2: arms e" + s(a) + " cited twice");
            }
        }
    }
    return out;
}

// 16 item 13 (relations): 7.1's change, 7.2's cool-down (not on the fallback causes alone), 8 on every expulsion.
inline std::vector<std::string> relation_violations(const World& w, const Tuning& t) {
    std::vector<std::string> out;
    if (!has_value_key(w, "disposition_turned", "change")) return {"0.28.0: disposition_turned has no change"};
    const Rows r(w);
    const p503check::Index& x = r.x();
    using P = std::pair<std::int64_t, std::int64_t>;
    std::map<P, std::int64_t> band, met, last, hatred;
    for (const std::int64_t e : r.of("peoples_met")) {
        const P k{r.one(e, "race_a"), r.one(e, "race_b")};
        band[k] = r.value(e, "band");
        met[k] = e;
    }
    std::vector<std::int64_t> order = r.of("disposition_turned");
    for (const std::int64_t e : r.of("people_expelled")) order.push_back(e);
    std::sort(order.begin(), order.end());
    for (const std::int64_t e : order) {
        if (r.kind(e) == "disposition_turned") {
            const P k{r.one(e, "race_a"), r.one(e, "race_b")};
            if (band.count(k) == 0U) continue;
            const std::int64_t old_b = band[k], new_b = r.value(e, "band");
            if (r.value(e, "change") != p504a_f::turn_change(old_b, new_b)) {
                out.push_back("7.1: e" + s(e) + " change " + s(r.value(e, "change")));
            }
            if (const auto it = last.find(k); it != last.end() && r.year(e) - r.year(it->second) < t.turn_cooldown_years) {
                const std::vector<std::int64_t> fallback{new_b > old_b ? met[k] : it->second};
                if (x.cause_list(e) == fallback) {
                    out.push_back("7.2: e" + s(e) + " turns " + s(r.year(e) - r.year(it->second)) + " years after e" +
                                  s(it->second) + " on no fresh cause");
                }
            }
            band[k] = new_b;
            last[k] = e;
            if (new_b == p504a_f::HATRED) hatred[k] = r.year(e);
        } else {
            const std::int64_t years = r.value(e, "years");
            const std::int64_t q = x.civ_race(r.one(e, "civilization")), g = r.one(e, "race");
            const P k{std::min(q, g), std::max(q, g)};
            const auto h = hatred.find(k);
            if (!p504a_f::expel_allowed(r.year(e), r.year(e) - years,
                                        h == hatred.end() ? std::nullopt : std::optional<std::int64_t>(h->second),
                                        t.expel_settled_years, t.expel_fresh_years)) {
                out.push_back("8: e" + s(e) + " expels a people held " + s(years) + " years without a fresh hatred");
            }
        }
    }
    return out;
}

// 16 item 19 (figures): towers apart, ended towers' events, books not after their tower's end, hero_rose's quiet.
// books: the rows of table book (id, title, author, site, year, event); width: the map width.
inline std::vector<std::string> figure_violations(const World& w, const std::vector<std::vector<worldgen::db::ColumnValue>>& books,
                                                  std::int64_t width, const Tuning& t) {
    std::vector<std::string> out;
    if (!has_value_key(w, "hero_rose", "quiet")) return {"0.28.0: hero_rose has no quiet"};
    const Rows r(w);
    const p503check::Index& x = r.x();
    const std::int64_t D = p504a_f::tower_spacing(t.tower_spacing_permille, width);
    std::vector<const Row*> towers;
    for (const Row& site : w.rows.t.site) {
        if (at(site, 2) == 4) towers.push_back(&site);
    }
    std::sort(towers.begin(), towers.end(), [](const Row* a, const Row* b) { return at(*a, 0) < at(*b, 0); });
    std::map<std::int64_t, std::int64_t> ended;
    for (std::size_t i = 0; i < towers.size(); ++i) {
        const Row& t4 = *towers[i];
        const std::int64_t id = at(t4, 0), c = at(t4, 7);
        ended[id] = is_null(t4, 11) ? -1 : at(t4, 11);
        if (at(t4, 6) == 0 && D > 0) {
            for (std::size_t j = 0; j < i; ++j) {
                const Row& q = *towers[j];
                if (at(q, 7) == c && p504a_f::chebyshev(at(t4, 4), at(t4, 5), at(q, 4), at(q, 5)) <= D) {
                    out.push_back("19: tower si" + s(id) + " within " + s(D) + " of si" + s(at(q, 0)));
                }
            }
        }
        if (is_null(t4, 11)) {
            if (at(t4, 13) != 0) out.push_back("19: open tower si" + s(id) + " with an ending");
            continue;
        }
        const std::int64_t ee = at(t4, 12), ending = at(t4, 13);
        // P5-08 12.6 (generator 0.30.0): ending 7, the lapse of the line of masters, is written with a tower_abandoned
        if ((ending != 4 && ending != 5 && ending != 7) || x.ev.count(ee) == 0U || r.year(ee) != at(t4, 11) ||
            (ending == 7 && r.kind(ee) != "tower_abandoned")) {
            out.push_back("19: tower si" + s(id) + " ending " + s(ending) + " e" + s(ee));
            continue;
        }
        if (ending == 7) continue;   // its quiet years after the master's death: the P5-08 reference's check 13
        if (ending == 5) {
            if (x.civ.count(c) == 0U || at(*x.civ.at(c), 9) != ee) out.push_back("19: tower si" + s(id) + " ending 5 not by its realm's end");
            continue;
        }
        bool ok = false;
        const std::int64_t R = at(t4, 3), st = at(t4, 6);
        for (const Row& rc : w.rows.t.region_control) {
            ok = ok || (at(rc, 0) == R && at(rc, 4) == ee) ||
                 (st == 0 && at(rc, 0) == R && at(rc, 3) == c && !is_null(rc, 2) && at(rc, 2) == at(t4, 11));
        }
        if (st != 0) {
            const auto sit = x.stl.find(st);
            ok = ok || (sit != x.stl.end() && at(*sit->second, 13) == ee) || at(*x.ev.at(ee), 6) == st;
            if (const auto ps = x.parts.find(ee); ps != x.parts.end()) {
                for (const Row* p : ps->second) ok = ok || (at(*p, 3) == 4 && at(*p, 4) == st);
            }
        }
        if (!ok) out.push_back("19: tower si" + s(id) + " ending 4 by e" + s(ee) + ", not 21.4's event");
    }
    for (const auto& b : books) {
        const auto it = ended.find(b.at(3).integer);
        if (it != ended.end() && it->second >= 0 && b.at(4).integer > it->second) {
            out.push_back("19: book " + s(b.at(0).integer) + " after its tower ended");
        }
    }
    std::map<std::int64_t, std::vector<std::int64_t>> raids;
    for (const std::int64_t e : r.of("monster_raid")) raids[r.one(e, "monster")].push_back(e);
    for (const std::int64_t h : r.of("hero_rose")) {
        std::vector<std::int64_t> cs;
        for (const std::int64_t c : x.cause_list(h)) {
            if (r.kind(c) == "monster_raid") cs.push_back(c);
        }
        if (cs.size() != 1U) continue;
        std::int64_t prev = 0;
        for (const std::int64_t e : raids[r.one(h, "monster")]) {
            if (e < cs[0]) prev = std::max(prev, e);
        }
        const std::int64_t g = prev == 0 ? 0 : r.year(cs[0]) - r.year(prev);
        if (r.value(h, "quiet") != p504a_f::hero_quiet(g, t.hero_return_years)) {
            out.push_back("19: hero_rose e" + s(h) + " quiet " + s(r.value(h, "quiet")) + ", g " + s(g));
        }
    }
    return out;
}

// The kinds and values of 0.28.0 seen in a world (for the coverage checks of the fixture runs).
inline std::map<std::string, std::set<std::int64_t>> value_coverage(const World& w) {
    const Rows r(w);
    std::map<std::string, std::set<std::int64_t>> out;
    for (const auto& [kind, value] : std::vector<std::pair<std::string, std::string>>{
             {"ally_refused", "motive"}, {"alliance_betrayed", "motive"}, {"spoils_withheld", "grievance"},
             {"alliance_spurned", "reason"}, {"disposition_turned", "change"}}) {
        for (const std::int64_t e : r.of(kind)) out[kind + "." + value].insert(r.value(e, value));
    }
    return out;
}

}  // namespace p504a_check
