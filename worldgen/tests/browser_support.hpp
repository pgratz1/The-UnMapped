#pragma once

// Test support for spec P4-05 Part A (the browser of civilizations and events): fixture T3
// (worldgen/tests/data/browser/, the P3-07 fixture format with the [markers] region seed cells) written through
// the writer API, the cases of 10.1 (the P3-07 fixtures, T1, T2, T3 and the world cases of P4-01 13.4), a
// world opened with its history, marker index and browser index, the dumps of 10.1 computed from the
// library's answers, and the golden file worldgen/tests/golden/p4-05-browser.txt (written by
// reference/p4_browser_reference.py, whose READINGS B1 to B12 fix the dump lines the spec leaves open).
//
// Written from the spec only (docs/specs/p4-05-browser.md r1); the library is reached through browser_api.hpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <toml.hpp>

#include "browser_api.hpp"
#include "history_report_fixture.hpp"
#include "markers_support.hpp"
#include "timeline_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"

#ifndef UNMAPPED_P4_05_FIXTURE_DIR
#error "UNMAPPED_P4_05_FIXTURE_DIR must name worldgen/tests/data/browser"
#endif
#ifndef UNMAPPED_P4_05_GOLDEN_PATH
#error "UNMAPPED_P4_05_GOLDEN_PATH must name worldgen/tests/golden/p4-05-browser.txt"
#endif

namespace brtest {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace wh = worldgen::history;
namespace wi = worldgen::inspect;

using brapi::HistoryLevel;

inline const std::string T3 = "t3-browser";

// --- Fixture T3 -------------------------------------------------------------------------------------

inline fs::path t3_toml() { return fs::path(UNMAPPED_P4_05_FIXTURE_DIR) / "t3-browser.toml"; }

// The rows of T3: hrtest's expansion of the P3-07 format with the region seed cells of [markers].
inline const hrtest::Fixture& t3_fixture() {
    static const hrtest::Fixture fx = [] {
        hrtest::Fixture f = hrtest::load_fixture(db::path_to_utf8(t3_toml().replace_extension()));
        const fs::path path = t3_toml();
        const toml::table doc = toml::parse(hrtest::detail::text_of_file(path), path.string());
        const toml::array* seeds = doc["markers"]["region_seed"].as_array();
        REQUIRE(seeds != nullptr);
        for (const toml::node& n : *seeds) {
            const toml::array& r = *n.as_array();
            const std::int64_t id = hrtest::detail::i64(*r.get(0));
            bool found = false;
            for (db::RegionRow& row : f.regions) {
                if (row.id == id) {
                    row.seed_x = hrtest::detail::i64(*r.get(1));
                    row.seed_y = hrtest::detail::i64(*r.get(2));
                    found = true;
                }
            }
            REQUIRE_MESSAGE(found, "T3: region_seed of no region " << id);
        }
        return f;
    }();
    return fx;
}

namespace detail {
inline const dbtest::TempDir& scratch() {
    static const dbtest::TempDir d("browser");
    return d;
}
}  // namespace detail

// The T3 world, written once per process.
inline const std::string& t3_world() {
    static const std::string path = [] {
        const std::string p = detail::scratch().file("t3-browser.db");
        hrtest::write_fixture_world(t3_fixture(), p);
        return p;
    }();
    return path;
}

// --- Cases ------------------------------------------------------------------------------------------

inline std::vector<std::string> fixture_names() {
    std::vector<std::string> v = mktest::fixture_names();  // the P3-07 fixtures, T1, T2
    v.push_back(T3);
    return v;
}

inline std::string fixture_world(const std::string& name) {
    return name == T3 ? t3_world() : mktest::fixture_world(name);
}

// --- A world opened with every index --------------------------------------------------------------

struct Opened {
    db::WorldReader reader;
    wh::LoadedHistory h;
    wi::MarkerIndex markers;
    brapi::BrowserIndex index;
};

inline brapi::BrowserIndex build_index(const wh::LoadedHistory& h) {
    auto b = brapi::BrowserIndex::build(h);
    REQUIRE_MESSAGE(b.ok(), "BrowserIndex::build failed: " << (b.ok() ? "" : b.error().message));
    return std::move(b).value();
}

inline std::unique_ptr<Opened> open_fresh(const std::string& path) {
    db::WorldReader r = tltest::open_reader(path);
    wh::LoadedHistory h = tltest::load(r);
    wi::MarkerIndex m = mktest::load_index(r, h);
    brapi::BrowserIndex b = build_index(h);
    return std::make_unique<Opened>(Opened{std::move(r), std::move(h), std::move(m), std::move(b)});
}

// Opened once per process and path.
inline const Opened& opened(const std::string& path) {
    static std::map<std::string, std::unique_ptr<Opened>> cache;
    auto it = cache.find(path);
    if (it == cache.end()) it = cache.emplace(path, open_fresh(path)).first;
    return *it->second;
}

inline const Opened& t3() { return opened(t3_world()); }

// --- Text -------------------------------------------------------------------------------------------

inline std::string num(std::int64_t v) { return std::to_string(v); }
inline std::string unum(std::uint64_t v) { return std::to_string(v); }

// 10.1: raw bytes; only "\", LF and the bytes below 0x20 escaped as escape_history_text writes them.
inline std::string dtext(std::string_view s) {
    static const char* const hex = "0123456789ABCDEF";
    std::string out;
    for (const char ch : s) {
        const auto b = static_cast<unsigned char>(ch);
        if (b == 0x5CU) {
            out += "\\\\";
        } else if (b < 0x20U) {
            out += "\\x";
            out += hex[b >> 4U];
            out += hex[b & 0xFU];
        } else {
            out += ch;
        }
    }
    return out;
}

inline std::string sha(std::string_view text) { return worldgen::sha256_hex(text); }

// The report's tag of an entity (P3-07 "Ids"): ra, g, c, s, rg, f, fa, m, si plus the id.
inline std::string tag(std::int64_t type, std::int64_t id) {
    static const char* const prefix[] = {"", "ra", "g", "c", "s", "rg", "f", "fa", "m", "si"};
    REQUIRE((type >= 1 && type <= 9));
    return std::string(prefix[type]) + num(id);
}

inline std::int64_t event_count(const wh::LoadedHistory& h) {
    return static_cast<std::int64_t>(h.view.events().size());
}

inline std::string kind_key(const wh::LoadedHistory& h, std::int64_t event) {
    const wh::HistoryView::Event* e = h.view.event(event);
    REQUIRE(e != nullptr);
    const wh::KindDef* k = h.view.catalogue().find_id(e->row.kind);
    REQUIRE(k != nullptr);
    return k->key;
}

// --- The dumps of 10.1 --------------------------------------------------------------------------------

inline std::string dump_civilizations(const Opened& o) {
    std::string out;
    for (const brapi::CivilizationEntry& c : o.index.civilizations()) {
        out += "civ " + num(c.id) + " " + num(c.race) + " " + num(c.founded_year) + " " + num(c.ended_year) + " " +
               num(c.ending_event) + " " + num(c.events) + " " + dtext(c.name) + "|" + dtext(c.race_name) + "\n";
    }
    return out;
}

inline std::string link_lines(const brapi::LinkedText& t, bool with_tag) {
    std::string out;
    for (const brapi::TextLink& l : t.links) {
        out += "link " + unum(l.start) + " " + unum(l.length) + " " + num(l.type) + " " + num(l.id);
        if (with_tag) out += l.tagged ? " 1" : " 0";
        out += "\n";
    }
    return out;
}

inline std::string dump_links(const Opened& o) {
    std::string out;
    for (std::int64_t e = 1; e <= event_count(o.h); ++e) {
        const brapi::LinkedText t = brapi::render_event_linked(o.h.view, e);
        out += "event " + num(e) + " " + dtext(t.text) + "\n" + link_lines(t, false);
    }
    return out;
}

inline std::string dump_details(const Opened& o) {
    std::string out;
    for (std::int64_t e = 1; e <= event_count(o.h); ++e) {
        const brapi::EventDetail d = o.index.event_detail(o.h, e);
        out += "detail " + num(e) + " " + std::string(brapi::event_group_word(d.entry.group)) + " " +
               (d.entry.summary ? "1" : "0") + " " + dtext(d.kind_key) + "\n";
        out += "note " + dtext(d.note) + "\n";
        for (const brapi::Participant& p : d.participants) {
            out += "part " + dtext(p.role) + " " + num(p.type) + " " + num(p.id) + " " + dtext(p.name) + "\n";
        }
        for (const auto& [k, v] : d.values) out += "value " + dtext(k) + " " + num(v) + "\n";
        for (const std::int64_t c : d.causes) out += "cause " + num(c) + "\n";
        for (const std::int64_t c : d.effects) out += "effect " + num(c) + "\n";
        const auto p = brapi::event_place(o.markers, o.h, e);
        out += p ? "place " + num((*p)[0]) + " " + num((*p)[1]) + "\n" : std::string("place none\n");
    }
    return out;
}

inline std::string dump_blocks(const Opened& o, HistoryLevel level) {
    std::string out;
    for (const brapi::CivilizationEntry& c : o.index.civilizations()) {
        out += "block " + num(c.id) + "\n";
        for (const brapi::LinkedText& t : o.index.block(o.h, c.id, level)) {
            out += "line " + dtext(t.text) + "\n" + link_lines(t, true);
        }
    }
    return out;
}

inline brapi::EventFilter full_filter() {
    brapi::EventFilter f;
    f.summary_only = false;
    return f;
}

// Reading B6 of the reference: the filters of 10.1 in order, with their descriptions.
inline std::vector<std::pair<std::string, brapi::EventFilter>> filter_list(const Opened& o) {
    std::vector<std::pair<std::string, brapi::EventFilter>> out;
    const std::int64_t E = o.h.tables.end_year;
    out.emplace_back("all summary", brapi::EventFilter{});
    out.emplace_back("all full", full_filter());
    for (const brapi::CivilizationEntry& c : o.index.civilizations()) {
        brapi::EventFilter s;
        s.civilization = c.id;
        out.emplace_back("civ " + num(c.id) + " summary", s);
        brapi::EventFilter f = full_filter();
        f.civilization = c.id;
        out.emplace_back("civ " + num(c.id) + " full", f);
    }
    const auto settlements = static_cast<std::int64_t>(o.h.timeline.settlement_count());
    for (std::int64_t s = 1; s <= settlements; ++s) {
        if (o.h.timeline.events_of_settlement(s).empty()) continue;
        brapi::EventFilter f = full_filter();
        f.settlement = s;
        out.emplace_back("settlement " + num(s) + " full", f);
    }
    for (const std::int64_t r : o.h.timeline.region_ids()) {
        if (o.h.timeline.events_of_region(r).empty()) continue;
        brapi::EventFilter f = full_filter();
        f.region = r;
        out.emplace_back("region " + num(r) + " full", f);
    }
    for (std::uint8_t g = 0; g <= 8; ++g) {  // the nine groups: monsters (5), peoples (6, P5-03 ruling 4), alliances (7, P5-04 11.2), figures (8, P5-06 11.3)
        brapi::EventFilter f = full_filter();
        f.groups = 1U << g;
        out.emplace_back("group " + std::string(brapi::event_group_word(static_cast<brapi::EventGroup>(g))) + " full",
                         f);
    }
    brapi::EventFilter a = full_filter();
    a.from_year = 0;
    a.to_year = E / 2;
    out.emplace_back("years 0 " + num(E / 2) + " full", a);
    brapi::EventFilter b = full_filter();
    b.from_year = E / 2;
    b.to_year = E;
    out.emplace_back("years " + num(E / 2) + " " + num(E) + " full", b);
    if (!o.index.civilizations().empty() && o.index.civilizations().front().id == 1) {
        brapi::EventFilter f = full_filter();
        f.civilization = 1;
        f.groups = 1U << 2U;
        out.emplace_back("civ 1 group war full", f);
    }
    return out;
}

inline std::vector<std::int64_t> select(const Opened& o, const brapi::EventFilter& f) {
    std::vector<std::int64_t> ids;
    o.index.select(o.h, f, ids);
    return ids;
}

inline std::string ids_text(const std::vector<std::int64_t>& ids) {
    std::string out;
    for (std::size_t i = 0; i < ids.size(); ++i) out += (i ? " " : "") + num(ids[i]);
    return out;
}

inline std::string dump_filters(const Opened& o) {
    std::string out;
    for (const auto& [desc, f] : filter_list(o)) out += "filter " + desc + "\n" + ids_text(select(o, f)) + "\n";
    return out;
}

inline const std::string NOTHING = "qqqzzzxxq";

inline std::vector<std::pair<std::string, std::size_t>> search_queries(const Opened& o) {
    std::vector<std::pair<std::string, std::size_t>> q = {{"a", 50}, {"the", 50}, {"THE", 50}};
    for (const brapi::CivilizationEntry& c : o.index.civilizations()) q.emplace_back(c.name.substr(0, 3), 50);
    q.emplace_back(NOTHING, 50);
    q.emplace_back("a", 3);
    return q;
}

inline std::string dump_search(const Opened& o) {
    std::string out;
    for (const auto& [q, limit] : search_queries(o)) {
        out += "query " + unum(limit) + " " + dtext(q) + "\n";
        for (const brapi::NameMatch& m : o.index.search(o.h, q, limit)) {
            // P5-06 11.5 adds figures (type 6) after the regions; the reference's search and its golden
            // (p4-05-browser.txt) know only the three older entity types, so the dump leaves them out.
            if (m.type == 6) continue;
            out += "match " + num(m.type) + " " + num(m.id) + " " + dtext(m.name) + "\n";
        }
    }
    return out;
}

inline std::string groups_dump() {
    std::string out;
    for (const wh::KindDef& k : wh::compiled_event_catalogue().kinds) {
        out += "group " + k.key + " " + std::string(brapi::event_group_word(brapi::event_group(k.key))) + "\n";
    }
    out += "group unknown_key " + std::string(brapi::event_group_word(brapi::event_group("unknown_key"))) + "\n";
    return out;
}

inline const std::vector<std::string>& dump_names() {
    static const std::vector<std::string> v = {"civilizations",  "links",       "details", "blocks-summary",
                                               "blocks-full",    "filters",     "search"};
    return v;
}

inline std::string dump(const Opened& o, const std::string& name) {
    if (name == "civilizations") return dump_civilizations(o);
    if (name == "links") return dump_links(o);
    if (name == "details") return dump_details(o);
    if (name == "blocks-summary") return dump_blocks(o, HistoryLevel::summary);
    if (name == "blocks-full") return dump_blocks(o, HistoryLevel::full);
    if (name == "filters") return dump_filters(o);
    if (name == "search") return dump_search(o);
    FAIL("unknown dump " << name);
    return {};
}

// --- The golden file --------------------------------------------------------------------------------

struct GoldenCase {
    std::map<std::string, std::string> dumps;   // dump name -> sha256
    std::map<std::string, std::string> tables;  // T3 only: hash --detail's table lines of the reference's copy
};
struct Golden {
    std::map<std::string, GoldenCase> cases;
    std::string groups;
};

inline const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        std::istringstream in(hrtest::detail::text_of_file(fs::path(UNMAPPED_P4_05_GOLDEN_PATH)));
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string name, what, a, b;
            ls >> name >> what;
            if (name == "groups") {
                out.groups = what;
                continue;
            }
            ls >> a;
            if (what == "table") {
                ls >> b;
                out.cases[name].tables[a] = b;
            } else {
                out.cases[name].dumps[what] = a;
            }
        }
        return out;
    }();
    return g;
}

inline const GoldenCase& golden_case(const std::string& name) {
    const auto it = golden().cases.find(name);
    REQUIRE_MESSAGE(it != golden().cases.end(), "no golden lines for " << name);
    return it->second;
}

// Every dump of a case against the golden file; on a difference the C++ dump is printed (its first 4000
// bytes), to diff against `p4_browser_reference.py dump DB NAME`.
inline void check_dumps(const std::string& name, const Opened& o) {
    CAPTURE(name);
    const GoldenCase& g = golden_case(name);
    for (const std::string& d : dump_names()) {
        CAPTURE(d);
        const auto it = g.dumps.find(d);
        REQUIRE_MESSAGE(it != g.dumps.end(), "no golden " << d << " line for " << name);
        const std::string text = dump(o, d);
        CHECK_MESSAGE(sha(text) == it->second, d << " dump of " << name << ":\n" << text.substr(0, 4000));
    }
}

// --- Report text helpers (P3-07 6.1) ------------------------------------------------------------------

// The report writes each trailing space of a line as \x20 (P3-07 R4); applied to an already escaped line.
inline std::string trailing_spaces_escaped(std::string s) {
    std::size_t n = 0;
    while (n < s.size() && s[s.size() - 1 - n] == ' ') ++n;
    s.resize(s.size() - n);
    for (std::size_t i = 0; i < n; ++i) s += "\\x20";
    return s;
}

// B4's transform: the display line turned back into report form (every byte outside a tagged link escaped,
// each tagged link's text t replaced by escape(t) + " (" + tag + ")").
inline std::string b4_report_form(const brapi::LinkedText& t) {
    std::string out;
    std::size_t at = 0;
    for (const brapi::TextLink& l : t.links) {
        if (!l.tagged) continue;
        const std::size_t a = l.start, n = l.length;
        REQUIRE(a >= at);
        REQUIRE(a + n <= t.text.size());
        out += wi::escape_history_text(std::string_view(t.text).substr(at, a - at));
        out += wi::escape_history_text(std::string_view(t.text).substr(a, n)) + " (" + tag(l.type, l.id) + ")";
        at = a + n;
    }
    out += wi::escape_history_text(std::string_view(t.text).substr(at));
    return out;
}

// Links ascend and never overlap, and lie inside the text.
inline bool links_well_formed(const brapi::LinkedText& t) {
    std::uint64_t end = 0;
    for (const brapi::TextLink& l : t.links) {
        if (l.start < end || std::uint64_t{l.start} + l.length > t.text.size()) return false;
        end = std::uint64_t{l.start} + l.length;
    }
    return true;
}

}  // namespace brtest
