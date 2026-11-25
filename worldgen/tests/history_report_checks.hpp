#pragma once

// The invariants of spec P3-07 section 13 as restated by r17 24.10, as checks on the two report texts of one
// world; the war of an event and the war frames of r17 24.1 computed here from the stored tables (test-side,
// for the orphan count of 24.10 item 1); and the library calls of section 5.1 the P3-07 tests use.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "history_report_fixture.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/inspect/history_report.hpp"
#include "worldgen/inspect/report.hpp"

namespace hrtest {

using worldgen::inspect::HistoryLevel;

// build_history_report at `level`, REQUIREd to succeed.
inline std::string report_of(const db::WorldReader& r, HistoryLevel level) {
    auto rep = worldgen::inspect::build_history_report(r, level);
    REQUIRE_MESSAGE(rep.ok(), "build_history_report failed: " << (rep.ok() ? "" : rep.error().message));
    return std::move(rep).value();
}
inline std::string report_of(const std::string& path, HistoryLevel level) {
    const db::WorldReader r = open_world(path);
    return report_of(r, level);
}

inline std::string sha_hex(std::string_view text) {
    return worldgen::sha256_hex(std::vector<std::uint8_t>(text.begin(), text.end()));
}

// The escaping of 6.1, written here from the spec (not the library's function), for the expected lines.
inline std::string esc(std::string_view s) {
    static const char* const hex = "0123456789ABCDEF";
    std::string out;
    for (const char ch : s) {
        const auto b = static_cast<unsigned char>(ch);
        if (b == 0x5C) {
            out += "\\\\";
        } else if (b < 0x20 || b >= 0x7F) {
            out += "\\x";
            out += hex[b >> 4];
            out += hex[b & 15];
        } else {
            out += ch;
        }
    }
    return out;
}

// The [history] section of the world report, as key -> value text.
inline std::map<std::string, std::string> world_history_section(const db::WorldReader& r) {
    auto rep = worldgen::inspect::build_report(r);
    REQUIRE_MESSAGE(rep.ok(), "build_report failed");
    std::map<std::string, std::string> out;
    bool in = false;
    for (const std::string& l : lines_of(rep.value())) {
        if (!l.empty() && l[0] == '[') {
            in = l == "[history]";
            continue;
        }
        if (in) {
            const std::size_t eq = l.find(" = ");
            REQUIRE(eq != std::string::npos);
            out[l.substr(0, eq)] = l.substr(eq + 3);
        }
    }
    return out;
}

inline bool is_event_line(const std::string& l) { return !l.empty() && l[0] >= '0' && l[0] <= '9'; }

// The event id of a chronicle event line ("<span> e<id> ...").
inline std::int64_t event_id_of(const std::string& l) {
    const std::size_t sp = l.find(' ');
    REQUIRE(sp != std::string::npos);
    REQUIRE(l.compare(sp + 1, 1, "e") == 0);
    return std::stoll(l.substr(sp + 2));
}

// --- r17 24.1: the war of an event, the frames, the summary rules of 24.10 item 1 -------------------

class Wars {
public:
    explicit Wars(const db::WorldReader& r) : view_(load(r)) {
        const auto& cat = view_.catalogue();
        auto ids = [&](std::initializer_list<const char*> keys) {
            std::set<std::int64_t> out;
            for (const char* k : keys) {
                if (const auto* kd = cat.find(k)) out.insert(kd->id);
            }
            return out;
        };
        // P5-04 11.1: allied_battle is a battle kind wherever P3-07 names battle and battle_at_pass
        W_ = ids({"war_declared", "battle", "battle_at_pass", "allied_battle", "siege", "sacked", "settlement_captured", "refuge_held",
                  "peace", "war_won", "truce"});
        EW_ = ids({"battle", "battle_at_pass", "allied_battle", "siege", "sacked", "settlement_captured", "refuge_held", "peace",
                   "war_won", "truce", "search_abandoned", "region_ceded", "settlements_ceded", "tribute",
                   "capital_moved", "refugees_settled", "civ_conquered"});
        battles_ = ids({"battle", "battle_at_pass", "allied_battle"});
        renamed_ = ids({"region_renamed"});
        falls_ = ids({"sacked", "settlement_captured"});
        conseq_ = ids({"capital_moved", "refugees_settled", "civ_conquered"});
        claims_ = ids({"claim", "claim_settlements"});
        spread_ = ids({"plague_spread"});
        quiet_ = ids({"quiet_stretch"});
        const auto* decl = cat.find("war_declared");
        const auto* search = cat.find("search_abandoned");
        const auto* joined = cat.find("ally_joined_war");
        // the war rows: since_event -> (closed, until_event); P5-04 10.4: an ally's joined row is not a war
        for (const auto& row : rows_of(r, "relation")) {
            if (row[4].bytes != "war") continue;
            if (joined != nullptr && view_.event(row[7].integer) != nullptr &&
                view_.event(row[7].integer)->row.kind == joined->id) {
                continue;
            }
            const std::int64_t since_event = row[7].integer, until_event = row[8].integer;
            const bool closed = row[6].type != db::ValueType::null;
            rows_[since_event] = closed ? until_event : 0;
            if (closed) {
                auto it = ended_by_.find(until_event);
                if (it == ended_by_.end() || since_event < it->second) ended_by_[until_event] = since_event;
            }
        }
        for (const auto& e : view_.events()) {
            const std::int64_t id = e.row.id, k = e.row.kind;
            std::optional<std::int64_t> w;
            if (decl != nullptr && k == decl->id) {
                w = id;
            } else if (EW_.count(k) != 0U) {
                std::optional<std::int64_t> c;
                for (const std::int64_t x : e.causes) {
                    if (W_.count(kind(x)) != 0U && (!c || x < *c)) c = x;
                }
                if (c) {
                    w = war_.count(*c) != 0U ? war_.at(*c) : std::optional<std::int64_t>();
                } else if (ended_by_.count(id) != 0U) {
                    w = ended_by_.at(id);
                }
            }
            if (w) war_[id] = *w;
            for (const std::int64_t x : e.causes) kids_[x].push_back(id);
            if (battles_.count(k) != 0U && w) last_battle_[*w] = std::max(last_battle_[*w], id);
        }
        for (const auto& [d, u] : rows_) {
            if (u != 0 && search != nullptr && kind(u) == search->id &&
                view_.event(u)->row.salience < 20) {
                excluded_.insert(d);
            }
        }
    }
    [[nodiscard]] std::optional<std::int64_t> war(std::int64_t id) const {
        const auto it = war_.find(id);
        return it == war_.end() ? std::nullopt : std::optional<std::int64_t>(it->second);
    }
    [[nodiscard]] std::optional<std::int64_t> last_battle(std::int64_t d) const {
        const auto it = last_battle_.find(d);
        return it == last_battle_.end() ? std::nullopt : std::optional<std::int64_t>(it->second);
    }
    [[nodiscard]] bool excluded(std::int64_t d) const { return excluded_.count(d) != 0U; }
    [[nodiscard]] std::int64_t kind(std::int64_t id) const { return view_.event(id)->row.kind; }
    [[nodiscard]] const worldgen::history::HistoryView& view() const { return view_; }

    // 24.10 item 1: shown war events whose war's declaration (not excluded by 22.5a) is not shown.
    [[nodiscard]] std::int64_t orphans(const std::set<std::int64_t>& shown) const {
        std::int64_t n = 0;
        for (const std::int64_t x : shown) {
            const auto d = war(x);
            if (EW_.count(kind(x)) != 0U && d && !excluded(*d) && shown.count(*d) == 0U) ++n;
        }
        return n;
    }
    // The other rules of 24.10 item 1 (reading R10: a sack or capture below salience 20, shown only as part 4 of
    // a consequence's unit, need not bring its consequences: 24.2 says companions are not followed further).
    [[nodiscard]] std::vector<std::string> violations(const std::set<std::int64_t>& shown) const {
        std::vector<std::string> bad;
        const auto renaming_cause = [this](std::int64_t x, const std::set<std::int64_t>& sh) {
            for (const std::int64_t y : sh) {
                const auto* e = view_.event(y);
                if (e != nullptr && renamed_.count(e->row.kind) != 0U &&
                    std::find(e->causes.begin(), e->causes.end(), x) != e->causes.end()) {
                    return true;
                }
            }
            return false;
        };
        for (const std::int64_t x : shown) {
            const std::int64_t k = kind(x);
            const auto d = war(x);
            const std::string ex = "e" + std::to_string(x) + ": ";
            if (EW_.count(k) != 0U && d && !excluded(*d)) {
                const std::int64_t u = rows_.count(*d) != 0U ? rows_.at(*d) : 0;
                if (u != 0 && shown.count(u) == 0U) bad.push_back(ex + "its war's ending is not shown");
                const auto lb = last_battle(*d);
                if (lb && shown.count(*lb) == 0U) bad.push_back(ex + "its war's last battle is not shown");
            }
            if (rows_.count(x) != 0U && rows_.at(x) != 0 && shown.count(rows_.at(x)) == 0U) {
                bad.push_back(ex + "the declaration's ending is not shown");
            }
            if (falls_.count(k) != 0U && view_.event(x)->row.salience >= 20 && kids_.count(x) != 0U) {
                for (const std::int64_t y : kids_.at(x)) {
                    if (conseq_.count(kind(y)) != 0U && shown.count(y) == 0U) bad.push_back(ex + "a consequence is not shown");
                }
            }
            if (conseq_.count(k) != 0U) {
                for (const std::int64_t y : view_.event(x)->causes) {
                    if (falls_.count(kind(y)) != 0U && shown.count(y) == 0U) bad.push_back(ex + "its sack is not shown");
                }
            }
            if (claims_.count(k) != 0U || spread_.count(k) != 0U) bad.push_back(ex + "a claim or plague_spread");
            if (battles_.count(k) != 0U && (!d || last_battle(*d) != x) && !renaming_cause(x, shown)) {
                bad.push_back(ex + "not its war's last battle");   // r21 26.5 item 1: or a shown renaming's cause
            }
        }
        for (const auto& e : view_.events()) {
            if (quiet_.count(e.row.kind) != 0U && shown.count(e.row.id) == 0U) {
                bad.push_back("e" + std::to_string(e.row.id) + ": quiet stretch not shown");
            }
        }
        return bad;
    }

private:
    static worldgen::history::HistoryView load(const db::WorldReader& r) {
        auto v = worldgen::history::HistoryView::load(r);
        REQUIRE(v.ok());
        return std::move(v).value();
    }
    static std::vector<std::vector<db::ColumnValue>> rows_of(const db::WorldReader& r, std::string_view t) {
        auto rows = r.table_rows(t);
        REQUIRE(rows.ok());
        return std::move(rows).value();
    }
    worldgen::history::HistoryView view_;
    std::set<std::int64_t> W_, EW_, battles_, renamed_, falls_, conseq_, claims_, spread_, quiet_, excluded_;
    std::map<std::int64_t, std::int64_t> rows_, ended_by_, war_, last_battle_;
    std::map<std::int64_t, std::vector<std::int64_t>> kids_;
};

// The event ids of the chronicle's event lines.
inline std::set<std::int64_t> shown_ids(const std::string& report) {
    std::set<std::int64_t> out;
    for (const std::string& l : block(report, "[chronicle]")) {
        if (is_event_line(l)) out.insert(event_id_of(l));
    }
    return out;
}

// The lines of [civilizations] of the text without the lines that differ between the levels (24.10 item 3, as
// restated by r19 25.12 item 2): the relation lines (full) and the adversary lines (summary); and at full, for a
// block whose fate line says the realm ended, the lines r19 25.5 leaves out at summary (home, the two peak lines,
// events). An ended block's milestones are compared only where both levels print them (25.5: at most four at
// summary), so the milestone lines of ended blocks are left out here and checked by check_invariants.
// P5-03a 9.2 (format v6): at summary the milestones are `  milestones: <n>` and the quiet-stretch lines only, so
// the milestone lines are left out at both levels and the head line is read as `  milestones:`; v6_milestones
// checks n and the quiet-stretch lines against full.
inline std::vector<std::string> civ_common(const std::string& text, bool full) {
    std::vector<std::string> out;
    std::vector<std::string> blk;
    auto flush = [&]() {
        bool ended = false;
        for (const std::string& l : blk) ended = ended || l.rfind("  fate: ended in ", 0) == 0;
        bool in_milestones = false;
        for (const std::string& l : blk) {
            if (l == "  milestones:" || l.rfind("  milestones: ", 0) == 0) {
                in_milestones = true;
                out.push_back("  milestones:");
                continue;
            }
            if (in_milestones && l.rfind("    ", 0) == 0) continue;
            if (l.rfind("  allies: ", 0) == 0) {   // P5-04 10.3: the summary's "; at the end: ..." suffix
                out.push_back(l.substr(0, l.find("; at the end: ")));
                continue;
            }
            if (full && ended && (l.rfind("  home: ", 0) == 0 || l.rfind("  peak regions: ", 0) == 0 ||
                                  l.rfind("  peak population: ", 0) == 0 || l.rfind("  events: ", 0) == 0)) {
                continue;
            }
            out.push_back(l);
        }
        blk.clear();
    };
    bool skip = false;
    for (const std::string& l : block(text, "[civilizations]")) {
        if (!l.empty() && l[0] == 'c') flush();
        if (l.rfind(full ? "  relations:" : "  adversaries:", 0) == 0) {
            skip = true;
            continue;
        }
        if (skip && l.rfind("    ", 0) == 0) continue;   // relation rows, adversary lines and `without war:`
        skip = false;
        blk.push_back(l);
    }
    flush();
    return out;
}

// P5-03a 9.2: per realm, n of `  milestones: <n>` at summary is the number of full's milestone lines, and the
// summary's milestone lines are full's of kind quiet_stretch, in order.
inline void v6_milestones(const std::string& summary, const std::string& full) {
    auto per_civ = [](const std::string& text) {
        std::map<std::string, std::pair<std::string, std::vector<std::string>>> out;
        std::string civ;
        bool in = false;
        for (const std::string& l : block(text, "[civilizations]")) {
            if (!l.empty() && l[0] == 'c') {
                civ = l.substr(0, l.find(' '));
                in = false;
                continue;
            }
            if (l.rfind("  milestones:", 0) == 0) {
                out[civ].first = l;
                in = true;
                continue;
            }
            if (in && l.rfind("    ", 0) == 0) {
                out[civ].second.push_back(l);
            } else {
                in = false;
            }
        }
        return out;
    };
    const auto s = per_civ(summary), f = per_civ(full);
    CHECK(s.size() == f.size());
    for (const auto& [civ, sv] : s) {
        CAPTURE(civ);
        const auto it = f.find(civ);
        REQUIRE(it != f.end());
        CHECK(it->second.first == "  milestones:");
        CHECK(sv.first == "  milestones: " + std::to_string(it->second.second.size()));
        std::vector<std::string> quiet;
        for (const std::string& l : it->second.second) {
            if (l.find(" quiet_stretch: ") != std::string::npos) quiet.push_back(l);
        }
        CHECK(sv.second == quiet);
    }
}

// [present] without its ruin lines and `  and <x> more` (P5-03a 9.3 cuts them at summary; v6_ruins checks them).
inline std::vector<std::string> v6_present(const std::vector<std::string>& ls) {
    std::vector<std::string> out;
    for (const std::string& l : ls) {
        const bool ruin = l.rfind("  s", 0) == 0 && l.find(": ruined ") != std::string::npos &&
                          l.find(", at ") == std::string::npos;
        const bool more = l.rfind("  and ", 0) == 0 && l.size() > 5 && l.compare(l.size() - 5, 5, " more") == 0;
        if (!ruin && !more) out.push_back(l);
    }
    return out;
}

// P5-03a 9.3: at most 15 ruins at summary are all of full's; more than 15: a subsequence of full's of 5 to 15 lines
// then `  and <x> more` with x the rest. (Which ruins: the H15 case and the reference's text.)
inline void v6_ruins(const std::string& summary, const std::string& full) {
    auto ruins = [](const std::string& text) {
        std::vector<std::string> out;
        std::vector<std::string> more;
        bool in = false;
        for (const std::string& l : block(text, "[present]")) {
            if (l.rfind("ruins: ", 0) == 0) {
                in = true;
                continue;
            }
            if (in && l.rfind("  s", 0) == 0 && l.find(": ruined ") != std::string::npos) {
                out.push_back(l);
            } else if (in && l.rfind("  and ", 0) == 0) {
                more.push_back(l);
            } else {
                in = false;
            }
        }
        return std::make_pair(out, more);
    };
    const auto [rs, ms] = ruins(summary);
    const auto [rf, mf] = ruins(full);
    CHECK(mf.empty());
    if (rf.size() <= 15U) {
        CHECK(rs == rf);
        CHECK(ms.empty());
        return;
    }
    CHECK(rs.size() >= 5U);
    CHECK(rs.size() <= 15U);
    std::vector<std::string> sub;
    for (const std::string& l : rf) {
        if (std::find(rs.begin(), rs.end(), l) != rs.end()) sub.push_back(l);
    }
    CHECK(sub == rs);
    CHECK(ms == std::vector<std::string>{"  and " + std::to_string(rf.size() - rs.size()) + " more"});
}

// r19 25.2, 25.6: the phase of each event and the ids of the calamity kinds and of flare_up, from the stored rows.
inline std::vector<std::string> v2_summary_violations(const worldgen::history::HistoryView& view,
                                                      const std::set<std::int64_t>& shown) {
    std::vector<std::string> bad;
    std::set<std::int64_t> cal;
    for (const char* k : {"famine", "plague", "plague_outbreak", "succession_crisis"}) {
        if (const auto* kd = view.catalogue().find(k)) cal.insert(kd->id);
    }
    const auto* flare = view.catalogue().find("flare_up");
    const auto* decl = view.catalogue().find("war_declared");
    std::map<std::int64_t, std::int64_t> per_phase;
    std::set<std::int64_t> war_phases;
    // r21 26.5 item 1 (part 4): a shown renaming brings its cause, also a calamity beyond 25.2's cap (25.2 says such a
    // calamity is no companion of anything; both the C++ and the reference let 26.5 rule: a spec question). Those
    // causes are not counted against the cap.
    std::set<std::int64_t> renaming_causes;
    if (const auto* rn = view.catalogue().find("region_renamed")) {
        for (const auto& e : view.events()) {
            if (e.row.kind == rn->id && shown.count(e.row.id) != 0U) {
                renaming_causes.insert(e.causes.begin(), e.causes.end());
            }
        }
    }
    for (const auto& e : view.events()) {
        if (decl != nullptr && e.row.kind == decl->id) war_phases.insert(e.row.phase);
        if (cal.count(e.row.kind) != 0U && shown.count(e.row.id) != 0U && renaming_causes.count(e.row.id) == 0U) {
            ++per_phase[e.row.phase];
        }
    }
    for (const auto& [p, n] : per_phase) {
        if (n > 2) bad.push_back("p" + std::to_string(p) + ": " + std::to_string(n) + " calamities shown (25.2)");
    }
    for (const auto& e : view.events()) {
        if (flare != nullptr && e.row.kind == flare->id && war_phases.count(e.row.phase) != 0U &&
            shown.count(e.row.id) == 0U) {
            bad.push_back("e" + std::to_string(e.row.id) + ": flare-up with a war in its phase not shown (25.6)");
        }
    }
    return bad;
}

// r19 25.7 item 4 with r21 26.5 item 2: SUMMARY_STATS, the 17 keys printed at summary (in section 9's order, that of
// full's block).
inline const std::set<std::string>& summary_stat_keys() {
    static const std::set<std::string> k = {"end_year", "events", "summary_threshold", "summary_events",
                                            "civilizations", "civilizations_alive", "civilizations_ended",
                                            "settlements_standing", "settlements_ruined", "population", "wars",
                                            "wars_open", "wars_hollow_permille", "share_max_permille", "races_alive",
                                            "regions_renamed", "refuges_hidden"};
    return k;
}

// The invariants of r17 24.10 (items 1 to 7 as far as they are checks on the text; item 7's standing claims
// are spelled out per fixture by the tests) on the summary and full texts of one world.
inline void check_invariants(const db::WorldReader& r, const std::string& summary, const std::string& full) {
    // P5-02 12.4, 12.5 (v4): [monsters] and [sites] after [present]; P5-03 17.2 (v5): [peoples] after [sites]
    const std::vector<std::string> heads_s = {"[settings]", "[ages]",     "[chronicle]", "[civilizations]",
                                              "[present]",  "[monsters]", "[sites]",     "[peoples]", "[alliances]",
                                              "[stats]"};   // P5-04 10.2
    const std::vector<std::string> heads_f = {"[settings]", "[ages]",    "[phases]", "[chronicle]", "[civilizations]",
                                              "[present]",  "[monsters]", "[sites]",  "[peoples]", "[alliances]",
                                              "[stats]"};
    for (const std::string* text : {&summary, &full}) {
        const bool is_full = text == &full;
        CAPTURE(is_full);
        bool bytes_ok = !text->empty() && text->back() == '\n';
        for (const char c : *text) {
            const auto b = static_cast<unsigned char>(c);
            bytes_ok = bytes_ok && (b == 0x0A || (b >= 0x20 && b <= 0x7E));
        }
        CHECK(bytes_ok);
        const std::vector<std::string> lines = lines_of(*text);
        REQUIRE(lines.size() >= 2U);
        // r19 25.1: the header is v2 (tested on the text: the spec names no library constant for it)
        CHECK(lines[0] == "# worldgen history v7");   // P5-02 12.1; P5-03 17.1; P5-03a 9.1
        CHECK(lines[1] == (is_full ? "level = full" : "level = summary"));
        std::vector<std::string> got_heads;
        for (const std::string& l : lines) {
            CHECK_MESSAGE(!l.empty(), "an empty line");
            if (!l.empty()) {
                CHECK_MESSAGE(l.back() != ' ', "trailing space: " << l);
                CHECK_MESSAGE((l[0] != ' ' || l.rfind("  ", 0) == 0), "one leading space: " << l);
            }
            if (!l.empty() && l.front() == '[' && l.back() == ']' && l.find(' ') == std::string::npos) {
                got_heads.push_back(l);
            }
        }
        CHECK(got_heads == (is_full ? heads_f : heads_s));  // 24.4
    }
    const auto stats = stats_of(full);   // r19 25.12 item 2: invariant 6 and the counts are read on full
    const std::vector<std::string> chron_s = block(summary, "[chronicle]");
    const std::vector<std::string> chron_f = block(full, "[chronicle]");
    const Wars wars(r);
    const auto& view = wars.view();
    const std::int64_t events = static_cast<std::int64_t>(view.events().size());
    // 24.10 item 1
    std::vector<std::int64_t> ids_f;
    for (const std::string& l : chron_f) {
        if (is_event_line(l)) ids_f.push_back(event_id_of(l));
    }
    std::vector<std::int64_t> all;
    for (std::int64_t i = 1; i <= events; ++i) all.push_back(i);
    CHECK(ids_f == all);
    CHECK(contains_line(chron_f, "threshold = 0"));
    CHECK(contains_line(chron_f, "shown = " + std::to_string(events)));
    const std::set<std::int64_t> shown = shown_ids(summary);
    CHECK(contains_line(chron_s, "shown = " + std::to_string(shown.size())));
    CHECK(contains_line(chron_s, "events = " + std::to_string(events)));
    CHECK(static_cast<std::int64_t>(shown.size()) == stats.at("summary_events"));
    CHECK(contains_line(chron_s, "threshold = " + std::to_string(stats.at("summary_threshold"))));
    if (stats.at("summary_threshold") < 100) CHECK(shown.size() <= 250U);
    CHECK(wars.orphans(shown) == 0);
    CHECK(wars.violations(shown) == std::vector<std::string>{});
    CHECK(v2_summary_violations(view, shown) == std::vector<std::string>{});   // r19 25.12 item 1
    // 24.10 item 2: shown and hidden per phase heading (both levels) and [phases] (full)
    for (const std::string* text : {&summary, &full}) {
        std::map<std::int64_t, std::int64_t> per_phase, heading;
        std::int64_t cur = 0, total = 0;
        for (const std::string& l : block(*text, "[chronicle]")) {
            if (l.rfind("-- p", 0) == 0) {
                cur = std::stoll(l.substr(4));
                heading[cur] = std::stoll(l.substr(l.rfind("; ") + 2));
                per_phase[cur] += 0;
            } else if (is_event_line(l)) {
                ++per_phase[cur];
                ++total;
            } else if (!l.empty() && l[0] == '(') {
                const std::int64_t n = std::stoll(l.substr(1));
                CHECK(n > 0);
                per_phase[cur] += n;
                total += n;
                CHECK(l == "(" + std::to_string(n) + (n == 1 ? " more event not shown)" : " more events not shown)"));
            }
        }
        CHECK(total == events);
        CHECK(per_phase == heading);
        if (text == &full) {
            for (const std::string& l : block(full, "[phases]")) {
                const std::int64_t p = std::stoll(l.substr(1));
                CHECK_MESSAGE(heading[p] == std::stoll(l.substr(l.rfind("; ") + 2)), l);
            }
        }
    }
    // 13 item 4: every chronicle sentence is render_event's, escaped
    for (const std::string& l : chron_f) {
        if (!is_event_line(l)) continue;
        const std::int64_t id = event_id_of(l);
        const std::size_t colon = l.find(": ");
        REQUIRE(colon != std::string::npos);
        const std::string s = esc(worldgen::history::render_event(view, id));
        const std::string rest = l.substr(colon + 2);
        CHECK_MESSAGE((rest.rfind(s + " {", 0) == 0 || rest.rfind(s + " (", 0) == 0), l);
    }
    // 24.10 item 3
    for (const char* b : {"[settings]", "[ages]"}) {
        CAPTURE(std::string_view(b));
        CHECK(block(summary, b) == block(full, b));
    }
    {   // r19 25.12 item 2: [stats] at summary is the SUMMARY_STATS subset of full's, with its values, in its order
        std::vector<std::string> subset;
        for (const std::string& l : block(full, "[stats]")) {
            if (summary_stat_keys().count(l.substr(0, l.find(" = "))) != 0U) subset.push_back(l);
        }
        CHECK(block(summary, "[stats]") == subset);
        CHECK(subset.size() == 17U);
    }
    CHECK(civ_common(summary, false) == civ_common(full, true));
    v6_milestones(summary, full);
    std::vector<std::string> present_f;
    for (const std::string& l : block(full, "[present]")) {
        // a [present] region line `  rg<id> <name>: ...` (full only); r21 26.2's `  rg<id>: ...` is at both levels
        const bool region_line = l.rfind("  rg", 0) == 0 && l.find(' ', 4) != std::string::npos &&
                                 l.find(' ', 4) < l.find(':');
        const bool settlement_line = l.rfind("  s", 0) == 0 && l.size() > 3 && l[3] >= '0' && l[3] <= '9' &&
                                     l.find(", at ") != std::string::npos;
        const bool beasts_line = l.rfind("beasts:", 0) == 0;   // P5-02 12.3: the world's beasts, full only
        if (!region_line && !settlement_line && !beasts_line) present_f.push_back(l);
    }
    CHECK(v6_present(present_f) == v6_present(block(summary, "[present]")));
    v6_ruins(summary, full);
    // 13 item 6: [history] of the world report is a subset of [stats]
    for (const auto& [k, v] : world_history_section(r)) {
        CAPTURE(k);
        REQUIRE(stats.count(k) == 1U);
        CHECK(std::to_string(stats.at(k)) == v);
    }
    // 13 item 7, 24.10 items 5 and 6
    std::set<std::int64_t> qs_civs;
    if (const auto* q = view.catalogue().find("quiet_stretch")) {
        for (const auto& e : view.events()) {
            if (e.row.kind != q->id) continue;
            for (const auto& p : e.participants) {
                if (p.entity_type == 3) qs_civs.insert(p.entity_id);
            }
        }
    }
    std::map<std::string, std::string> fates;
    std::map<std::string, std::vector<std::string>> civ_lines;
    std::string civ;
    for (const std::string& l : block(summary, "[civilizations]")) {
        if (!l.empty() && l[0] == 'c') {
            civ = l.substr(0, l.find(' '));
            continue;
        }
        civ_lines[civ].push_back(l);
        if (l.rfind("  fate: alive in ", 0) == 0) fates[civ] = l.substr(l.find(": ", 8) + 2);
    }
    auto numbers = [](const std::string& l) {
        std::vector<std::int64_t> out;
        std::string cur;
        for (const char ch : l + " ") {
            if (ch >= '0' && ch <= '9') {
                cur += ch;
            } else if (!cur.empty()) {
                out.push_back(std::stoll(cur));
                cur.clear();
            }
        }
        return out;
    };
    for (const auto& [id, ls] : civ_lines) {
        CAPTURE(id);
        std::int64_t af = -1, k_sum = 0, n_adv = 0, war_adv = 0, without = 0;
        bool in_adv = false;
        std::int64_t quiet_milestones = 0;
        bool quiet_noted = true;
        for (const std::string& l : ls) {
            if (l.rfind("  wars: ", 0) == 0) {
                const auto n = numbers(l);
                REQUIRE(n.size() == 6U);
                CHECK(n[2] + n[3] + n[4] + n[5] == n[0] + n[1]);
                af = n[0] + n[1];
            }
            if (l.rfind("  adversaries:", 0) == 0) {
                in_adv = true;
                n_adv = l == "  adversaries: none" ? 0 : std::stoll(l.substr(15));
                continue;
            }
            if (in_adv && l.rfind("    without war: ", 0) == 0) {   // r19 25.4: m (enmity e, none q), e + q = m
                const auto n = numbers(l);
                REQUIRE(n.size() == 3U);
                CHECK(n[1] + n[2] == n[0]);
                CHECK(n[0] >= 1);
                without = n[0];
                continue;
            }
            CHECK_MESSAGE(l.find(": no war; now ") == std::string::npos, "a v1 no-war line: " << l);
            if (in_adv && l.rfind("    ", 0) == 0) {
                ++war_adv;
                const std::string body = l.substr(l.rfind("): ") != std::string::npos ? l.rfind("): ") + 3 : l.find(": ") + 2);
                if (body.rfind("no war;", 0) != 0) {
                    const auto n = numbers(body.substr(0, body.find("; now")));
                    REQUIRE(n.size() >= 5U);
                    CHECK(n[1] + n[2] + n[3] + n[4] == n[0]);
                    k_sum += n[0];
                }
                continue;
            }
            in_adv = false;
            if (l.rfind("    ", 0) == 0 && l.find(" quiet_stretch: ") != std::string::npos) {
                ++quiet_milestones;
                quiet_noted = quiet_noted && l.find("rulers.)") != std::string::npos;
            }
        }
        CHECK(k_sum == af);
        CHECK(without == 0);                 // P5-03a 9.2: no `without war:` line at v6
        CHECK(war_adv <= n_adv);             // r19 25.12 item 3 without m
        CHECK(quiet_milestones == (qs_civs.count(std::stoll(id.substr(1))) != 0U ? 1 : 0));
        CHECK(quiet_noted);
    }
    std::int64_t alive_lines = 0;
    for (const std::string& l : block(summary, "[present]")) {
        if (l.size() > 1 && l[0] == 'c' && l[1] >= '0' && l[1] <= '9') {
            ++alive_lines;
            const std::string id = l.substr(0, l.find(' '));
            CHECK_MESSAGE(fates[id] == l.substr(l.find(": ") + 2), l);
        } else if (l.rfind("civilizations alive: ", 0) == 0) {
            CHECK(std::stoll(l.substr(21)) == stats.at("civilizations_alive"));
        } else if (l.rfind("regions: ", 0) == 0) {
            CHECK(std::stoll(l.substr(l.find("held ") + 5)) == stats.at("regions_held"));
        }
    }
    CHECK(alive_lines == stats.at("civilizations_alive"));
    CHECK(static_cast<std::int64_t>(fates.size()) == stats.at("civilizations_alive"));
}

}  // namespace hrtest
