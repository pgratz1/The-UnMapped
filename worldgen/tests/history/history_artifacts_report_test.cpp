// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) test 16 items 10 and 12 on fixture H21 (h21-v12.toml, written by
// reference/p5_11_h21_fixture.py from the reference's replay in H20's shape): the v12 header at both levels; every line
// of h21-v12.expected.txt in the report of its level (the [artifacts] block at both levels, the realm lines
// `artifacts:`, the [stats] lines of 10 item 6 that rows give, the sentence of the first event of every kind 111 to
// 116) and every `part-full` pair in one full line (the [figures] lines' `; bore`/`; made`, the [monsters] lines'
// `, hoard`); the timeline reads of 11.1 on H21 against the same reads from its rows; the browser group `artifacts`
// (10), ALL_GROUPS, the entity tag `ar`, artifact_state_word, MarkerOptions::artifacts and glyph 24. Every case starts
// with P511_REQUIRE_INTEGRATED(): the C++ fixture builder (history_report_fixture.hpp) learns the schema 15 tables at
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
#include "p5_11_api.hpp"
#include "p5_11_checks.hpp"
#include "worldgen/history/timeline.hpp"

namespace {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace h = worldgen::history;
using hrtest::HistoryLevel;

const char* const H21 = "h21-v12";

fs::path dir() { return fs::path(UNMAPPED_P3_07_FIXTURE_DIR); }
bool present() { return fs::exists(dir() / (std::string(H21) + ".toml")) && fs::exists(dir() / "h21-v12.expected.txt"); }

const std::string& text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const int k = static_cast<int>(level);
    if (const auto it = cache.find(k); it == cache.end()) cache[k] = hrtest::report_of(hrtest::fixture_world(H21), level);
    return cache[k];
}
std::vector<std::string> lines(HistoryLevel level) { return hrtest::lines_of(text(level)); }

std::vector<std::pair<std::string, std::string>> expected() {
    std::ifstream in(dir() / "h21-v12.expected.txt", std::ios::binary);
    std::vector<std::pair<std::string, std::string>> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::size_t sp = line.find(' ');
        out.emplace_back(line.substr(0, sp), line.substr(sp + 1));
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-11 10 item 1: the v12 header at both levels") {
    P511_REQUIRE_INTEGRATED();
    REQUIRE(present());
    CHECK(lines(HistoryLevel::summary).at(0) == p511api::REPORT_HEADER_SPEC);
    CHECK(lines(HistoryLevel::full).at(0) == p511api::REPORT_HEADER_SPEC);
}

TEST_CASE("P5-11 10, 8.2: the reference's expected lines of H21 at both levels") {
    P511_REQUIRE_INTEGRATED();
    REQUIRE(present());
    const std::vector<std::string> full = lines(HistoryLevel::full);
    const std::vector<std::string> summary = lines(HistoryLevel::summary);
    const std::string all = text(HistoryLevel::full);
    std::size_t k = 0;
    for (const auto& [level, want] : expected()) {
        CAPTURE(level);
        CAPTURE(want);
        ++k;
        if (want.rfind("sentence ", 0) == 0) {
            CHECK(all.find(want.substr(9)) != std::string::npos);   // a chronicle line holds the sentence
            continue;
        }
        if (level == "part-full") {
            const std::size_t tab = want.find('\t');
            REQUIRE(tab != std::string::npos);
            const std::string needle = want.substr(0, tab), frag = want.substr(tab + 1U);
            const bool found = std::any_of(full.begin(), full.end(), [&](const std::string& l) {
                return l.find(needle) != std::string::npos && l.find(frag) != std::string::npos;
            });
            CHECK(found);
            continue;
        }
        const std::vector<std::string>& ls = level == "summary" ? summary : full;
        CHECK(std::find(ls.begin(), ls.end(), want) != ls.end());
    }
    CHECK(k > 0U);
    // 10 item 2: the block's place, after [cults]
    const std::size_t cults = all.find("[cults]"), arts = all.find("[artifacts]");
    CHECK(arts != std::string::npos);
    CHECK((cults != std::string::npos && cults < arts));
}

TEST_CASE("P5-11 11.1: the timeline reads on H21 against the rows") {
    P511_REQUIRE_INTEGRATED();
    REQUIRE(present());
    const std::string path = hrtest::fixture_world(H21);
    const db::WorldReader r = hrtest::open_world(path);
    const p511_check::World w = p511_check::world_of_file(r);
    auto lh = h::load_history(r);
    REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
    const h::Timeline& t = lh.value().timeline;
    REQUIRE_FALSE(w.art.empty());
    for (const auto& [aid, ap] : w.art) {
        CAPTURE(aid);
        CHECK(p511api::artifact_at(t, aid, p511_check::I(*ap, 9) - 1).state == 0);
        for (const auto* hp : w.hold.at(aid)) {
            const std::int64_t y = p511_check::I(*hp, 2);
            CAPTURE(y);
            const p511api::Place want = p511_check::artifact_at(w, aid, y);
            const p511api::Place got = p511api::artifact_at(t, aid, y);
            CHECK(got.state == want.state);
            CHECK(got.holder == want.holder);
            CHECK(got.event == want.event);
            if (want.state >= p511api::LOST) CHECK(got == want);
        }
    }
    for (std::int64_t y = 0; y < w.base.end_year; y += 100) {
        CHECK(p511api::artifacts_placed(t, y) == p511_check::artifacts_placed(w, y));
    }
}

TEST_CASE("P5-11 11.1: the browser group artifacts, the tag ar, artifact_state_word, the markers") {
    P511_REQUIRE_INTEGRATED();
    for (const auto& [id, key] : p511api::new_kinds()) {
        CAPTURE(key);
        CHECK(p511api::event_group_of(key) == p511api::GROUP_ARTIFACTS);
    }
    CHECK(p511api::all_groups() == p511api::ALL_GROUPS);
    CHECK(p511api::entity_tag(p511api::ENTITY_ARTIFACT, 7) == "ar7");
    static const char* const words[6] = {"kept", "borne", "hoarded", "lost", "buried", "enshrined"};
    for (std::int64_t s = 1; s <= 6; ++s) CHECK(p511api::artifact_state_word(s) == words[s - 1]);
    CHECK(p511api::marker_option_artifacts_default());
    CHECK(p511api::GLYPH_ARTIFACT == 24);
}

}  // TEST_SUITE
