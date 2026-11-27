#pragma once

// The invariants of spec P5-06 r3 13 that rows show, on the rows of a world file of schema 12, as the reference's
// invariant_violations (reference/p5_06_figures_reference.py): items 1 (one ruler at a time, reigns in order), 2
// (a recorded ruler at E and a first ruler), 3 (a ruler at every acting event when name_rulers), 4 (born <=
// office_from <= the start event's year), 5 (battle_led's generals serve their side), 6 (heroes alive at their deeds,
// one at a time), 7 (disputes follow crises), 9 (towers and books), 12 (regnal numbers and names), 14 (no ruin fades)
// and 16 (towers' places). Item 8 is the engine's W9 (checked by the reference on quiet steps), 10 is I0, 11 the
// earlier stages' checks, 13 the reference's (it needs the reign tallies), 15 the report's. Messages name the item.
// No library name beyond the 0.26.0 tests'.

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p3_03_chronology_support.hpp"
#include "worldgen/core/roman.hpp"

namespace p506check {

namespace db = worldgen::db;
using Rows = std::vector<std::vector<db::ColumnValue>>;

inline std::int64_t I(const std::vector<db::ColumnValue>& r, std::size_t i) { return r.at(i).integer; }
inline bool is_null(const std::vector<db::ColumnValue>& r, std::size_t i) {
    return r.at(i).type == db::ValueType::null;
}

struct Fig {
    std::int64_t id = 0, role = 0, race = 0, culture = 0, civ = 0, born = 0, office_from = 0, start_event = 0,
                 died = -1, death_event = 0, status = 0, reign_no = 0, regnal = 0;
    std::string name, epithet;
};
struct Rel {
    std::int64_t a_type = 0, a = 0, b_type = 0, b = 0, since = 0, since_event = 0, until_event = 0;
    std::string kind;
};
struct Event {
    std::int64_t id = 0, year = 0, kind = 0, phase = 0, region = 0;
};

struct World {
    std::int64_t E = 0;
    std::map<std::int64_t, std::string> kind_key;                          // event kind id -> key
    std::map<std::pair<std::int64_t, std::int64_t>, std::string> role_name; // (kind, seq) -> role
    std::map<std::pair<std::int64_t, std::int64_t>, std::string> value_key;
    std::map<std::int64_t, Event> events;
    std::map<std::int64_t, std::vector<std::pair<std::string, std::int64_t>>> parts;   // event -> (role, id)
    std::map<std::int64_t, std::map<std::string, std::int64_t>> values;
    std::map<std::int64_t, std::vector<std::int64_t>> causes;
    std::map<std::int64_t, Fig> figs;
    std::map<std::int64_t, std::vector<db::ColumnValue>> civs, sites, settlements;
    std::map<std::int64_t, std::int64_t> lifespan;   // race -> lifespan_years
    Rows census, books;
    std::vector<Rel> rels;

    [[nodiscard]] std::string kind(std::int64_t e) const {
        const auto it = events.find(e);
        if (it == events.end()) return "";
        const auto k = kind_key.find(it->second.kind);
        return k == kind_key.end() ? "" : k->second;
    }
    [[nodiscard]] std::vector<std::int64_t> of(const std::string& key) const {
        std::vector<std::int64_t> out;
        for (const auto& [id, e] : events) {
            const auto k = kind_key.find(e.kind);
            if (k != kind_key.end() && k->second == key) out.push_back(id);
        }
        return out;
    }
    [[nodiscard]] std::vector<std::int64_t> role(std::int64_t e, const std::string& r) const {
        std::vector<std::int64_t> out;
        const auto it = parts.find(e);
        if (it == parts.end()) return out;
        for (const auto& [n, x] : it->second) {
            if (n == r) out.push_back(x);
        }
        return out;
    }
    [[nodiscard]] std::int64_t one(std::int64_t e, const std::string& r) const {
        const auto v = role(e, r);
        return v.empty() ? 0 : v.front();
    }
    [[nodiscard]] std::int64_t value(std::int64_t e, const std::string& k) const {
        const auto it = values.find(e);
        if (it == values.end()) return -1;
        const auto v = it->second.find(k);
        return v == it->second.end() ? -1 : v->second;
    }
    [[nodiscard]] std::int64_t year(std::int64_t e) const {
        const auto it = events.find(e);
        return it == events.end() ? -1 : it->second.year;
    }
    [[nodiscard]] std::int64_t ending_event(std::int64_t c) const {
        const auto it = civs.find(c);
        return it == civs.end() ? 0 : I(it->second, 9);
    }
};

inline World world_of_file(const db::WorldReader& r) {
    World w;
    w.E = std::stoll(r.meta("history_end_year").value_or("0"));
    for (const auto& k : chrono_test::rows_of(r, "event_kind")) w.kind_key[I(k, 0)] = k.at(1).bytes;
    for (const auto& k : chrono_test::rows_of(r, "event_kind_role")) w.role_name[{I(k, 0), I(k, 1)}] = k.at(2).bytes;
    for (const auto& k : chrono_test::rows_of(r, "event_kind_value")) w.value_key[{I(k, 0), I(k, 1)}] = k.at(2).bytes;
    for (const auto& e : chrono_test::rows_of(r, "event")) {
        w.events[I(e, 0)] = Event{I(e, 0), I(e, 1), I(e, 3), I(e, 4), I(e, 5)};
    }
    for (const auto& p : chrono_test::rows_of(r, "event_participant")) {
        const std::int64_t k = w.events.count(I(p, 0)) != 0U ? w.events[I(p, 0)].kind : 0;
        w.parts[I(p, 0)].emplace_back(w.role_name[{k, I(p, 2)}], I(p, 4));
    }
    for (const auto& v : chrono_test::rows_of(r, "event_value")) {
        const std::int64_t k = w.events.count(I(v, 0)) != 0U ? w.events[I(v, 0)].kind : 0;
        w.values[I(v, 0)][w.value_key[{k, I(v, 1)}]] = I(v, 2);
    }
    for (const auto& c : chrono_test::rows_of(r, "event_cause")) w.causes[I(c, 0)].push_back(I(c, 1));
    for (const auto& f : chrono_test::rows_of(r, "figure")) {
        Fig x;
        x.id = I(f, 0);
        x.name = f.at(1).bytes;
        x.epithet = f.at(2).bytes;
        x.role = I(f, 3);
        x.race = I(f, 4);
        x.culture = I(f, 5);
        x.civ = I(f, 6);
        x.born = I(f, 7);
        x.office_from = I(f, 8);
        x.start_event = I(f, 9);
        x.died = is_null(f, 10) ? -1 : I(f, 10);
        x.death_event = I(f, 11);
        x.status = I(f, 14);
        x.reign_no = I(f, 15);
        x.regnal = I(f, 16);
        w.figs[x.id] = x;
    }
    for (const auto& c : chrono_test::rows_of(r, "civilization")) w.civs[I(c, 0)] = c;
    for (const auto& s : chrono_test::rows_of(r, "site")) w.sites[I(s, 0)] = s;
    for (const auto& s : chrono_test::rows_of(r, "settlement")) w.settlements[I(s, 0)] = s;
    for (const auto& x : chrono_test::rows_of(r, "race")) w.lifespan[I(x, 0)] = I(x, 6);
    w.census = chrono_test::rows_of(r, "settlement_census");
    w.books = chrono_test::rows_of(r, "book");
    for (const auto& x : chrono_test::rows_of(r, "relation")) {
        const std::string& k = x.at(4).bytes;
        if (k != "ruled" && k != "served" && k != "rival" && k != "parent") continue;
        w.rels.push_back(Rel{I(x, 0), I(x, 1), I(x, 2), I(x, 3), I(x, 5), I(x, 7), I(x, 8), k});
    }
    return w;
}

inline bool in_force(const Rel& r, std::int64_t e) { return r.since_event <= e && (r.until_event == 0 || e <= r.until_event); }

inline std::vector<std::string> violations(const World& w, bool name_rulers = true) {
    std::vector<std::string> out;
    const auto s = [](std::int64_t v) { return std::to_string(v); };
    std::vector<const Rel*> ruled, served;
    for (const Rel& r : w.rels) {
        if (r.kind == "ruled") ruled.push_back(&r);
        if (r.kind == "served") served.push_back(&r);
    }
    // 1
    std::map<std::int64_t, std::vector<const Rel*>> by_civ;
    for (const Rel* r : ruled) by_civ[r->b].push_back(r);
    for (auto& [c, rs] : by_civ) {
        std::sort(rs.begin(), rs.end(), [](const Rel* a, const Rel* b) { return a->since_event < b->since_event; });
        for (std::size_t i = 1; i < rs.size(); ++i) {
            if (rs[i - 1]->until_event == 0 || rs[i]->since_event <= rs[i - 1]->until_event) {
                out.push_back("1: two rulers of c" + s(c) + " at e" + s(rs[i]->since_event));
            }
        }
    }
    std::map<std::int64_t, const Fig*> last_ruler;
    for (const auto& [id, f] : w.figs) {
        if (f.role != 1) continue;
        const auto it = last_ruler.find(f.civ);
        if (it != last_ruler.end()) {
            const Fig& a = *it->second;
            const std::int64_t end_a = a.died >= 0 ? a.died : w.E;
            if (f.reign_no <= a.reign_no) out.push_back("1: c" + s(f.civ) + " reigns out of order f" + s(a.id));
            if (end_a > f.office_from) out.push_back("1: c" + s(f.civ) + " reign of f" + s(a.id) + " overlaps f" + s(id));
        }
        last_ruler[f.civ] = &f;
    }
    // 2, 3
    if (name_rulers) {
        for (const auto& [c, row] : w.civs) {
            const bool alive = is_null(row, 8);
            bool open = false;
            for (const Rel* r : ruled) open = open || (r->b == c && r->until_event == 0);
            if (alive && !open) out.push_back("2: alive c" + s(c) + " has no open ruled row at E");
            bool first = false;
            for (const auto& [id, f] : w.figs) {
                if (f.role == 1 && f.civ == c && f.reign_no == 1) {
                    first = true;
                    const std::string k = w.kind(f.start_event);
                    if (!(k == "came_to_rule" || (k == "succession_disputed" && w.value(f.start_event, "outcome") == 1))) {
                        out.push_back("2: c" + s(c) + " first ruler f" + s(id) + " start " + k);
                    }
                }
            }
            if (!first) out.push_back("2: c" + s(c) + " has no recorded first ruler");
        }
        const std::vector<std::pair<std::string, std::string>> acting = {
            {"war_declared", "attacker"},      {"alliance_betrayed", "betrayer"}, {"spoils_shared", "principal"},
            {"spoils_withheld", "principal"}, {"civ_conquered", "civilization"}, {"civ_ended", "civilization"}};
        for (const auto& [k, role] : acting) {
            for (const std::int64_t e : w.of(k)) {
                const std::int64_t c = w.one(e, role);
                int n = 0;
                for (const Rel* r : ruled) n += (r->b == c && in_force(*r, e)) ? 1 : 0;
                if (n != 1) out.push_back("3: " + k + " e" + s(e) + ": c" + s(c) + " has " + s(n) + " rulers");
            }
        }
    }
    // 4
    for (const auto& [id, f] : w.figs) {
        if (!(f.born <= f.office_from && f.office_from <= w.year(f.start_event))) {
            out.push_back("4: f" + s(id) + " born " + s(f.born) + " office_from " + s(f.office_from));
        }
    }
    // 5
    for (const std::int64_t b : w.of("battle_led")) {
        std::vector<std::pair<std::int64_t, std::int64_t>> gens;
        for (const std::int64_t g : w.role(b, "winner_general")) gens.emplace_back(g, w.one(b, "winner"));
        for (const std::int64_t g : w.role(b, "loser_general")) gens.emplace_back(g, w.one(b, "loser"));
        if (gens.empty()) out.push_back("5: battle_led e" + s(b) + " without a general");
        for (const auto& [g, side] : gens) {
            const auto it = w.figs.find(g);
            if (it == w.figs.end() || it->second.civ != side) out.push_back("5: e" + s(b) + " general of another side");
            bool ok = false;
            for (const Rel* r : served) ok = ok || (r->a == g && r->b == side && in_force(*r, b));
            if (!ok) out.push_back("5: e" + s(b) + " general f" + s(g) + " has no served row in force");
        }
    }
    // 6
    for (const auto& [k, fr, cr] : std::vector<std::array<std::string, 3>>{
             {"monster_slain_by_hero", "hero", "slayer"}, {"hero_slain", "figure", "civilization"}}) {
        for (const std::int64_t e : w.of(k)) {
            const auto it = w.figs.find(w.one(e, fr));
            if (it == w.figs.end() || (it->second.role != 4 && it->second.role != 6) || it->second.civ != w.one(e, cr) ||   // P5-08: a champion (role 6) hunts and may fall as a hero does
                it->second.start_event > e || (it->second.death_event != 0 && it->second.death_event < e)) {
                out.push_back("6: " + k + " e" + s(e) + " names no living hero of its realm");
            }
        }
    }
    std::map<std::int64_t, std::vector<std::pair<std::int64_t, std::int64_t>>> heroes;
    for (const auto& [id, f] : w.figs) {
        if (f.role == 4) heroes[f.civ].emplace_back(f.start_event, f.death_event != 0 ? f.death_event : INT64_MAX);
    }
    for (auto& [c, hs] : heroes) {
        std::sort(hs.begin(), hs.end());
        for (std::size_t i = 1; i < hs.size(); ++i) {
            if (hs[i].first < hs[i - 1].second) out.push_back("6: c" + s(c) + " has two living heroes");
        }
    }
    // 7
    std::set<std::int64_t> seceded;
    for (const std::int64_t e : w.of("civ_seceded")) {
        const auto it = w.causes.find(e);
        if (it == w.causes.end()) continue;
        for (const std::int64_t x : it->second) {
            if (w.kind(x) == "succession_crisis") seceded.insert(x);
        }
    }
    for (const std::int64_t e : w.of("succession_disputed")) {
        std::int64_t cr = 0;
        const auto it = w.causes.find(e);
        if (it != w.causes.end()) {
            for (const std::int64_t x : it->second) {
                if (w.kind(x) == "succession_crisis") cr = x;
            }
        }
        if (cr == 0 || w.one(cr, "civilization") != w.one(e, "civilization") || w.year(cr) != w.year(e)) {
            out.push_back("7: succession_disputed e" + s(e) + " follows no crisis");
            continue;
        }
        const bool split = w.value(e, "outcome") == 1;
        if (split != (seceded.count(cr) != 0U)) out.push_back("7: succession_disputed e" + s(e) + " outcome");
        if (split) {
            const auto c = w.figs.find(w.one(e, "claimant"));
            if (c == w.figs.end() || c->second.role != 1 || c->second.reign_no != 1 ||
                c->second.civ != w.one(e, "new_realm")) {
                out.push_back("7: e" + s(e) + " claimant is not the new realm's first ruler");
            }
        }
    }
    // 9
    std::map<std::int64_t, int> books_of, blights_of;
    for (const auto& [sid, row] : w.sites) {
        if (I(row, 2) != 4) continue;
        const std::int64_t oe = I(row, 10);
        const auto f = w.figs.find(w.one(oe, "figure"));
        if (w.kind(oe) != "tower_founded" || f == w.figs.end() || f->second.role != 5 || f->second.civ != I(row, 7)) {
            out.push_back("9: tower si" + s(sid) + " is not a site of its caster's realm");
        }
    }
    // P5-08 12.6: a caster who took a tower up (tower_taken_up) writes there too
    std::set<std::pair<std::int64_t, std::int64_t>> taken_up;
    for (const std::int64_t e : w.of("tower_taken_up")) taken_up.emplace(w.one(e, "figure"), w.one(e, "site"));
    for (const auto& b : w.books) {
        ++books_of[I(b, 2)];
        const auto st = w.sites.find(I(b, 3));
        if (st == w.sites.end() || I(st->second, 2) != 4 ||
            (w.one(I(st->second, 10), "figure") != I(b, 2) && taken_up.count({I(b, 2), I(b, 3)}) == 0U)) {
            out.push_back("9: book " + s(I(b, 0)) + " is not at its author's tower");
        }
    }
    for (const std::int64_t e : w.of("blight")) {
        if (++blights_of[w.one(e, "figure")] > 1) out.push_back("9: a second blight e" + s(e));
    }
    // 12
    std::map<std::string, std::vector<const Fig*>> names;
    for (const auto& [id, f] : w.figs) names[f.name].push_back(&f);
    for (const auto& [n, fs] : names) {
        std::set<std::int64_t> civs_of;
        bool non_ruler = false;
        for (const Fig* f : fs) {
            civs_of.insert(f->civ);
            non_ruler = non_ruler || f->role != 1;
        }
        if (fs.size() > 1 && (civs_of.size() > 1 || non_ruler)) out.push_back("12: name " + n + " shared");
        for (std::size_t i = 0; i < fs.size(); ++i) {
            const std::int64_t want = i == 0 ? 0 : static_cast<std::int64_t>(i) + 1;
            if (fs[i]->regnal != want) out.push_back("12: f" + s(fs[i]->id) + " regnal " + s(fs[i]->regnal));
            if (fs[i]->regnal >= 2 && fs[i]->reign_no == 1) out.push_back("12: a first ruler with a number");
        }
    }
    std::set<std::string> display;
    for (const auto& [id, f] : w.figs) {
        std::string d = f.name;
        if (f.regnal >= 2) d += " " + worldgen::roman_numeral(f.regnal);
        if (!f.epithet.empty()) d += " " + f.epithet;
        if (!display.insert(d).second) out.push_back("12: display name " + d + " twice");
    }
    // 14
    for (const auto& [sid, row] : w.settlements) {
        if (I(row, 10) == 4) out.push_back("14: settlement " + s(sid) + " faded");
    }
    for (const std::int64_t e : w.of("ruin_faded")) out.push_back("14: ruin_faded e" + s(e));
    std::map<std::int64_t, std::int64_t> first_ruin;
    for (const auto& c : w.census) {
        if ((I(c, 4) == 3 || I(c, 4) == 4) && first_ruin.count(I(c, 0)) == 0U) first_ruin[I(c, 0)] = I(c, 1);
    }
    for (const auto& c : w.census) {
        const auto it = first_ruin.find(I(c, 0));
        if (it != first_ruin.end() && I(c, 1) > it->second && I(c, 4) != 3) {
            out.push_back("14: settlement " + s(I(c, 0)) + " census status after its ruin");
        }
    }
    for (const auto& [sid, y] : first_ruin) {
        const auto& row = w.settlements.at(sid);
        if (I(row, 10) != 3 || is_null(row, 12) || I(row, 13) == 0 || I(row, 14) < 1) {
            out.push_back("14: settlement " + s(sid) + " ruined in " + s(y) + " is not ruined at E");
        }
    }
    // 16
    // Two towers share a cell only when the earlier one has ended by the later one's opening (P5-04a 21.4: an ended
    // tower's cell is free again, and another realm's tower does not repel; at 0.27.0 no tower ends).
    std::map<std::int64_t, std::int64_t> at_settlement;
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> cells;   // cell -> ended year of its last tower
    constexpr std::int64_t OPEN = std::numeric_limits<std::int64_t>::max();
    for (const auto& [sid, row] : w.sites) {
        if (I(row, 2) != 4) continue;
        if (I(row, 6) != 0 && I(row, 13) == 0 && !at_settlement.emplace(I(row, 6), sid).second) {
            out.push_back("16: two towers at settlement " + s(I(row, 6)));
        }
        const std::pair<std::int64_t, std::int64_t> cell{I(row, 4), I(row, 5)};
        const auto prev = cells.find(cell);
        if (prev != cells.end() && prev->second > I(row, 9)) out.push_back("16: two towers share a cell");
        cells[cell] = is_null(row, 11) ? OPEN : I(row, 11);
        const auto e = w.events.find(I(row, 10));
        if (e == w.events.end() || e->second.region != I(row, 3)) out.push_back("16: tower si" + s(sid) + " region");
    }
    return out;
}

// Counts by kind key of 79 to 92 (for messages and the every-kind test).
inline std::map<std::string, std::int64_t> figure_kind_counts(const World& w) {
    static const std::vector<std::string> keys = {
        "came_to_rule", "figure_died", "succession_disputed", "exiled", "reigned_at_end", "general_appointed",
        "battle_led", "fell_in_battle", "hero_rose", "monster_slain_by_hero", "hero_slain", "tower_founded",
        "book_written", "blight"};
    std::map<std::string, std::int64_t> out;
    for (const std::string& k : keys) out[k] = static_cast<std::int64_t>(w.of(k).size());
    return out;
}

}  // namespace p506check
