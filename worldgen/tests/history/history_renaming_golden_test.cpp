// Spec P5-12 (docs/specs/p5-12-renamed-regions.md r0) sections 6, 7, 8, 12 and 13 "Property", "Fixtures",
// "Golden": the shipped history of the world cases of worldgen/tests/golden/p5-12-renaming.txt (the conflict
// reference's replay with the renaming subsystem, reference/p5_12_renaming_reference.py) against the file: the
// table region_name (rows, hash --detail's hash, each row's region, year, namer's culture, event, cause and name);
// each row's event of kind 43 (region, year, one cause, the namer as role 1); invariants 1 to 5 of section 8 from
// the rows; invariant 6 (region.name equals the --until regions world's); the fixtures rename-small (synthetic
// stress geographies, every rule min_magnitude 1 and chance 1000, caps opened: the reference's rows) and off
// (region_max 0: no row and no kind 43 event).
//
// geo-seed-42-512 and the fixtures run by default; every other world case is in the suite "renaming-slow".
// Written from the specs before the implementation (test-writer rule).

#include "p5_12_renaming_support.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace renaming_test;

// A stored world of a golden case (shipped data), as history_conflict_golden_test.cpp builds them.
std::string world_of(const RCase& g, const std::string& until = "") {
    const CCase& c = conflict_case(g.name.c_str());
    const bool phrase = c.name == "geo-phrase-512";
    if (c.detail == "normal" && c.civs == "auto" && c.intensity == "normal") {
        return chrono_test::world(
            chrono_test::WorldKey{c.seed, phrase ? "the unmapped" : "", c.size, c.span, c.races, until, 1});
    }
    static std::map<std::string, std::string> cache;
    const std::string key = c.name + "/" + until;
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    chrono_api::Params p = chrono_test::params_with(c.span, chrono_test::races_of_text(c.races));
    p.detail = c.detail;
    p.civs = c.civs == "auto" ? std::nullopt : std::optional<std::int64_t>(std::stoll(c.civs));
    p.intensity = c.intensity;
    const std::string path = chrono_test::world_dir().file("rn" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams wp = chrono_test::world_params(
        c.seed, c.size, p, shipped(), until.empty() ? std::nullopt : std::optional<std::string>(until), 1);
    REQUIRE(worldgen::generate_world(wp, shipped(), path, db::Overwrite::refuse).ok());
    cache[key] = path;
    return path;
}

void check_rows(const RCase& g, const Tables& t) {
    REQUIRE(static_cast<std::int64_t>(t.region_name.size()) == g.rows);
    CHECK(table_hash(5, t.region_name) == g.hash);
    REQUIRE(g.renames.size() == t.region_name.size());
    std::map<std::int64_t, const Row*> ev;
    for (const Row& e : t.event) ev[at(e, 0)] = &e;
    std::map<std::int64_t, std::vector<std::int64_t>> causes;
    for (const Row& c : t.event_cause) causes[at(c, 0)].push_back(at(c, 1));
    std::map<std::int64_t, std::vector<const Row*>> parts;
    for (const Row& p : t.event_participant) parts[at(p, 0)].push_back(&p);
    std::map<std::int64_t, std::int64_t> civ_culture;
    for (const Row& c : t.civilization) civ_culture[at(c, 0)] = at(c, 3);
    for (std::size_t i = 0; i < g.renames.size(); ++i) {
        const RenameRow& w = g.renames[i];
        const Row& r = t.region_name[i];
        CAPTURE(w.name);
        CHECK(at(r, 0) == w.region);
        CHECK(at(r, 1) == w.from_year);
        CHECK(r[2].bytes == w.name);
        CHECK(at(r, 3) == w.culture);
        CHECK(at(r, 4) == w.event);
        // 6 item 3: the event (kind 43, year Y = end_year, region r, one cause e, the namer as role 1)
        REQUIRE(ev.count(w.event) == 1U);
        const Row& e = *ev.at(w.event);
        CHECK(at(e, 3) == k_region_renamed);
        CHECK(at(e, 1) == w.from_year);
        CHECK(at(e, 2) == w.from_year);
        CHECK(at(e, 5) == w.region);
        CHECK(causes[w.event] == std::vector<std::int64_t>{w.cause});
        const std::vector<const Row*>& ps = parts[w.event];
        CHECK(ps.size() <= 1U);
        if (ps.size() == 1U) {
            CHECK(at(*ps[0], 2) == 1);   // role 1 namer
            CHECK(at(*ps[0], 3) == 3);   // civilization
            CHECK(civ_culture[at(*ps[0], 4)] == w.culture);
        } else {
            CHECK(w.culture == 0);
        }
        // the settlement: e's settlement when it lies in r, else 0
        const Row& c = *ev.at(w.cause);
        if (at(e, 6) != 0) {
            CHECK(at(e, 6) == at(c, 6));
        }
    }
}

void check_world_case(const RCase& g) {
    CAPTURE(g.name);
    const db::WorldReader r = open_world(world_of(g));
    const Tables t = tables_of_world(r);
    check_rows(g, t);
    const std::int64_t regions = static_cast<std::int64_t>(rows_of(r, "region").size());
    CHECK(renaming_violations(t, tuning_of(shipped()), regions, other_names_of(r, t), geo_names_of(r)) ==
          std::vector<std::string>{});
    // invariant 6: region.name is the geography's (the world generated up to regions)
    const db::WorldReader geo = open_world(world_of(g, "regions"));
    CHECK(rows_of(geo, "region") == rows_of(r, "region"));
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-12 12, 13: the golden file lists every world case of p3-06-conflict.txt and the fixtures") {
    std::set<std::string> worlds;
    for (const RCase& c : renaming_golden().cases) {
        if (c.kind == "world") worlds.insert(c.name);
    }
    std::set<std::string> conflict;
    for (const CCase& c : conflict_golden()) {
        if (c.kind == "world") conflict.insert(c.name);
    }
    CHECK(worlds == conflict);
    // every row of the golden worlds also in p3-06-conflict.txt's r21 line
    for (const RCase& c : renaming_golden().cases) {
        if (c.kind != "world") continue;
        CAPTURE(c.name);
        const CCase& cc = conflict_case(c.name.c_str());
        std::string want = "renamings " + std::to_string(c.rows) + " ";
        std::string items;
        for (const RenameRow& r : c.renames) {
            items += (items.empty() ? "" : ",") + std::to_string(r.region) + ":" + std::to_string(r.from_year) + ":" +
                     r.rule + ":e" + std::to_string(r.cause);
        }
        CHECK(cc.r21 == want + (items.empty() ? "-" : items));
        CHECK(cc.tables.at("region_name") == c.hash);
    }
}

TEST_CASE("P5-12 13: geo-seed-42-512 against the golden file; invariants 1 to 6") {
    check_world_case(renaming_case("geo-seed-42-512"));
}

TEST_CASE("P5-12 13 fixtures: rename-small (each rule renames; the reference's rows) and off (region_max 0)") {
    std::set<std::string> rules;
    for (const RCase& g : renaming_golden().cases) {
        if (g.kind != "fixture") continue;
        CAPTURE(g.name);
        CCase c;
        c.kind = "stress";
        c.name = g.name;
        c.seed = g.seed;
        c.span = g.span;
        c.races = g.races;
        c.civs = g.civs;
        c.intensity = g.intensity;
        const wd::ContentSet& content = g.data == "off" ? off_content() : rename_small_content();
        const eng::HistoryInputs in = inputs_of(make_synthetic(g.seed), g.seed, settings_of(c), content);
        const Tables t = eng::history_stage(content, in);
        check_rows(g, t);
        std::map<std::int64_t, std::string> geo;
        for (const auto& x : in.regions) geo[x.id] = x.name;
        std::set<std::string> other;
        for (const Row& x : t.civilization) other.insert(x[1].bytes);
        for (const Row& x : t.settlement) other.insert(x[1].bytes);
        CHECK(renaming_violations(t, tuning_of(content), static_cast<std::int64_t>(in.regions.size()), other, geo) ==
              std::vector<std::string>{});
        if (g.data == "off") {
            CHECK(t.region_name.empty());
            for (const Row& e : t.event) CHECK(at(e, 3) != k_region_renamed);
        }
        for (const RenameRow& r : g.renames) rules.insert(r.rule);
    }
    CHECK(std::vector<std::string>(rules.begin(), rules.end()).size() == renaming_golden().rules_seen.size());
}

}  // TEST_SUITE

TEST_SUITE("renaming-slow" * doctest::skip()) {

TEST_CASE("P5-12 13: every other world case against the golden file; invariants 1 to 6 (R2, R6)") {
    for (const RCase& g : renaming_golden().cases) {
        if (g.kind == "world" && g.name != "geo-seed-42-512") {
            check_world_case(g);
        }
    }
}

}  // TEST_SUITE
