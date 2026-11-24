// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 11: the history report's format v5 (17) on
// fixture H14 (worldgen/tests/data/history-report/h14-v5.toml, the P3-07 format with the tables race_relation and
// settlement_race, beast rows with bold_phrases and monster_kind rows with raid_phrases): the header at both
// levels; [peoples] after [sites], its count lines from the rows, a pair line per pair (full) or per turned pair
// (summary), a settlement line per mixed settlement (full); [monsters]' ended lines at summary (10.2); the
// [stats] keys of 17.4 at the end of the full list; and the whole text against p3-07-report.txt's `h14-v5`
// lines (written by p3_report_reference.py). The checks are from the spec, not from H14's particular rows, so
// they hold whatever the fixture holds; they are skipped with a message while the fixture is absent.
//
// Written from the spec before the implementation. Library names: build_history_report (P3-07) and the adapter's
// guessed writers StageContext::write_race_relation and write_settlement_race (p5_03_relations_api.hpp).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../history_report_checks.hpp"
#include "../history_report_fixture.hpp"
#include "p5_03_relations_api.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

namespace fs = std::filesystem;
using hrtest::HistoryLevel;

const char* const H14 = "h14-v5";

bool fixture_present() { return fs::exists(fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H14) + ".toml")); }

struct H14Fixture {
    hrtest::Fixture fx;
    std::vector<db::RaceRelationRow> rr;
    std::vector<db::SettlementRaceRow> sr;
};

const H14Fixture*& current() {
    static const H14Fixture* f = nullptr;
    return f;
}

// The P3-07 loader for the tables it knows, then the schema 10 columns and tables (GUESS row members, adapter).
H14Fixture load_h14() {
    H14Fixture out;
    out.fx = hrtest::load_fixture(H14);
    const fs::path path = fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H14) + ".toml");
    const toml::table doc = toml::parse(hrtest::detail::text_of_file(path), path.string());
    using hrtest::detail::i64;
    using hrtest::detail::str;
    out.fx.beasts.clear();
    for (const toml::array* r : hrtest::detail::rows(doc, "beast")) {
        db::BeastRow b;
        b.id = i64(*r->get(0));
        b.key = str(*r->get(1));
        b.name = str(*r->get(2));
        b.plural = str(*r->get(3));
        b.menace = i64(*r->get(4));
        b.food = i64(*r->get(5));
        b.bold_phrases = str(*r->get(6));
        out.fx.beasts.push_back(b);
    }
    out.fx.monster_kinds.clear();
    for (const toml::array* r : hrtest::detail::rows(doc, "monster_kind")) {
        db::MonsterKindRow k;
        k.id = i64(*r->get(0));
        k.key = str(*r->get(1));
        k.name = str(*r->get(2));
        k.noun = str(*r->get(3));
        k.grouped = i64(*r->get(4));
        k.wake = i64(*r->get(5));
        k.might = i64(*r->get(6));
        k.raid_phrases = str(*r->get(7));
        out.fx.monster_kinds.push_back(k);
    }
    for (const toml::array* r : hrtest::detail::rows(doc, "race_relation")) {
        db::RaceRelationRow x;
        x.race_a = i64(*r->get(0));
        x.race_b = i64(*r->get(1));
        x.year = i64(*r->get(2));
        x.disposition = i64(*r->get(3));
        x.band = i64(*r->get(4));
        out.rr.push_back(x);
    }
    for (const toml::array* r : hrtest::detail::rows(doc, "settlement_race")) {
        db::SettlementRaceRow x;
        x.settlement = i64(*r->get(0));
        x.race = i64(*r->get(1));
        x.year = i64(*r->get(2));
        x.share = i64(*r->get(3));
        x.standing = i64(*r->get(4));
        out.sr.push_back(x);
    }
    return out;
}

void stage_biomes(worldgen::stages::StageContext& ctx) {
    for (const auto& r : current()->fx.biomes) ctx.write_biome(r);
}
void stage_regions(worldgen::stages::StageContext& ctx) {
    for (const auto& r : current()->fx.regions) ctx.write_region(r);
}
void stage_chronology(worldgen::stages::StageContext& ctx) {
    const hrtest::Fixture& f = current()->fx;
    for (const auto& r : f.race_rows) ctx.write_race(r);
    for (const auto& r : f.cultures) ctx.write_culture(r);
    for (const auto& r : f.gods) ctx.write_god(r);
    for (const auto& r : f.ages) ctx.write_age(r);
}
void stage_history(worldgen::stages::StageContext& ctx) {
    const hrtest::Fixture& f = current()->fx;
    for (const auto& r : f.kinds) ctx.write_event_kind(r);
    for (const auto& r : f.roles) ctx.write_event_kind_role(r);
    for (const auto& r : f.values) ctx.write_event_kind_value(r);
    for (const auto& r : f.phases) ctx.write_phase(r);
    for (const auto& r : f.events) ctx.write_event(r);
    for (const auto& r : f.participants) ctx.write_event_participant(r);
    for (const auto& r : f.causes) ctx.write_event_cause(r);
    for (const auto& r : f.event_values) ctx.write_event_value(r);
    for (const auto& r : f.civilizations) ctx.write_civilization(r);
    for (const auto& r : f.settlements) ctx.write_settlement(r);
    for (const auto& r : f.census) ctx.write_settlement_census(r);
    for (const auto& r : f.region_control) ctx.write_region_control(r);
    for (const auto& r : f.relations) ctx.write_relation(r);
    for (const auto& r : f.region_names) ctx.write_region_name(r);
    for (const auto& r : f.beasts) ctx.write_beast(r);
    for (const auto& r : f.monster_kinds) ctx.write_monster_kind(r);
    for (const auto& r : f.beast_populations) ctx.write_beast_population(r);
    for (const auto& r : f.monsters) ctx.write_monster(r);
    for (const auto& r : f.sites) ctx.write_site(r);
    for (const auto& r : current()->rr) ctx.write_race_relation(r);        // GUESS (16.2)
    for (const auto& r : current()->sr) ctx.write_settlement_race(r);      // GUESS (16.2)
}

const std::string& h14_world() {
    static const std::string path = [] {
        static const H14Fixture f = load_h14();
        static dbtest::TempDir dir("relations-h14");
        const std::string p = dir.file("h14-v5.db");
        static const worldgen::data::ContentSet empty;
        worldgen::WorldParams wp;
        wp.seed = f.fx.seed;
        wp.map_size = f.fx.map_size;
        wp.content_hash = worldgen::data::content_hash(empty);
        wp.history.span = f.fx.span;
        wp.history.races = f.fx.races;
        wp.threads = 1;
        const worldgen::stages::StageInfo stages[] = {{"biomes", &stage_biomes},
                                                      {"regions", &stage_regions},
                                                      {"chronology", &stage_chronology},
                                                      {"history", &stage_history}};
        current() = &f;
        const db::Status s =
            worldgen::stages::testing::generate_world_with_stages(wp, empty, stages, p, db::Overwrite::replace);
        current() = nullptr;
        REQUIRE_MESSAGE(s.ok(), "fixture h14-v5: " << (s.ok() ? "" : s.error().message));
        return p;
    }();
    return path;
}

std::string text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const auto it = cache.find(static_cast<int>(level));
    if (it != cache.end()) return it->second;
    return cache[static_cast<int>(level)] = hrtest::report_of(h14_world(), level);
}

std::vector<std::string> headings(const std::string& t) {
    std::vector<std::string> out;
    for (const std::string& l : hrtest::lines_of(t)) {
        if (!l.empty() && l.front() == '[' && l.back() == ']' && l.find(' ') == std::string::npos) out.push_back(l);
    }
    return out;
}

// The [peoples] count lines of 17.2 from the fixture's rows.
struct Counts {
    std::string pairs, mixed;
    std::set<std::pair<std::int64_t, std::int64_t>> turned;
    std::int64_t npairs = 0, nmixed = 0;
};

Counts counts() {
    const H14Fixture f = load_h14();
    std::map<std::int64_t, std::string> key;
    for (const auto& k : f.fx.kinds) key[k.id] = k.key;
    std::map<std::int64_t, std::string> kind_of;
    for (const auto& e : f.fx.events) kind_of[e.id] = key[e.kind];
    std::map<std::int64_t, std::vector<std::int64_t>> parts;
    for (const auto& p : f.fx.participants) parts[p.event].push_back(p.entity_id);
    std::map<std::int64_t, std::int64_t> people;
    for (const auto& v : f.fx.event_values) {
        if (v.seq == 1) people[v.event] = v.value;
    }
    std::int64_t present = 0;
    for (const auto& r : f.fx.race_rows) present += r.present != 0 ? 1 : 0;
    Counts c;
    c.npairs = present * (present - 1) / 2;
    std::int64_t met = 0, arrivals = 0, arrived = 0, expelled = 0, gone = 0;
    for (const auto& [e, k] : kind_of) {
        if (k == "peoples_met") ++met;
        if (k == "disposition_turned") c.turned.insert({parts[e].at(0), parts[e].at(1)});
        if (k == "refugees_arrived") {
            ++arrivals;
            arrived += people[e];
        }
        if (k == "people_expelled") {
            ++expelled;
            gone += people[e];
        }
    }
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> band;
    for (const auto& r : f.rr) band[{r.race_a, r.race_b}] = r.band;   // ascending year: the last wins
    std::int64_t b[3] = {0, 0, 0};
    for (const auto& [p, v] : band) ++b[v];
    c.pairs = "pairs: " + std::to_string(c.npairs) + "; met " + std::to_string(met) + "; hatred " + std::to_string(b[0]) +
              ", wariness " + std::to_string(b[1]) + ", friendship " + std::to_string(b[2]);
    // mixed at E: standing settlements with a non-ruling race at >= mixed_min_permille (100 shipped; the fixture
    // carries no tuning, reading R-H1: the report reads the threshold of the shipped data)
    std::map<std::int64_t, std::int64_t> status;
    for (const auto& s : f.fx.settlements) status[s.id] = s.status;
    std::map<std::int64_t, std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>>> last;
    for (const auto& r : f.sr) last[r.settlement][r.race] = {r.share, r.standing};
    for (const auto& [s, es] : last) {
        if (status[s] != 1 && status[s] != 2) continue;
        bool mixed = false;
        for (const auto& [g, v] : es) mixed = mixed || (v.first >= 100 && v.second != 3);
        c.nmixed += mixed ? 1 : 0;
    }
    // lead ruling 3 (integration): "1 settlement" singular
    c.mixed = "mixed: " + std::to_string(c.nmixed) + (c.nmixed == 1 ? " settlement" : " settlements") +
              " at E; refugees: " + std::to_string(arrivals) +
              " arrivals, " + std::to_string(arrived) + " people; expelled: " + std::to_string(expelled) + " times, " +
              std::to_string(gone) + " people";
    return c;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 17.1: the header is `# worldgen history v5` at both levels; the text equals the reference's") {
    if (!fixture_present()) {
        MESSAGE("h14-v5.toml absent: skipped (written with the reference)");
        return;
    }
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        const std::string t = text(level);
        CHECK(hrtest::lines_of(t).front() == "# worldgen history v5");
        const std::string name = level == HistoryLevel::full ? "full" : "summary";
        const auto& g = hrtest::golden();
        const auto it = g.find(H14);
        if (it == g.end() || it->second.report.count(name) == 0U) {
            MESSAGE("p3-07-report.txt has no h14-v5 " << name << " line yet");
            continue;
        }
        CHECK(worldgen::to_hex(worldgen::sha256(t)) == it->second.report.at(name));
    }
}

TEST_CASE("P5-03 17.2: [peoples] after [sites]; count lines, pair lines and settlement lines at both levels") {
    if (!fixture_present()) {
        MESSAGE("h14-v5.toml absent: skipped");
        return;
    }
    const Counts c = counts();
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        const bool full = level == HistoryLevel::full;
        CAPTURE(full);
        const std::string t = text(level);
        const std::vector<std::string> hs = headings(t);
        const auto sites = std::find(hs.begin(), hs.end(), "[sites]");
        REQUIRE(sites != hs.end());
        REQUIRE(sites + 1 != hs.end());
        CHECK(*(sites + 1) == "[peoples]");
        const std::vector<std::string> b = hrtest::block(t, "[peoples]");
        REQUIRE(b.size() >= 2U);
        CHECK(b[0] == c.pairs);
        std::size_t mixed_at = 0;
        for (std::size_t i = 0; i < b.size(); ++i) {
            if (b[i].rfind("mixed: ", 0) == 0) mixed_at = i;
        }
        REQUIRE(mixed_at >= 1U);
        CHECK(b[mixed_at] == c.mixed);
        const std::size_t pair_lines = mixed_at - 1U, stl_lines = b.size() - mixed_at - 1U;
        if (full) {
            CHECK(static_cast<std::int64_t>(pair_lines) == c.npairs);
            CHECK(static_cast<std::int64_t>(stl_lines) == c.nmixed);
        } else {
            CHECK(pair_lines == c.turned.size());
            CHECK(stl_lines == 0U);
        }
        for (std::size_t i = 1; i < mixed_at; ++i) {
            const std::string& l = b[i];
            CAPTURE(l);
            CHECK(l.rfind("  ", 0) == 0U);
            CHECK((l.find(": never met") != std::string::npos || l.find(", met ") != std::string::npos));
            if (!full) CHECK(l.find(", turned ") != std::string::npos);
        }
        for (std::size_t i = mixed_at + 1; i < b.size(); ++i) {
            CHECK(b[i].rfind("  s", 0) == 0U);
        }
    }
}

TEST_CASE("P5-03 17.4: the [stats] keys appended at the end of the full list, in order; none at summary") {
    if (!fixture_present()) {
        MESSAGE("h14-v5.toml absent: skipped");
        return;
    }
    const std::vector<std::string> want = {
        "race_relations", "settlement_races", "race_pairs", "race_pairs_met", "dispositions_turned", "pairs_hatred",
        "pairs_friendship", "refugees_arrived", "refugees_people", "people_expelled", "expelled_people",
        "settlements_mixed", "settlements_mixed_ever", "settlements_fled", "abandoned_without_destination",
        "abandonment_lost", "monster_stories", "ruins_oldest_year"};
    std::vector<std::string> keys;
    for (const std::string& l : hrtest::block(text(HistoryLevel::full), "[stats]")) {
        keys.push_back(l.substr(0, l.find(" = ")));
    }
    // events_<key> lines of the new kinds come with the existing per-kind lines; 17.4's keys close the list
    REQUIRE(keys.size() >= want.size());
    CHECK(std::vector<std::string>(keys.end() - static_cast<std::ptrdiff_t>(want.size()), keys.end()) == want);
    const H14Fixture f = load_h14();
    CHECK(std::find(keys.begin(), keys.end(), "race_relations") != keys.end());
    std::vector<std::string> skeys;
    for (const std::string& l : hrtest::block(text(HistoryLevel::summary), "[stats]")) skeys.push_back(l.substr(0, l.find(" = ")));
    for (const std::string& k : want) CHECK(std::find(skeys.begin(), skeys.end(), k) == skeys.end());
    // the two row counts
    for (const std::string& l : hrtest::block(text(HistoryLevel::full), "[stats]")) {
        if (l.rfind("race_relations = ", 0) == 0) CHECK(l == "race_relations = " + std::to_string(f.rr.size()));
        if (l.rfind("settlement_races = ", 0) == 0) CHECK(l == "settlement_races = " + std::to_string(f.sr.size()));
    }
}

TEST_CASE("P5-03 10.2: [monsters] at summary: living, then at most two ended notable monsters, then `and <x> more`") {
    if (!fixture_present()) {
        MESSAGE("h14-v5.toml absent: skipped");
        return;
    }
    const std::vector<std::string> s = hrtest::block(text(HistoryLevel::summary), "[monsters]");
    const std::vector<std::string> fl = hrtest::block(text(HistoryLevel::full), "[monsters]");
    REQUIRE_FALSE(s.empty());
    std::int64_t listed = 0;
    bool more = false;
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (s[i].rfind("  and ", 0) == 0) {
            more = true;
            continue;
        }
        if (s[i].rfind("  m", 0) == 0) {
            ++listed;
            CHECK(hrtest::contains_line(fl, s[i]));   // each as the full form's line
        }
    }
    CHECK(listed <= 4);   // SUMMARY_MONSTER_LINES 2 + SUMMARY_MONSTER_ENDED_LINES 2
    const H14Fixture f = load_h14();
    const std::int64_t x = static_cast<std::int64_t>(f.fx.monsters.size()) - listed;
    CHECK(more == (x > 0));
    if (x > 0) CHECK(s.back() == "  and " + std::to_string(x) + " more");
}

}  // TEST_SUITE
