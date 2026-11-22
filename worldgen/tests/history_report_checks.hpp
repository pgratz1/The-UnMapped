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
        W_ = ids({"war_declared", "battle", "battle_at_pass", "siege", "sacked", "settlement_captured", "refuge_held",
                  "peace", "war_won", "truce"});
        EW_ = ids({"battle", "battle_at_pass", "siege", "sacked", "settlement_captured", "refuge_held", "peace",
                   "war_won", "truce", "search_abandoned", "region_ceded", "settlements_ceded", "tribute",
                   "capital_moved", "refugees_settled", "civ_conquered"});
        battles_ = ids({"battle", "battle_at_pass"});
        falls_ = ids({"sacked", "settlement_captured"});
        conseq_ = ids({"capital_moved", "refugees_settled", "civ_conquered"});
        claims_ = ids({"claim", "claim_settlements"});
        spread_ = ids({"plague_spread"});
        quiet_ = ids({"quiet_stretch"});
        const auto* decl = cat.find("war_declared");
        const auto* search = cat.find("search_abandoned");
        // the war rows: since_event -> (closed, until_event)
        for (const auto& row : rows_of(r, "relation")) {
            if (row[4].bytes != "war") continue;
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
            if (battles_.count(k) != 0U && (!d || last_battle(*d) != x)) bad.push_back(ex + "not its war's last battle");
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
    std::set<std::int64_t> W_, EW_, battles_, falls_, conseq_, claims_, spread_, quiet_, excluded_;
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

// The lines of [civilizations] of the text without the lines that differ between the levels (24.10 item 3).
inline std::vector<std::string> civ_common(const std::string& text, bool full) {
    std::vector<std::string> out;
    bool skip = false;
    for (const std::string& l : block(text, "[civilizations]")) {
        if (l.rfind(full ? "  relations:" : "  adversaries:", 0) == 0) {
            skip = true;
            continue;
        }
        if (skip && l.rfind("    ", 0) == 0) continue;
        skip = false;
        out.push_back(l);
    }
    return out;
}

// The invariants of r17 24.10 (items 1 to 7 as far as they are checks on the text; item 7's standing claims
// are spelled out per fixture by the tests) on the summary and full texts of one world.
inline void check_invariants(const db::WorldReader& r, const std::string& summary, const std::string& full) {
    const std::vector<std::string> heads_s = {"[settings]", "[ages]", "[chronicle]", "[civilizations]", "[present]",
                                              "[stats]"};
    const std::vector<std::string> heads_f = {"[settings]", "[ages]", "[phases]", "[chronicle]", "[civilizations]",
                                              "[present]", "[stats]"};
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
        CHECK(lines[0] == worldgen::inspect::HISTORY_REPORT_HEADER_V1);
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
    const auto stats = stats_of(summary);
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
    for (const char* b : {"[settings]", "[ages]", "[stats]"}) {
        CAPTURE(std::string_view(b));
        CHECK(block(summary, b) == block(full, b));
    }
    CHECK(civ_common(summary, false) == civ_common(full, true));
    std::vector<std::string> present_f;
    for (const std::string& l : block(full, "[present]")) {
        const bool region_line = l.rfind("  rg", 0) == 0;
        const bool settlement_line = l.rfind("  s", 0) == 0 && l.size() > 3 && l[3] >= '0' && l[3] <= '9' &&
                                     l.find(", at ") != std::string::npos;
        if (!region_line && !settlement_line) present_f.push_back(l);
    }
    CHECK(present_f == block(summary, "[present]"));
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
        std::int64_t af = -1, k_sum = 0;
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
                continue;
            }
            if (in_adv && l.rfind("    ", 0) == 0) {
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
