// The generator engineer's checks of spec P4-05 Part A on one generated world:
// linked sentences against render_event, the report's [civilizations] blocks
// against the block lines, the browser index's lists, the kind groups and
// event_place. The stage's tests are the test writer's browser_* files.
//
// The skipped suite "browser-bench" prints the costs of 4.7, run by hand:
//
//     worldgen-tests -ts=browser-bench --no-skip
//
// Wall-clock time is printed only, never stored and never a pass condition.

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "timeline_support.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/history_report.hpp"
#include "worldgen/inspect/markers.hpp"

namespace {

using namespace tltest;
namespace hb = worldgen::inspect;

// The report's trailing-space rule (P3-07 6.1).
std::string report_line(std::string text) {
    std::size_t keep = text.size();
    while (keep > 0 && text[keep - 1] == ' ') --keep;
    std::string out = text.substr(0, keep);
    for (std::size_t i = keep; i < text.size(); ++i) out += "\\x20";
    return out + "\n";
}

// The text of a report between "[civilizations]\n" and "[present]\n".
std::string civilizations_section(const std::string& report) {
    const std::size_t a = report.find("\n[civilizations]\n");
    const std::size_t b = report.find("\n[present]\n");
    REQUIRE(a != std::string::npos);
    REQUIRE(b != std::string::npos);
    const std::size_t from = a + std::string_view("\n[civilizations]\n").size();
    return report.substr(from, b + 1 - from);
}

// B4's transform: escape outside tagged links, a tagged link t becomes
// escape(t) + " (" + tag + ")".
std::string back_to_report(const wh::LinkedText& t) {
    std::string out;
    std::size_t at = 0;
    for (const wh::TextLink& l : t.links) {
        if (!l.tagged) continue;
        out += wi::escape_history_text(std::string_view(t.text).substr(at, l.start - at));
        out += wi::escape_history_text(std::string_view(t.text).substr(l.start, l.length)) + " (" +
               wi::history_entity_tag(l.type, l.id) + ")";
        at = l.start + l.length;
    }
    return out + wi::escape_history_text(std::string_view(t.text).substr(at));
}

void check_links(const wh::LinkedText& t) {
    std::uint64_t end = 0;
    for (const wh::TextLink& l : t.links) {
        CHECK(l.start >= end);
        CHECK(l.length > 0U);
        end = std::uint64_t{l.start} + l.length;
        CHECK(end <= t.text.size());
    }
}

const WorldSpec& world_512() { return world_cases_512()[2]; }  // geo-seed-42-512

}  // namespace

TEST_CASE("P4-05 engineer: render_event_linked") {
    const db::WorldReader r = open_reader(world(world_512()));
    const wh::LoadedHistory h = load(r);
    std::size_t links = 0;
    for (const wh::HistoryView::Event& e : h.view.events()) {
        const wh::LinkedText t = wh::render_event_linked(h.view, e.row.id);
        REQUIRE(t.text == wh::render_event(h.view, e.row.id));
        check_links(t);
        for (const wh::TextLink& l : t.links) {
            ++links;
            CHECK_FALSE(l.tagged);
            const std::string_view s = std::string_view(t.text).substr(l.start, l.length);
            const auto type = static_cast<wh::EntityType>(l.type);
            const std::string* name = nullptr;
            if (type == wh::EntityType::race) {
                const std::string* p = h.view.race_plural(l.id);
                const std::string* a = h.view.race_adjective(l.id);
                CHECK(((p != nullptr && s == *p) || (a != nullptr && s == *a)));
                continue;
            }
            if (type == wh::EntityType::region) {
                // Its name at the event or (region_former) just before it.
                const std::string* now = h.view.region_name_at_event(l.id, e.row.id);
                const std::string* before = h.view.region_name_at_event(l.id, e.row.id - 1);
                const auto same = [&s](const std::string* n) {
                    return n != nullptr && (s == *n || (n->rfind("The ", 0) == 0 && s == "the " + n->substr(4)));
                };
                CHECK((same(now) || same(before)));
                continue;
            }
            if (type == wh::EntityType::monster) {
                const std::string* noun = h.view.monster_noun(l.id);
                name = h.view.name(type, l.id);
                // P5-03a 7.2: a host's name ("The Dead of ...") mid-sentence.
                CHECK(((noun != nullptr && s == *noun) ||
                       (name != nullptr && (s == *name || (name->rfind("The ", 0) == 0 && s == "the " + name->substr(4))))));
                continue;
            }
            name = h.view.name(type, l.id);
            REQUIRE(name != nullptr);
            CHECK((s == *name || (name->rfind("The ", 0) == 0 && s == "the " + name->substr(4))));
        }
    }
    CHECK(links > h.view.events().size());
    CHECK_THROWS_AS((void)wh::render_event_linked(h.view, 0), worldgen::FatalError);
}

TEST_CASE("P4-05 engineer: block lines print the report's [civilizations]") {
    const db::WorldReader r = open_reader(world(world_512()));
    const wh::LoadedHistory h = load(r);
    const wh::HistoryDigest d = wi::history_report_digest(h.view, h.tables);
    for (const wi::HistoryLevel level : {wi::HistoryLevel::summary, wi::HistoryLevel::full}) {
        const auto report = wi::build_history_report(r, level);
        REQUIRE(report.ok());
        std::string text;
        for (const db::CivilizationRow& c : h.tables.civilizations) {
            const std::vector<wi::BlockLine> lines = wi::civilization_block(h.view, h.tables, d, c.id, level);
            REQUIRE_FALSE(lines.empty());
            for (const wi::BlockLine& l : lines) {
                const std::string report_form = wi::block_line_report_text(h.view, l);
                text += report_line(report_form);
                const wh::LinkedText shown = wi::block_line_display(h.view, l);
                check_links(shown);
                CHECK(back_to_report(shown) == report_form);
            }
        }
        CHECK(text == civilizations_section(report.value()));
    }
    CHECK_THROWS_AS((void)wi::civilization_block(h.view, h.tables, d, 0, wi::HistoryLevel::full), worldgen::FatalError);
}

TEST_CASE("P4-05 engineer: kind groups") {
    for (const wh::KindDef& k : wh::compiled_event_catalogue().kinds) {
        const hb::EventGroup g = hb::event_group(k.key);
        CHECK_MESSAGE(g != hb::EventGroup::other, k.key);
        // P5-02 kinds: the monsters group of section 9 (lead's ruling at the P4-05 integration); P5-03 (lead ruling
        // 4): kind 56 in monsters, kinds 57 to 62 in the group peoples
        CHECK_MESSAGE((g == hb::EventGroup::monsters) == ((k.id >= 44 && k.id <= 56) || k.id == 88 || (k.id >= 94 && k.id <= 96) || k.id == 99), k.key);   // P5-06 11.3: 88 in monsters; P5-07 11.1: 94 to 96 and 99
        CHECK_MESSAGE((g == hb::EventGroup::peoples) == (k.id >= 57 && k.id <= 62), k.key);   // P5-04: 63 on, 11.2
    }
    CHECK(hb::event_group("") == hb::EventGroup::other);
    CHECK(hb::event_group("warx") == hb::EventGroup::other);
    CHECK(hb::event_group("plague_outbreak") == hb::EventGroup::calamity);
    CHECK(hb::event_group_word(hb::EventGroup::settlements) == "settlements");
}

TEST_CASE("P4-05 engineer: the browser index") {
    const db::WorldReader r = open_reader(world(world_512()));
    const wh::LoadedHistory h = load(r);
    auto built = hb::BrowserIndex::build(h);
    REQUIRE(built.ok());
    const hb::BrowserIndex& b = built.value();
    REQUIRE(b.events().size() == h.view.events().size());
    REQUIRE(b.civilizations().size() == h.tables.civilizations.size());
    std::vector<std::int64_t> ids;
    hb::EventFilter f;
    b.select(h, f, ids);
    CHECK(static_cast<std::int64_t>(ids.size()) == b.digest().summary_events);
    f.summary_only = false;
    b.select(h, f, ids);
    CHECK(ids.size() == h.view.events().size());
    for (const hb::CivilizationEntry& c : b.civilizations()) {
        f.civilization = c.id;
        b.select(h, f, ids);
        CHECK(static_cast<std::int64_t>(ids.size()) == c.events);
        const auto block = b.block(h, c.id, wi::HistoryLevel::summary);
        REQUIRE_FALSE(block.empty());
        CHECK(block.front().text == "c" + num(c.id) + " " + c.name);
    }
    // Two entity fields: the settlement's events of the civilization.
    f.civilization = 1;
    f.settlement = 1;
    b.select(h, f, ids);
    for (const std::int64_t id : ids) {
        CHECK(std::binary_search(h.timeline.events_of_settlement(1).begin(), h.timeline.events_of_settlement(1).end(), id));
        CHECK(std::binary_search(h.timeline.events_of_civilization(1).begin(), h.timeline.events_of_civilization(1).end(), id));
    }
    f = hb::EventFilter{};
    f.summary_only = false;
    f.from_year = 100;
    f.to_year = 200;
    f.groups = 1U << static_cast<unsigned>(hb::EventGroup::war);
    b.select(h, f, ids);
    for (const std::int64_t id : ids) {
        const hb::EventEntry& e = b.events()[static_cast<std::size_t>(id - 1)];
        CHECK(e.year <= 200);
        CHECK(e.end_year >= 100);
        CHECK(e.group == hb::EventGroup::war);
    }
    f.civilization = 1000000;
    CHECK_THROWS_AS(b.select(h, f, ids), worldgen::FatalError);
    for (const wh::HistoryView::Event& e : h.view.events()) {
        const hb::EventDetail d = b.event_detail(h, e.row.id);
        CHECK(d.sentence.text == wh::render_event(h.view, e.row.id));
        CHECK(d.participants.size() == e.participants.size());
        CHECK(d.values.size() == e.values.size());
        const std::string row = b.row_text(h, e.row.id);
        CHECK(row.find(d.sentence.text) != std::string::npos);
    }
    CHECK_THROWS_AS((void)b.event_detail(h, 0), worldgen::FatalError);
    CHECK_THROWS_AS((void)b.block(h, 0, wi::HistoryLevel::full), worldgen::FatalError);
    CHECK(b.search(h, "", 50).empty());
    const std::string first = b.civilizations().front().name;
    std::string q = first.substr(0, 3);
    for (char& c : q) c = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    const auto found = b.search(h, q, 50);
    bool has = false;
    for (const hb::NameMatch& m : found) has = has || (m.type == 3 && m.id == b.civilizations().front().id);
    CHECK(has);
    CHECK(b.search(h, "a", 3).size() <= 3U);
}

TEST_CASE("P4-05 engineer: event_place equals the places of markers_at") {
    const db::WorldReader r = open_reader(world(world_512()));
    const wh::LoadedHistory h = load(r);
    auto loaded = wi::MarkerIndex::load(r, h);
    REQUIRE(loaded.ok());
    const wi::MarkerIndex& m = loaded.value();
    for (const std::int64_t y : {std::int64_t{0}, h.tables.end_year / 3, h.tables.end_year / 2, h.tables.end_year}) {
        const wi::YearMarkers ym = m.markers_at(h, y, wi::MarkerOptions{});
        for (const wi::PlaceMarker& p : ym.places) {
            for (const std::int64_t e : p.events) {
                const auto at = m.event_place(h, e);
                REQUIRE(at.has_value());
                CHECK((*at)[0] == p.x);
                CHECK((*at)[1] == p.y);
            }
        }
    }
    CHECK_THROWS_AS((void)m.event_place(h, 0), worldgen::FatalError);
}

TEST_SUITE("browser-bench" * doctest::skip()) {

TEST_CASE("P4-05 4.7: the browser's costs") {
    using Clock = std::chrono::steady_clock;
    const auto ms_since = [](Clock::time_point t) {
        return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
    };
    // The large world of 7: seed 42 at 1024, dense, turbulent, 4 civilizations per race.
    worldgen::WorldParams p;
    p.seed = 42;
    p.map_size = 1024;
    p.content_hash = wd::content_hash(shipped());
    p.history.races = six_races();
    p.history.detail = wd::HistoryDetail::dense;
    p.history.event_intensity = wd::EventIntensity::turbulent;
    p.history.civs_per_race = 4;
    p.history.span = 12000;
    p.threads = 1;
    const std::string big = detail::scratch().file("browser-bench-large-1024.db");
    REQUIRE(worldgen::generate_world(p, shipped(), big, db::Overwrite::replace).ok());
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"sample-42-1024", world(world_cases_1024()[3])}, {"large-42-1024", big}};
    for (int run = 1; run <= 2; ++run)
    for (const auto& [name, path] : cases) {
        WorldSpec spec;
        spec.name = name;
        const db::WorldReader r = open_reader(path);
        const wh::LoadedHistory h = load(r);
        Clock::time_point t0 = Clock::now();
        const wh::HistoryDigest plain = wh::digest_history(h.view, h.tables);
        const double digest_ms = ms_since(t0);
        t0 = Clock::now();
        const wh::HistoryDigest d = wi::history_report_digest(h.view, h.tables);
        const double report_digest_ms = ms_since(t0);
        t0 = Clock::now();
        auto built = hb::BrowserIndex::build(h);
        const double build_ms = ms_since(t0);
        REQUIRE(built.ok());
        const hb::BrowserIndex& b = built.value();
        std::vector<std::int64_t> ids;
        double select_max = 0;
        hb::EventFilter f;
        f.summary_only = false;
        for (int pass = 0; pass < 3; ++pass) {
            t0 = Clock::now();
            b.select(h, f, ids);
            select_max = std::max(select_max, ms_since(t0));
        }
        for (const hb::CivilizationEntry& c : b.civilizations()) {
            f.civilization = c.id;
            t0 = Clock::now();
            b.select(h, f, ids);
            select_max = std::max(select_max, ms_since(t0));
        }
        double detail_max = 0, row_max = 0, rows_all = 0;
        std::size_t sink = 0;
        t0 = Clock::now();
        for (const hb::EventEntry& e : b.events()) {
            const Clock::time_point c0 = Clock::now();
            sink += b.event_detail(h, e.id).sentence.text.size();
            detail_max = std::max(detail_max, ms_since(c0));
        }
        const double details_all = ms_since(t0);
        t0 = Clock::now();
        for (const hb::EventEntry& e : b.events()) {
            const Clock::time_point c0 = Clock::now();
            sink += b.row_text(h, e.id).size();
            row_max = std::max(row_max, ms_since(c0));
        }
        rows_all = ms_since(t0);
        double block_max = 0;
        for (const hb::CivilizationEntry& c : b.civilizations()) {
            t0 = Clock::now();
            sink += b.block(h, c.id, wi::HistoryLevel::full).size();
            block_max = std::max(block_max, ms_since(t0));
        }
        double search_max = 0;
        for (const std::string_view q : {"a", "the", "THE", "zzqx", "or"}) {
            t0 = Clock::now();
            sink += b.search(h, q, 50).size();
            search_max = std::max(search_max, ms_since(t0));
        }
        t0 = Clock::now();
        sink += wi::build_history_report(r, wi::HistoryLevel::summary).value().size();
        const double report_summary_ms = ms_since(t0);
        t0 = Clock::now();
        sink += wi::build_history_report(r, wi::HistoryLevel::full).value().size();
        const double report_full_ms = ms_since(t0);
        std::cout << "browser-bench: run " << run << ", " << spec.name << ": " << b.events().size() << " events, "
                  << b.civilizations().size() << " civilizations\n"
                  << "browser-bench:   digest_history " << digest_ms << " ms, history_report_digest "
                  << report_digest_ms << " ms\n"
                  << "browser-bench:   BrowserIndex::build " << build_ms << " ms (budget 10)\n"
                  << "browser-bench:   select, slowest " << select_max << " ms (budget 1)\n"
                  << "browser-bench:   event_detail slowest " << detail_max << " ms, all " << details_all
                  << " ms (budget 0.1 each)\n"
                  << "browser-bench:   row_text slowest " << row_max << " ms, all " << rows_all << " ms\n"
                  << "browser-bench:   block (full) slowest " << block_max << " ms (budget 1)\n"
                  << "browser-bench:   search slowest " << search_max << " ms (budget 2)\n"
                  << "browser-bench:   report summary " << report_summary_ms << " ms, full " << report_full_ms
                  << " ms\n"
                  << "browser-bench:   (checksum " << sink << ", " << plain.summary_events << ", " << d.summary_events
                  << ")\n";
    }
}

}  // TEST_SUITE
