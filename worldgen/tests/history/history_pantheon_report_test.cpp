// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) test 16 items 9, 11 and 13 on fixture H20 (h20-v11.toml, written
// by reference/p5_08_h20_fixture.py from the reference's replay, reading P-R16): the v11 header at both levels; every
// line of h20-v11.expected.txt in the report of its level (the [pantheon] and [cults] blocks, the realm lines `gods:`
// and `faith:`, the [stats] lines of 10 item 8 that rows give, and the sentence of the first event of every kind 100 to
// 110); the timeline reads of 11.1 (faith_at, cults_in_at, cult_status_at, divine_effect_at) against the same reads from
// the fixture's rows; the browser group `gods` (9) and ALL_GROUPS; the marker glyphs and shrine_word. Every case starts
// with P508_REQUIRE_INTEGRATED(): the C++ fixture builder (history_report_fixture.hpp) learns the schema 14 tables at
// integration (item 4 of the notes).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../history_report_checks.hpp"
#include "../history_report_fixture.hpp"
#include "p5_08_api.hpp"
#include "p5_08_checks.hpp"
#include "worldgen/history/timeline.hpp"

namespace {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace h = worldgen::history;
using hrtest::HistoryLevel;

const char* const H20 = "h20-v11";

fs::path dir() { return fs::path(UNMAPPED_P3_07_FIXTURE_DIR); }
bool present() { return fs::exists(dir() / (std::string(H20) + ".toml")) && fs::exists(dir() / "h20-v11.expected.txt"); }

const std::string& text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const int k = static_cast<int>(level);
    if (const auto it = cache.find(k); it == cache.end()) cache[k] = hrtest::report_of(hrtest::fixture_world(H20), level);
    return cache[k];
}
std::vector<std::string> lines(HistoryLevel level) { return hrtest::lines_of(text(level)); }

std::vector<std::pair<std::string, std::string>> expected() {
    std::ifstream in(dir() / "h20-v11.expected.txt", std::ios::binary);
    std::vector<std::pair<std::string, std::string>> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::size_t sp = line.find(' ');
        out.emplace_back(line.substr(0, sp), line.substr(sp + 1));
    }
    return out;
}

#define SKIP_UNLESS_PRESENT()                                     \
    if (!present()) {                                             \
        MESSAGE("h20-v11.toml absent: skipped");                  \
        return;                                                   \
    }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-08 10 item 1: the v11 header at both levels") {
    P508_REQUIRE_INTEGRATED();
    SKIP_UNLESS_PRESENT();
    CHECK(lines(HistoryLevel::summary).at(0) == p508api::REPORT_HEADER_SPEC);
    CHECK(lines(HistoryLevel::full).at(0) == p508api::REPORT_HEADER_SPEC);
}

TEST_CASE("P5-08 10, 8.2: the reference's expected lines of H20 at both levels") {
    P508_REQUIRE_INTEGRATED();
    SKIP_UNLESS_PRESENT();
    const std::vector<std::string> full = lines(HistoryLevel::full);
    const std::vector<std::string> summary = lines(HistoryLevel::summary);
    const std::string all = text(HistoryLevel::full);
    std::size_t n = 0;
    for (const auto& [level, want] : expected()) {
        CAPTURE(level);
        CAPTURE(want);
        ++n;
        if (want.rfind("sentence ", 0) == 0) {
            CHECK(all.find(want.substr(9)) != std::string::npos);   // a chronicle line holds the sentence
            continue;
        }
        const std::vector<std::string>& ls = level == "summary" ? summary : full;
        CHECK(std::find(ls.begin(), ls.end(), want) != ls.end());
    }
    CHECK(n > 0U);
}

TEST_CASE("P5-08 11.1: the timeline reads on H20 against the rows") {
    P508_REQUIRE_INTEGRATED();
    SKIP_UNLESS_PRESENT();
    const std::string path = hrtest::fixture_world(H20);
    const db::WorldReader r = hrtest::open_world(path);
    const p508_check::World w = p508_check::world_of_file(r);
    auto lh = h::load_history(r);
    REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
    const h::Timeline& t = lh.value().timeline;
    for (const auto& [c, row] : w.civ) {
        CAPTURE(c);
        for (std::int64_t y = 0; y < w.end_year; y += 50) {
            CAPTURE(y);
            CHECK(p508api::faith_at(t, c, y) == p508_check::faith_at(w, c, y));
            CHECK(p508api::divine_effect_at(t, c, y) == p508_check::divine_effect_at(w, c, y));
        }
    }
    for (const auto& [k, f] : w.faction) {
        CAPTURE(k);
        CHECK(p508api::cult_status_at(t, k, p508_check::I(*f, 7) - 1) == 0);
        CHECK(p508api::cult_status_at(t, k, p508_check::I(*f, 7)) == p508api::STATUS_PRESENT);
        const std::int64_t st = p508_check::I(*f, 12);
        if (st == p508api::STATUS_ENDED) CHECK(p508api::cult_status_at(t, k, p508_check::I(*f, 9)) == p508api::STATUS_ENDED);
        if (st != p508api::STATUS_ENDED) CHECK(p508api::cult_status_at(t, k, w.end_year - 1) == st);
        const std::int64_t c = p508_check::I(*f, 3);
        const std::vector<std::int64_t> in = p508api::cults_in_at(t, c, p508_check::I(*f, 7));
        CHECK(std::find(in.begin(), in.end(), k) != in.end());
    }
}

TEST_CASE("P5-08 11.1: shrine_word, the browser group gods and the marker glyphs") {
    P508_REQUIRE_INTEGRATED();
    CHECK(p508api::shrine_word(false) == "holy to");
    CHECK(p508api::shrine_word(true) == "cursed by");
    CHECK(p508api::GROUP_GODS == 9);
    CHECK(p508api::ALL_GROUPS == 0x3FF);
    CHECK(p508api::GLYPH_SHRINE == 21);
    CHECK(p508api::GLYPH_TEMPLE == 22);
    CHECK(p508api::GLYPH_SANCTUM == 23);
}

}  // TEST_SUITE
