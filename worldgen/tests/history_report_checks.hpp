#pragma once

// The invariants of spec P3-07 section 13 as checks on the two report texts of one world, and the
// library calls of section 5.1 the P3-07 tests use. Compiles to the switch only until the report exists
// (p3_07_switch.hpp).

#include "p3_07_switch.hpp"

#if UNMAPPED_P3_07_REPORT

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
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

// Invariants 1 to 7 of section 13 on the summary and full texts of one world (8: the callers vary the
// environment, the thread count and the platform).
inline void check_invariants(const db::WorldReader& r, const std::string& summary, const std::string& full) {
    const std::vector<std::string> heads = {"[settings]", "[ages]",    "[phases]", "[civilizations]",
                                            "[chronicle]", "[present]", "[stats]"};
    for (const std::string* text : {&summary, &full}) {
        const bool is_full = text == &full;
        CAPTURE(is_full);
        // 1: bytes, lines, header and blocks
        bool bytes_ok = !text->empty() && text->back() == '\n';
        for (const char c : *text) {
            const auto b = static_cast<unsigned char>(c);
            bytes_ok = bytes_ok && (b == 0x0A || (b >= 0x20 && b <= 0x7E));
        }
        CHECK(bytes_ok);
        const std::vector<std::string> lines = lines_of(*text);
        REQUIRE(lines.size() >= 2U);
        CHECK(lines[0] == "# worldgen history v1");
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
        CHECK(got_heads == heads);
    }
    const auto stats = stats_of(summary);
    const std::vector<std::string> chron_s = block(summary, "[chronicle]");
    const std::vector<std::string> chron_f = block(full, "[chronicle]");
    const auto& view_r = worldgen::history::HistoryView::load(r);
    REQUIRE(view_r.ok());
    const worldgen::history::HistoryView& view = view_r.value();
    const std::int64_t events = static_cast<std::int64_t>(view.events().size());
    // 2
    std::vector<std::int64_t> ids_f, ids_s;
    for (const std::string& l : chron_f) {
        if (is_event_line(l)) ids_f.push_back(event_id_of(l));
    }
    for (const std::string& l : chron_s) {
        if (is_event_line(l)) ids_s.push_back(event_id_of(l));
    }
    std::vector<std::int64_t> all;
    for (std::int64_t i = 1; i <= events; ++i) all.push_back(i);
    CHECK(ids_f == all);
    CHECK(contains_line(chron_f, "threshold = 0"));
    CHECK(contains_line(chron_f, "shown = " + std::to_string(events)));
    CHECK(contains_line(chron_s, "shown = " + std::to_string(ids_s.size())));
    CHECK(contains_line(chron_s, "events = " + std::to_string(events)));
    CHECK(static_cast<std::int64_t>(ids_s.size()) == stats.at("summary_events"));
    CHECK(contains_line(chron_s, "threshold = " + std::to_string(stats.at("summary_threshold"))));
    CHECK(std::is_sorted(ids_s.begin(), ids_s.end()));
    if (stats.at("summary_threshold") < 100) {
        CHECK(ids_s.size() <= 250U);
    }
    // 3: shown and hidden per phase, against [phases]
    std::map<std::int64_t, std::int64_t> per_phase;
    std::int64_t cur = 0, total = 0;
    for (const std::string& l : chron_s) {
        if (l.rfind("-- p", 0) == 0) {
            cur = std::stoll(l.substr(4));
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
    for (const std::string& l : block(summary, "[phases]")) {
        const std::int64_t p = std::stoll(l.substr(1));
        const std::size_t at = l.rfind("; ");
        REQUIRE(at != std::string::npos);
        const std::int64_t m = std::stoll(l.substr(at + 2));
        CHECK_MESSAGE(per_phase[p] == m, l);
    }
    // 4: every chronicle sentence is render_event's, escaped (the line continues with a note or the refs)
    for (const std::string& l : chron_f) {
        if (!is_event_line(l)) continue;
        const std::int64_t id = event_id_of(l);
        const std::size_t colon = l.find(": ");
        REQUIRE(colon != std::string::npos);
        const std::string s = esc(worldgen::history::render_event(view, id));
        const std::string rest = l.substr(colon + 2);
        CHECK_MESSAGE((rest.rfind(s + " {", 0) == 0 || rest.rfind(s + " (", 0) == 0), l);
    }
    // 5: blocks equal at both levels; [present] differs by the full-only lines
    for (const char* b : {"[settings]", "[ages]", "[phases]", "[civilizations]", "[stats]"}) {
        CAPTURE(std::string_view(b));
        CHECK(block(summary, b) == block(full, b));
    }
    std::vector<std::string> present_f;
    for (const std::string& l : block(full, "[present]")) {
        if (l.rfind("  rg", 0) != 0 && l.rfind("  s", 0) != 0) present_f.push_back(l);
    }
    CHECK(present_f == block(summary, "[present]"));
    // 6: [history] of the world report is a subset of [stats]
    const auto stats_text = [&] {
        std::map<std::string, std::string> m;
        for (const auto& [k, v] : stats) m[k] = std::to_string(v);
        return m;
    }();
    for (const auto& [k, v] : world_history_section(r)) {
        CAPTURE(k);
        REQUIRE(stats_text.count(k) == 1U);
        CHECK(stats_text.at(k) == v);
    }
    // 7: fate lines against [present]; alive and held counts
    std::map<std::string, std::string> fates;
    std::string civ;
    for (const std::string& l : block(summary, "[civilizations]")) {
        if (!l.empty() && l[0] == 'c') {
            civ = l.substr(0, l.find(' '));
        } else if (l.rfind("  fate: alive in ", 0) == 0) {
            const std::size_t at = l.find(": ", 8);
            fates[civ] = l.substr(at + 2);
        }
    }
    std::int64_t alive_lines = 0;
    for (const std::string& l : block(summary, "[present]")) {
        if (l.size() > 1 && l[0] == 'c' && l[1] >= '0' && l[1] <= '9') {
            ++alive_lines;
            const std::string id = l.substr(0, l.find(' '));
            const std::size_t at = l.find(": ");
            CHECK_MESSAGE(fates[id] == l.substr(at + 2), l);
        } else if (l.rfind("civilizations alive: ", 0) == 0) {
            CHECK(std::stoll(l.substr(21)) == stats.at("civilizations_alive"));
        } else if (l.rfind("regions: ", 0) == 0) {
            const std::size_t at = l.find("held ");
            REQUIRE(at != std::string::npos);
            CHECK(std::stoll(l.substr(at + 5)) == stats.at("regions_held"));
        }
    }
    CHECK(alive_lines == stats.at("civilizations_alive"));
    CHECK(static_cast<std::int64_t>(fates.size()) == stats.at("civilizations_alive"));
}

}  // namespace hrtest

#endif  // UNMAPPED_P3_07_REPORT
