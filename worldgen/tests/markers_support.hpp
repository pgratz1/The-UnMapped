#pragma once

// Test support for spec P4-04 (settlement and place markers, Part A): fixture T2 (worldgen/tests/data/markers/,
// the P3-07 fixture format with a [markers] table of region seed cells and pass rows) written through the
// writer API, the dumps of 10.1 computed from the library's answers, the overlay hash of 10.3 item 4, and the
// golden file worldgen/tests/golden/p4-04-markers.txt (written by reference/p4_markers_reference.py).
//
// Written from the spec only (docs/specs/p4-04-markers.md r1); the library's API is that of section 4. The one
// name the spec does not give is the namespace of settlement_kind_word and settlement_status_word: read as
// worldgen::history (the namespace of digest.hpp, where phase_kind_word lives), used through WORDS_NS below.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <toml.hpp>

#include "history_report_fixture.hpp"
#include "markers_fixture.hpp"
#include "timeline_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/territory.hpp"

#ifndef UNMAPPED_P4_04_GOLDEN_PATH
#error "UNMAPPED_P4_04_GOLDEN_PATH must name worldgen/tests/golden/p4-04-markers.txt"
#endif

// The namespace of settlement_kind_word / settlement_status_word (a reading; see the header comment).
#define WORDS_NS ::worldgen::history

namespace mktest {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace wh = worldgen::history;
namespace wi = worldgen::inspect;
namespace wd = worldgen::data;

// The world of a fixture case: T2, T1 or one of the P3-07 fixtures.
inline std::string fixture_world(const std::string& name) { return name == T2 ? t2_world() : tltest::fixture_world(name); }

inline std::vector<std::string> fixture_names() {
    std::vector<std::string> v = hrtest::fixture_names();
    v.push_back(tltest::T1);
    v.push_back(T2);
    return v;
}

// --- Opening a world with its marker index --------------------------------------------------------

struct Indexed {
    db::WorldReader reader;
    wh::LoadedHistory h;
    wi::MarkerIndex index;
};

inline wi::MarkerIndex load_index(const db::WorldReader& r, const wh::LoadedHistory& h) {
    auto m = wi::MarkerIndex::load(r, h);
    REQUIRE_MESSAGE(m.ok(), "MarkerIndex::load failed: " << (m.ok() ? "" : m.error().message));
    return std::move(m).value();
}

inline Indexed open_indexed(const std::string& path) {
    db::WorldReader r = tltest::open_reader(path);
    wh::LoadedHistory h = tltest::load(r);
    wi::MarkerIndex ix = load_index(r, h);
    return Indexed{std::move(r), std::move(h), std::move(ix)};
}

inline const wi::MarkerOptions ALL{true, true, true};

// --- The dumps of 10.1 ----------------------------------------------------------------------------

inline std::string num(std::int64_t v) { return std::to_string(v); }

inline std::string type_word(std::int64_t status) {
    switch (status) {
        case 1:
        case 2: return "standing";
        case 3: return "ruin";
        case 4: return "faded";
        default: return "?" + num(status);
    }
}

// M(Y) of a YearMarkers.
inline std::string marker_dump(const wi::YearMarkers& m) {
    std::string out = "year " + num(m.year) + "\n";
    for (const wi::SettlementMarker& s : m.settlements) {
        out += "settlement " + num(s.settlement) + " " + num(s.x) + " " + num(s.y) + " " + type_word(s.status) + " " +
               num(s.kind) + " " + num(s.status) + " " + num(s.owner) + " " + num(s.population) + " " + num(s.since) +
               " " + num(s.founded) + " " + (s.capital ? "1" : "0") + " " + num(s.ruined_year) + " " +
               num(s.ruin_event) + "\n";
    }
    for (const wi::PlaceMarker& p : m.places) {
        out += "place " + num(p.x) + " " + num(p.y) + " " + std::string(wi::event_symbol_word(p.symbol));
        for (const std::int64_t e : p.events) out += " " + num(e);
        out += "\n";
    }
    for (const wi::PassMarker& p : m.passes) {
        out += "pass " + num(p.pass) + " " + num(p.x) + " " + num(p.y) + " " + num(p.kind) + "\n";
    }
    return out;
}

// The kind key of an event (the stored catalogue).
inline std::string_view kind_key(const wh::LoadedHistory& h, std::int64_t event) {
    const wh::HistoryView::Event* e = h.view.event(event);
    REQUIRE(e != nullptr);
    const wh::KindDef* k = h.view.catalogue().find_id(e->row.kind);
    REQUIRE(k != nullptr);
    return k->key;
}

// The static dump of 10.1: the capital runs (civilization ascending, then event), then the ruins. The capital runs
// are not in the API: they are computed here from the stored events by the rule of 3.2, written independently
// of the reference; the ruin lines are the library's ruined_year and ruin_event. capital() itself is pinned by
// the capital column of the marker dumps and by the T2 and property tests.
inline std::string static_dump(const wh::LoadedHistory& h, const wi::MarkerIndex& ix) {
    std::string out;
    struct Entry {
        std::int64_t year, event, settlement;
    };
    std::map<std::int64_t, std::vector<Entry>> runs;
    for (const wh::HistoryView::Event& e : h.view.events()) {
        const wh::KindDef* k = h.view.catalogue().find_id(e.row.kind);
        REQUIRE(k != nullptr);
        std::string_view role;
        if (k->key == "settlement_founded") {
            role = "founder";
        } else if (k->key == "civ_seceded" || k->key == "capital_moved") {
            role = "civilization";
        } else {
            continue;
        }
        if (e.row.settlement == 0) continue;
        std::vector<std::int64_t> named;
        for (const db::EventParticipantRow& p : e.participants) {
            const wh::RoleDef* r = nullptr;
            for (const wh::RoleDef& d : k->roles) {
                if (d.seq == p.role) r = &d;
            }
            if (r != nullptr && r->role == role && p.entity_type == 3 &&
                std::find(named.begin(), named.end(), p.entity_id) == named.end()) {
                named.push_back(p.entity_id);
            }
        }
        for (const std::int64_t c : named) runs[c].push_back({e.row.year, e.row.id, e.row.settlement});
    }
    for (const auto& [c, run] : runs) {
        for (const Entry& en : run) {
            out += "capital " + num(c) + " " + num(en.year) + " " + num(en.event) + " " + num(en.settlement) + "\n";
        }
    }
    for (const db::SettlementRow& s : h.tables.settlements) {
        const std::int64_t r = ix.ruined_year(s.id);
        if (r != -1) out += "ruin " + num(s.id) + " " + num(r) + " " + num(ix.ruin_event(s.id)) + "\n";
    }
    return out;
}

// 8.3 glyph names.
inline std::string glyph_name(const wi::GlyphKey& k) {
    switch (k.base) {
        case 8: return "ruin";
        case 9: return "faded";
        case 10: return "pass";
        case 11: return "ring";
        default: break;
    }
    std::string s(WORDS_NS::settlement_kind_word(k.base));
    if (k.declining) s += " declining";
    if (k.capital) s += " capital";
    return s;
}

// The 24 keys of the glyph dump, in its order (reading M3 of the reference).
inline std::vector<wi::GlyphKey> glyph_keys() {
    std::vector<wi::GlyphKey> v;
    for (std::uint8_t b = 1; b <= 5; ++b) {
        for (const bool d : {false, true}) {
            for (const bool c : {false, true}) v.push_back({b, d, c});
        }
    }
    for (const std::uint8_t b : {std::uint8_t{8}, std::uint8_t{9}, std::uint8_t{10}, std::uint8_t{11}}) {
        v.push_back({b, false, false});
    }
    return v;
}

inline std::string glyph_dump() {
    std::string out;
    for (const wi::GlyphKey& k : glyph_keys()) {
        const wi::Glyph g = wi::marker_glyph(k);
        const auto n = static_cast<std::size_t>(2 * g.radius + 1);
        REQUIRE(g.cells.size() == n * n);
        out += "glyph " + glyph_name(k) + " " + num(g.radius) + "\n";
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < n; ++i) out += static_cast<char>('0' + g.cells[j * n + i]);
            out += "\n";
        }
    }
    return out;
}

inline std::string sha(std::string_view text) { return worldgen::sha256_hex(text); }
inline std::string sha_bytes(std::span<const std::uint8_t> b) {
    return worldgen::sha256_hex(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
}

// The golden years of 10.3 item 2.
inline std::vector<std::int64_t> golden_years(const wh::LoadedHistory& h, const wi::MarkerIndex& ix) {
    const std::int64_t E = h.tables.end_year;
    std::vector<std::int64_t> v = {0, E / 2, E - 1, E};
    for (const db::AgeRow& a : h.tables.ages) v.push_back(a.start_year);
    for (const db::SettlementRow& s : h.tables.settlements) {
        if (ix.ruined_year(s.id) != -1) v.push_back(ix.ruined_year(s.id));
    }
    for (const wh::HistoryView::Event& e : h.view.events()) {
        if (kind_key(h, e.row.id) == "ruin_faded") v.push_back(e.row.year);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    v.erase(std::remove_if(v.begin(), v.end(), [&](std::int64_t y) { return y < 0 || y > E; }), v.end());
    return v;
}

// SHA-256 of M(0) ... M(E), through the reusing form of markers_at.
inline std::string years_hash(const wh::LoadedHistory& h, const wi::MarkerIndex& ix) {
    worldgen::Sha256 m;
    wh::YearState st;
    wi::YearMarkers out;
    for (std::int64_t y = 0; y <= h.tables.end_year; ++y) {
        h.timeline.year_state(y, st);
        ix.markers_at(h, st, ALL, out);
        m.update(std::string_view(marker_dump(out)));
    }
    return worldgen::to_hex(m.final());
}

// The overlay of 10.3 item 4: render_territory at Y, then draw_markers (all options) at scale k.
inline std::vector<std::uint8_t> overlay(const wi::TerritoryBase& base, const wh::LoadedHistory& h,
                                         const wi::MarkerIndex& ix, std::int64_t y, std::uint32_t k,
                                         const wi::MarkerOptions& opt = ALL) {
    const std::vector<wi::Rgb> colours = wi::civilization_colours(h.tables);
    std::vector<std::uint8_t> rgb = wi::render_territory(base, h.timeline, colours, y, k);
    wi::draw_markers(rgb, base.width(), base.height(), k, ix.markers_at(h, y, opt), colours);
    return rgb;
}

inline std::vector<std::int64_t> overlay_years(const wh::LoadedHistory& h, const wi::MarkerIndex& ix) {
    const std::int64_t E = h.tables.end_year;
    std::vector<std::int64_t> v = {E / 2, E};
    std::optional<std::int64_t> first;
    for (const db::SettlementRow& s : h.tables.settlements) {
        const std::int64_t r = ix.ruined_year(s.id);
        if (r != -1 && (!first || r < *first)) first = r;
    }
    if (first) v.push_back(*first);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

// --- The golden file ------------------------------------------------------------------------------

struct GoldenCase {
    std::string static_hash;
    std::vector<std::pair<std::int64_t, std::string>> markers;  // item 2, file order
    std::string years_hash;                                      // item 3
    std::vector<std::pair<std::int64_t, std::string>> overlay;  // item 4, file order
    std::optional<std::pair<std::int64_t, std::string>> overlay3;
    std::map<std::string, std::string> tables;  // T2 only: hash --detail's table lines of the reference's copy
};

struct Golden {
    std::map<std::string, GoldenCase> cases;
    std::string glyphs;
};

inline const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        std::istringstream in(hrtest::detail::text_of_file(fs::path(UNMAPPED_P4_04_GOLDEN_PATH)));
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string name, what, a, b;
            ls >> name >> what;
            if (name == "glyphs") {
                out.glyphs = what;
                continue;
            }
            ls >> a;
            GoldenCase& c = out.cases[name];
            if (what == "static") {
                c.static_hash = a;
            } else if (what == "markers-years") {
                c.years_hash = a;
            } else if (what == "markers") {
                ls >> b;
                c.markers.emplace_back(std::stoll(a), b);
            } else if (what == "overlay") {
                ls >> b;
                c.overlay.emplace_back(std::stoll(a), b);
            } else if (what == "table") {
                ls >> b;
                c.tables[a] = b;
            } else if (what == "overlay-scale3") {
                ls >> b;
                c.overlay3 = std::make_pair(std::stoll(a), b);
            } else {
                FAIL("p4-04-markers.txt: unknown line " << line);
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

// Items 1 to 3 of 10.3 for a case; on a difference the C++ dump is printed (the first 4000 bytes), to diff
// against `p4_markers_reference.py dump`.
inline void check_dumps(const std::string& name, const wh::LoadedHistory& h, const wi::MarkerIndex& ix) {
    CAPTURE(name);
    const GoldenCase& g = golden_case(name);
    const std::string s = static_dump(h, ix);
    CHECK_MESSAGE(sha(s) == g.static_hash, "static dump of " << name << ":\n" << s.substr(0, 4000));
    std::vector<std::pair<std::int64_t, std::string>> got;
    for (const std::int64_t y : golden_years(h, ix)) {
        const std::string d = marker_dump(ix.markers_at(h, y, ALL));
        got.emplace_back(y, sha(d));
        const auto it = std::find_if(g.markers.begin(), g.markers.end(), [&](const auto& p) { return p.first == y; });
        if (it != g.markers.end() && it->second != got.back().second) {
            FAIL_CHECK("M(" << y << ") of " << name << " differs:\n" << d.substr(0, 4000));
        }
    }
    CHECK(got == g.markers);
    CHECK(years_hash(h, ix) == g.years_hash);
}

// Item 4 for a world case.
inline void check_overlays(const std::string& name, const std::string& path) {
    CAPTURE(name);
    const GoldenCase& g = golden_case(name);
    const Indexed o = open_indexed(path);
    const tltest::WorldLayers layers = tltest::read_layers(o.reader);
    const wi::TerritoryBase base = tltest::territory_base(layers);
    std::vector<std::pair<std::int64_t, std::string>> got;
    for (const std::int64_t y : overlay_years(o.h, o.index)) {
        got.emplace_back(y, sha_bytes(overlay(base, o.h, o.index, y, 1)));
    }
    CHECK(got == g.overlay);
    const std::int64_t E = o.h.tables.end_year;
    REQUIRE(g.overlay3.has_value());
    CHECK(g.overlay3->first == E);
    CHECK(sha_bytes(overlay(base, o.h, o.index, E, 3)) == g.overlay3->second);
}

}  // namespace mktest
