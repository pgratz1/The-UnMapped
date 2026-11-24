// Spec P4-05 10.2 items 1, 3 and 6 and invariants 1 to 3 of section 11, on every case of 10.1 (the P3-07
// fixtures, T1, T2, T3 and the 512 world cases; the 1024 ones in the skipped suite "browser-slow"):
//   - render_event_linked's text is render_event for every event, its links ascend and never overlap, and every
//     link covers the name of what it names (11 item 1);
//   - row_text and EventDetail's sentence and note are render_event and history_note(view, e, 0) (11 item 1);
//   - block_line_report_text over civilization_block, joined, is the report's [civilizations] block at both
//     levels, byte for byte (11 item 2); B4's transform of the display form gives the report form back;
//   - the report's own bytes are those of p3-07-report.txt at both levels for every case it records (11 item 2:
//     this stage leaves them unchanged);
//   - the summary flag is the report's summary chronicle, and the index's digest is history_report_digest's;
//   - select is ascending and its summary list is the summary chronicle (11 item 3);
//   - event_place equals the places of markers_at for every event (item 6).
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "browser_support.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

using namespace brtest;

std::string report_text(const Opened& o, HistoryLevel level) {
    auto r = wi::build_history_report(o.reader, level);
    REQUIRE_MESSAGE(r.ok(), "build_history_report failed: " << (r.ok() ? "" : r.error().message));
    return std::move(r).value();
}

// The event ids of the [chronicle] lines ("<span> e<id> <key>/<salience>: ...").
std::vector<std::int64_t> chronicle_ids(const std::string& text) {
    std::vector<std::int64_t> out;
    for (const std::string& l : hrtest::block(text, "[chronicle]")) {
        if (l.empty() || l[0] < '0' || l[0] > '9') continue;
        const std::size_t sp = l.find(" e");
        if (sp == std::string::npos) continue;
        const std::size_t end = l.find(' ', sp + 2);
        out.push_back(std::stoll(l.substr(sp + 2, end - sp - 2)));
    }
    return out;
}

// A link's text names its target: the participant's name as stored or with "The " lower-cased, a race's name,
// plural or adjective, a monster's noun (form kind), a region's name at the event or the one before it.
bool link_names_target(const wh::LoadedHistory& h, std::int64_t event, std::string_view got, const brapi::TextLink& l) {
    auto same = [&](const std::string* n) {
        if (n == nullptr) return false;
        if (got == *n) return true;
        return n->rfind("The ", 0) == 0 && got.size() == n->size() && got.substr(0, 4) == "the " &&
               got.substr(4) == std::string_view(*n).substr(4);
    };
    const auto type = wh::entity_type_from_code(l.type);
    if (!type) return false;
    if (*type == wh::EntityType::race) {
        return same(h.view.name(*type, l.id)) || same(h.view.race_plural(l.id)) || same(h.view.race_adjective(l.id));
    }
    if (*type == wh::EntityType::region) {
        return same(h.view.region_name_at_event(l.id, event)) || same(h.view.region_name_at_event(l.id, event - 1));
    }
    if (*type == wh::EntityType::monster && same(h.view.monster_noun(l.id))) return true;
    return same(h.view.name(*type, l.id));
}

void check_case(const std::string& name, const std::string& path, bool p307_golden) {
    CAPTURE(name);
    const Opened& o = opened(path);
    const std::int64_t n = event_count(o.h);
    const wh::HistoryDigest digest = brapi::history_report_digest(o.h);
    CHECK(o.index.digest().summary == digest.summary);
    CHECK(o.index.digest().summary_threshold == digest.summary_threshold);
    CHECK(o.index.digest().summary_events == digest.summary_events);

    // item 1 and invariant 1
    for (std::int64_t e = 1; e <= n; ++e) {
        CAPTURE(e);
        const brapi::LinkedText t = brapi::render_event_linked(o.h.view, e);
        const std::string plain = wh::render_event(o.h.view, e);
        REQUIRE(t.text == plain);
        CHECK(links_well_formed(t));
        for (const brapi::TextLink& l : t.links) {
            CHECK(l.length > 0U);
            CHECK_FALSE(l.tagged);
            const std::string_view got = std::string_view(t.text).substr(l.start, l.length);
            CHECK_MESSAGE(link_names_target(o.h, e, got, l), "link '" << got << "' to " << l.type << "/" << l.id);
        }
        const brapi::EventDetail d = o.index.event_detail(o.h, e);
        CHECK(d.sentence == t);
        const std::string note = wh::history_note(o.h.view, e, 0);
        CHECK(d.note == note);
        const wh::HistoryView::Event* ev = o.h.view.event(e);
        REQUIRE(ev != nullptr);
        const std::string span = ev->row.year == ev->row.end_year
                                     ? num(ev->row.year)
                                     : num(ev->row.year) + "-" + num(ev->row.end_year);
        CHECK(o.index.row_text(o.h, e) == span + "  " + plain + (note.empty() ? "" : " (" + note + ")"));
        CHECK(d.entry.id == e);
        CHECK(d.entry.year == ev->row.year);
        CHECK(d.entry.end_year == ev->row.end_year);
        CHECK(d.entry.kind == ev->row.kind);
        CHECK(d.entry.salience == ev->row.salience);
        CHECK(d.entry.region == ev->row.region);
        CHECK(d.entry.settlement == ev->row.settlement);
        CHECK(d.entry.summary == (digest.summary[static_cast<std::size_t>(e - 1)] == 1));
        CHECK(d.entry.group == brapi::event_group(kind_key(o.h, e)));
        CHECK(d.kind_key == kind_key(o.h, e));
        CHECK(d.causes == ev->causes);
        const auto eff = o.h.timeline.effects(e);
        CHECK(d.effects == std::vector<std::int64_t>(eff.begin(), eff.end()));
        CHECK(d.participants.size() == ev->participants.size());
        CHECK(d.values.size() == ev->values.size());
    }
    REQUIRE(o.index.events().size() == static_cast<std::size_t>(n));

    // item 3 and invariant 2, both levels; the summary flag and select's summary list
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        CAPTURE(static_cast<int>(level));
        const std::string text = report_text(o, level);
        if (p307_golden) {
            const hrtest::GoldenCase& g = hrtest::golden_case(name);
            const char* key = level == HistoryLevel::summary ? "summary" : "full";
            const auto it = g.report.find(key);
            REQUIRE(it != g.report.end());
            CHECK(sha(text) == it->second);
        }
        std::vector<std::string> got;
        for (const brapi::CivilizationEntry& c : o.index.civilizations()) {
            const std::vector<brapi::BlockLine> lines = brapi::civilization_block(o.h, digest, c.id, level);
            REQUIRE_FALSE(lines.empty());
            CHECK(brapi::block_line_report_text(o.h, lines[0]) ==
                  "c" + num(c.id) + " " + wi::escape_history_text(c.name));
            for (const brapi::BlockLine& l : lines) {
                const std::string report_form = brapi::block_line_report_text(o.h, l);
                got.push_back(trailing_spaces_escaped(report_form));
                const brapi::LinkedText disp = brapi::block_line_display(o.h, l);
                CHECK(links_well_formed(disp));
                CHECK_MESSAGE(b4_report_form(disp) == report_form, "display: " << disp.text);
            }
        }
        if (o.index.civilizations().empty()) got.push_back("(none)");
        const std::vector<std::string> want = hrtest::block(text, "[civilizations]");
        CHECK(got.size() == want.size());
        for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i) {
            CAPTURE(i);
            CHECK(got[i] == want[i]);
        }
        if (level == HistoryLevel::summary) {
            const std::vector<std::int64_t> shown = chronicle_ids(text);
            std::vector<std::int64_t> flagged;
            for (const brapi::EventEntry& e : o.index.events()) {
                if (e.summary) flagged.push_back(e.id);
            }
            CHECK(flagged == shown);
            CHECK(select(o, brapi::EventFilter{}) == shown);
        } else {
            std::vector<std::int64_t> all;
            for (std::int64_t e = 1; e <= n; ++e) all.push_back(e);
            CHECK(chronicle_ids(text) == all);
            CHECK(select(o, full_filter()) == all);
        }
    }

    // invariant 3 (and B3 of the headless check): a civilization's full list is its timeline list
    for (const brapi::CivilizationEntry& c : o.index.civilizations()) {
        brapi::EventFilter f = full_filter();
        f.civilization = c.id;
        const auto want = o.h.timeline.events_of_civilization(c.id);
        CHECK(select(o, f) == std::vector<std::int64_t>(want.begin(), want.end()));
        CHECK(c.events == static_cast<std::int64_t>(want.size()));
    }

    // item 6: event_place against the places of markers_at in the event's first year (an event of year E has
    // no place: YearState::events is empty at E)
    std::map<std::int64_t, std::vector<std::int64_t>> by_year;
    for (std::int64_t e = 1; e <= n; ++e) by_year[o.h.view.event(e)->row.year].push_back(e);
    const std::int64_t E = o.h.tables.end_year;
    for (const auto& [y, ids] : by_year) {
        std::map<std::int64_t, std::array<std::int64_t, 2>> at;
        if (y < E) {
            const wi::YearMarkers m = o.markers.markers_at(o.h, y, mktest::ALL);
            for (const wi::PlaceMarker& p : m.places) {
                for (const std::int64_t e : p.events) at[e] = {p.x, p.y};
            }
        }
        for (const std::int64_t e : ids) {
            CAPTURE(e);
            const auto p = brapi::event_place(o.markers, o.h, e);
            const auto it = at.find(e);
            if (y >= E) continue;
            if (it == at.end()) {
                CHECK_FALSE(p.has_value());
            } else {
                REQUIRE(p.has_value());
                CHECK(*p == it->second);
            }
        }
    }
}

// The cases whose report hashes p3-07-report.txt records (the P3-07 fixtures and the world cases).
bool in_p307_golden(const std::string& name) {
    const auto it = hrtest::golden().find(name);
    return it != hrtest::golden().end() && it->second.report.count("summary") == 1 &&
           it->second.report.count("full") == 1;
}

}  // namespace

TEST_SUITE("browser") {

TEST_CASE("P4-05 10.2 items 1, 3, 6 and 11 items 1 to 3 on every fixture case") {
    for (const std::string& name : fixture_names()) check_case(name, fixture_world(name), in_p307_golden(name));
}

TEST_CASE("P4-05 10.2 items 1, 3, 6 and 11 items 1 to 3 on the 512 world cases") {
    for (const tltest::WorldSpec& spec : tltest::world_cases_512()) {
        check_case(spec.name, tltest::world(spec), in_p307_golden(spec.name));
    }
}

}  // TEST_SUITE("browser")

TEST_SUITE("browser-slow" * doctest::skip()) {

TEST_CASE("P4-05 10.2 items 1, 3, 6 and 11 items 1 to 3 on the 1024 world cases") {
    for (const tltest::WorldSpec& spec : tltest::world_cases_1024()) {
        check_case(spec.name, tltest::world(spec), in_p307_golden(spec.name));
    }
}

}  // TEST_SUITE("browser-slow")
