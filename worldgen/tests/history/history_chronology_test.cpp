// Unit tests of spec P3-03 (chronology), section 14.1 items 1 to 6 and
// 14.3 items 18 and 19: the worked examples of 7.1 through the seams (u,
// pick, d and year per race; ages; gods with their attempts (from generator
// 0.17.0 the gods of P3-03 r3: drawn with the name filter and god_patterns,
// P3-02 11.6's table; the 7.1 text keeps r4's names); the
// collision and clamp cases), C2, the 63 subsets at seed 42, the age names
// of C4, the entry checks of C1 and the data checks of section 6 at the
// library and at the stage. Written from the spec only; the library is
// reached through p3_03_chronology_api.hpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "p3_03_chronology_support.hpp"

namespace {

using namespace chrono_test;
using chrono_api::Params;

constexpr std::uint64_t K_AW_42 = 0xA7284FA6D2415A60ULL;

std::vector<std::int64_t> years_of_rows(const Rows& r) {
    std::vector<std::int64_t> y;
    for (const RaceRow& x : r.races) {
        if (x.present == 1) {
            REQUIRE(x.awakening_year.has_value());
            y.push_back(*x.awakening_year);
        }
    }
    return y;
}

std::vector<std::string> god_names(const Rows& r) {
    std::vector<std::string> out;
    for (const GodRow& g : r.gods) {
        out.push_back(g.name);
    }
    return out;
}

std::string stage_error(const Params& p, const wd::ContentSet& c = shipped()) {
    try {
        (void)chrono_api::stage(c, p, 42, {});
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

}  // namespace

TEST_SUITE("chronology") {

// --- 14.1 item 1: the worked examples of 7.1 ----------------------------------------------------

TEST_CASE("P3-03 7.1, 14.1 item 1: seed 42, span 6000: K_aw, base, u, pick, d and year per race") {
    CHECK(worldgen::stream_key(42, "hist.chronology.awakening") == K_AW_42);
    const std::vector<OwnStep> s = own_shipped_schedule(42, 6000, six());
    REQUIRE(s.size() == 6U);
    const std::vector<std::int64_t> base = {0, 2100, 3300, 4200, 4980, 5580};
    const std::vector<std::uint64_t> u = {0x48F251C73BC89E2CULL, 0x04C4CCA630137EBAULL, 0xDA392A12D57DEAEBULL,
                                          0x67CC0B87B4CE8D5AULL, 0xA1211F03C3A8FF14ULL};
    const std::vector<std::int64_t> pick = {34, 2, 103, 49, 76};
    const std::vector<std::int64_t> d = {-26, -58, 43, -11, 16};
    const std::vector<std::int64_t> year = {0, 2074, 3242, 4243, 4969, 5596};
    for (std::size_t i = 0; i < 6; ++i) {
        CAPTURE(i);
        CHECK(s[i].base == base[i]);
        CHECK(s[i].year == year[i]);
        if (i > 0) {
            REQUIRE(s[i].u.has_value());
            CHECK(*s[i].u == u[i - 1]);
            CHECK(s[i].pick == pick[i - 1]);
            CHECK(s[i].d == d[i - 1]);
        }
    }
    // The seams: C2 and C3.
    const Tuning t = chrono_api::tuning_from(shipped());
    CHECK(t == shipped_tuning());
    CHECK(chrono_api::bounds(6000, 6, t) == chrono_api::Bounds{100, 60});
    const auto [ids, shares] = present_of(six());
    CHECK(chrono_api::awakening_years(42, 6000, ids, shares, t) == year);
    // The stage: years, ages and gods with attempts.
    const Rows r = chrono_api::stage(shipped(), Params{}, 42, {});
    CHECK(years_of_rows(r) == year);
    CHECK(ages_text(r.ages) ==
          "1:1:0:2073:The Age of Elves|2:2:2074:3241:The Age of Goblins|3:3:3242:4242:The Age of Dwarves|"
          "4:4:4243:4968:The Age of Faeries|5:5:4969:5595:The Age of Humans|6:6:5596:5999:The Age of Orcs");
    CHECK(god_names(r) == std::vector<std::string>{"Fielie", "Krokzit", "Bronhak", "Syfiash", "Wealstuld", "Kruu'mold"});
    CHECK(replay_gods(r, 42, {}, shipped()) == "1:Fielie:0|2:Krokzit:0|3:Bronhak:1|4:Syfiash:0|5:Wealstuld:0|6:Kruu'mold:0");
    CHECK(chrono_api::god_choices(shipped(), Params{}, 42, {}) == "1:Fielie:0|2:Krokzit:0|3:Bronhak:1|4:Syfiash:0|5:Wealstuld:0|6:Kruu'mold:0");
}

TEST_CASE("P3-03 7.1, 14.1 item 1: seed 0 and seed 1, span 6000") {
    {
        const std::vector<OwnStep> s = own_shipped_schedule(0, 6000, six());
        std::vector<std::int64_t> d;
        for (std::size_t i = 1; i < s.size(); ++i) {
            d.push_back(s[i].d);
        }
        CHECK(d == std::vector<std::int64_t>{-32, -35, -22, 23, -46});
        const Rows r = chrono_api::stage(shipped(), Params{}, 0, {});
        CHECK(years_of_rows(r) == std::vector<std::int64_t>{0, 2068, 3265, 4178, 5003, 5534});
        CHECK(god_names(r) == std::vector<std::string>{"Saene", "Kruskux", "Nurnthaimm", "Whaia", "Steldhil", "Uruld'kruum"});
        CHECK(replay_gods(r, 0, {}, shipped()) ==
              "1:Saene:0|2:Kruskux:0|3:Nurnthaimm:0|4:Whaia:1|5:Steldhil:0|6:Uruld'kruum:1");
    }
    {
        const std::vector<OwnStep> s = own_shipped_schedule(1, 6000, six());
        std::vector<std::int64_t> d;
        for (std::size_t i = 1; i < s.size(); ++i) {
            d.push_back(s[i].d);
        }
        CHECK(d == std::vector<std::int64_t>{-29, 41, -22, -46, 0});
        const Rows r = chrono_api::stage(shipped(), Params{}, 1, {});
        CHECK(years_of_rows(r) == std::vector<std::int64_t>{0, 2071, 3341, 4178, 4934, 5580});
        CHECK(god_names(r) ==
              std::vector<std::string>{"Laesiel", "Gragg-Gnurk", "Khummhor", "Synmia", "Steckwint", "Kaag'azak"});
    }
}

TEST_CASE("P3-03 7.1, 14.1 item 1: seed 7, span 2000 (hist-span-2000-512)") {
    const Tuning t = shipped_tuning();
    CHECK(chrono_api::bounds(2000, 6, t) == chrono_api::Bounds{100, 20});
    const std::vector<OwnStep> s = own_shipped_schedule(7, 2000, six());
    std::vector<std::int64_t> base;
    for (const OwnStep& x : s) {
        base.push_back(x.base);
    }
    CHECK(base == std::vector<std::int64_t>{0, 700, 1100, 1400, 1660, 1860});
    const Rows r = chrono_api::stage(shipped(), params_with(2000, six()), 7, {});
    CHECK(years_of_rows(r) == std::vector<std::int64_t>{0, 688, 1114, 1409, 1644, 1879});
    std::vector<std::int64_t> ends;
    for (const AgeRow& a : r.ages) {
        ends.push_back(a.end_year);
    }
    CHECK(ends == std::vector<std::int64_t>{687, 1113, 1408, 1643, 1878, 1999});
    CHECK(god_names(r) == std::vector<std::string>{"Onveth", "Buzork", "Mammga", "Ouwheefy", "Gordman", "Maarkzaagh"});
}

TEST_CASE("P3-03 7.1, 14.1 item 1: seed 3, --races elf,orc (hist-races-elf-orc-1024)") {
    const std::vector<OwnStep> s = own_shipped_schedule(3, 6000, {"elf", "orc"});
    REQUIRE(s.size() == 2U);
    REQUIRE(s[1].u.has_value());
    CHECK(*s[1].u == 0xAEA8CB7C6C7550B5ULL);
    CHECK(s[1].d == 22);
    const Rows r = chrono_api::stage(shipped(), params_with(6000, {"elf", "orc"}), 3, {});
    CHECK(ages_text(r.ages) == "1:1:0:5601:The Age of Elves|2:6:5602:5999:The Age of Orcs");
    CHECK(god_names(r) == std::vector<std::string>{"Vaennela", "Sluskat", "Nonthom", "Pyia", "Bulwan", "A'ghaz"});
    CHECK(replay_gods(r, 3, {}, shipped()) == "1:Vaennela:0|2:Sluskat:1|3:Nonthom:1|4:Pyia:0|5:Bulwan:0|6:A'ghaz:0");
    CHECK(chrono_api::god_choices(shipped(), params_with(6000, {"elf", "orc"}), 3, {}) == "1:Vaennela:0|2:Sluskat:1|3:Nonthom:1|4:Pyia:0|5:Bulwan:0|6:A'ghaz:0");
    REQUIRE(r.races.size() == 6U);
    for (std::size_t i = 1; i <= 4; ++i) {
        CAPTURE(i);
        CHECK(r.races[i].present == 0);
        CHECK_FALSE(r.races[i].awakening_year.has_value());
        CHECK(r.races[i].creator_god == static_cast<std::int64_t>(i + 1));
    }
    CHECK(r.races[0].awakening_year == std::optional<std::int64_t>(0));
    CHECK(r.races[5].awakening_year == std::optional<std::int64_t>(5602));
}

TEST_CASE("P3-03 7.1, 14.1 item 1: seed 42 goblin,dwarf,human; seed 2^64-1 span 12000 orc") {
    {
        const Rows r = chrono_api::stage(shipped(), params_with(6000, {"goblin", "dwarf", "human"}), 42, {});
        CHECK(ages_text(r.ages) ==
              "1:2:0:3241:The Age of Goblins|2:3:3242:4968:The Age of Dwarves|3:5:4969:5999:The Age of Humans");
        CHECK(god_names(r) == std::vector<std::string>{"Fielie", "Krokzit", "Bronhak", "Syfiash", "Wealstuld", "Kruu'mold"});
    }
    {
        const Rows r = chrono_api::stage(shipped(), params_with(12000, {"orc"}), M64, {});
        CHECK(ages_text(r.ages) == "1:6:0:11999:The Age of Orcs");
        CHECK(god_names(r) == std::vector<std::string>{"Ciefa", "Slogob", "Narinnadin", "Oupoubia", "Hardcald", "Kruurz'gruu"});
    }
}

TEST_CASE("P3-03 7.1, 14.1 item 1: collisions with preloaded region names") {
    const std::vector<std::string> regions = collision_regions();
    const Rows r = chrono_api::stage(shipped(), Params{}, 42, regions);
    REQUIRE(r.ages.size() == 6U);
    CHECK(r.ages[0].name == "The Age of Elves II");
    for (std::size_t i = 1; i < 6; ++i) {
        CHECK(r.ages[i].name.find(" II") == std::string::npos);
    }
    REQUIRE(r.gods.size() == 6U);
    CHECK(r.gods[0].name == "Mical");
    CHECK(r.gods[1].name == "Krokzit");  // r3: the r5 goblin god no longer meets the region "Krok"
    const std::string replay = replay_gods(r, 42, regions, shipped());
    CHECK(replay.substr(0, replay.find("|3:")) == "1:Mical:2|2:Krokzit:0");
    const std::string choices = chrono_api::god_choices(shipped(), Params{}, 42, regions);
    CHECK(choices == replay);
    CHECK(choices.substr(0, choices.find("|3:")) == "1:Mical:2|2:Krokzit:0");
    // Every name of the world is distinct (invariant 8).
    std::set<std::string> all(regions.begin(), regions.end());
    for (const AgeRow& a : r.ages) {
        CHECK(all.insert(a.name).second);
    }
    for (const GodRow& g : r.gods) {
        CHECK(all.insert(g.name).second);
    }
}

TEST_CASE("P3-03 7.1, 14.1 item 1: the clamp case (hand-made data, gap 100, jitter 0)") {
    const Tuning t{0, 100, "The Age of {plural}"};
    const std::vector<std::int64_t> ids = {1, 2, 3, 4};
    const std::vector<std::int64_t> shares = {0, ONE_RAW / 2, 2151778615, 4290672329};  // 0, 0.5, 0.501, 0.999
    const std::vector<OwnStep> s = own_schedule(0, 6000, ids, shares, 0, 100);
    std::vector<std::int64_t> base;
    for (const OwnStep& x : s) {
        base.push_back(x.base);
    }
    CHECK(base == std::vector<std::int64_t>{0, 3000, 3006, 5994});
    CHECK(years_of(s) == std::vector<std::int64_t>{0, 3000, 3100, 5900});
    CHECK(chrono_api::awakening_years(0, 6000, ids, shares, t) == std::vector<std::int64_t>{0, 3000, 3100, 5900});
}

// --- 14.1 item 2: C2 ----------------------------------------------------------------------------

TEST_CASE("P3-03 C2, 14.1 item 2: jmax 0 puts every later race at its base; G reduced to E div P") {
    const Tuning zero{0, 100, "The Age of {plural}"};
    CHECK(chrono_api::bounds(6000, 6, zero) == chrono_api::Bounds{100, 0});
    const auto [ids, shares] = present_of(six());
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{42}, M64}) {
        CAPTURE(seed);
        CHECK(chrono_api::awakening_years(seed, 6000, ids, shares, zero) ==
              std::vector<std::int64_t>{0, 2100, 3300, 4200, 4980, 5580});
    }
    const Tuning wide{0, 2000, "The Age of {plural}"};
    CHECK(chrono_api::bounds(2000, 6, wide) == chrono_api::Bounds{333, 0});
    CHECK(chrono_api::bounds(2000, 1, wide) == chrono_api::Bounds{2000, 0});
    CHECK(chrono_api::bounds(12000, 5, wide) == chrono_api::Bounds{2000, 0});
    // G = 333 forces every year: lo = hi at each step.
    CHECK(chrono_api::awakening_years(0, 2000, ids, shares, wide) ==
          std::vector<std::int64_t>{0, 335, 668, 1001, 1334, 1667});
    // The shipped jitter at the three spans (C2: 20 / 60 / 120) and G = 100 for every subset size.
    for (std::int64_t p = 1; p <= 6; ++p) {
        CHECK(chrono_api::bounds(2000, p, shipped_tuning()) == chrono_api::Bounds{100, 20});
        CHECK(chrono_api::bounds(6000, p, shipped_tuning()) == chrono_api::Bounds{100, 60});
        CHECK(chrono_api::bounds(12000, p, shipped_tuning()) == chrono_api::Bounds{100, 120});
    }
    // RHA of mul_int: jitter 0.05 (raw 214748365) at span 2001 is 100.05... -> 100; at 2010, 100.5... -> 101.
    const Tuning j5{214748365, 1, "The Age of {plural}"};
    CHECK(chrono_api::bounds(2001, 2, j5).jmax == own_jmax(2001, 214748365));
    CHECK(chrono_api::bounds(2010, 2, j5).jmax == own_jmax(2010, 214748365));
    CHECK(own_jmax(2010, 214748365) == 101);
}

// --- 14.1 item 3: the 63 subsets at seed 42 ---------------------------------------------------

TEST_CASE("P3-03 C3, 14.1 item 3: every non-empty subset at seed 42, span 6000") {
    const Rows full = chrono_api::stage(shipped(), Params{}, 42, {});
    std::map<std::int64_t, std::int64_t> full_year;
    for (const RaceRow& x : full.races) {
        full_year[x.id] = x.awakening_year.value_or(-1);
    }
    const std::vector<std::vector<std::string>> subsets = all_subsets();
    REQUIRE(subsets.size() == 63U);
    for (const std::vector<std::string>& sub : subsets) {
        CAPTURE(joined(sub));
        const Rows r = chrono_api::stage(shipped(), params_with(6000, sub), 42, {});
        REQUIRE(r.ages.size() == sub.size());
        CHECK(r.ages.front().start_year == 0);
        for (std::size_t i = 0; i < r.ages.size(); ++i) {
            CHECK(r.ages[i].id == static_cast<std::int64_t>(i + 1));
            CHECK(r.ages[i].race == race_listing(sub[i]).order);
            if (i > 0) {
                CHECK(r.ages[i].start_year == full_year[r.ages[i].race]);
            }
            CHECK(r.ages[i].end_year == (i + 1 < r.ages.size() ? r.ages[i + 1].start_year - 1 : 5999));
        }
        CHECK(r.gods == full.gods);
        CHECK(r.cultures == full.cultures);
        CHECK(r.race_biomes == full.race_biomes);
    }
}

// --- 14.1 item 4: age names ---------------------------------------------------------------------

TEST_CASE("P3-03 C4, 14.1 item 4: age_name_text with the shipped form and other forms") {
    CHECK(chrono_api::age_name_text("The Age of {plural}", "Elves", "Elven") == "The Age of Elves");
    CHECK(chrono_api::age_name_text("The {adjective} Age", "Elves", "Elven") == "The Elven Age");
    CHECK(chrono_api::age_name_text("{plural} and {adjective}, {plural} or {adjective}", "Orcs", "Orcish") ==
          "Orcs and Orcish, Orcs or Orcish");
    // Replacement text is not rescanned (C4 step 1).
    CHECK(chrono_api::age_name_text("{plural}", "{adjective}", "X") == "{adjective}");
    CHECK(chrono_api::age_name_text("{plural}{plural}", "A", "B") == "AA");
    for (const char* form : {"The Age of {plural}", "The {adjective} Age", "{adjective}{plural}", "Age"}) {
        CHECK(chrono_api::age_name_text(form, "Dwarves", "Dwarven") == own_age_text(form, "Dwarves", "Dwarven"));
    }
}

TEST_CASE("P3-03 C4, 14.1 item 4: forms through the stage; the numeral fallback II, then III") {
    {
        const wd::ContentSet c = edited_content({{"history", "chronology.toml", "age_name_form = \"The Age of {plural}\"",
                                                  "age_name_form = \"The {adjective} Age\""}});
        const Rows r = chrono_api::stage(c, Params{}, 42, {});
        REQUIRE(r.ages.size() == 6U);
        CHECK(r.ages[0].name == "The Elven Age");
        CHECK(r.ages[5].name == "The Orcish Age");
        CHECK(r.ages[1].name == "The Goblin Age");
    }
    {
        const wd::ContentSet c = edited_content({{"history", "chronology.toml", "age_name_form = \"The Age of {plural}\"",
                                                  "age_name_form = \"{plural} {adjective} {plural} {adjective}\""}});
        const Rows r = chrono_api::stage(c, Params{}, 42, {});
        REQUIRE(r.ages.size() == 6U);
        CHECK(r.ages[2].name == "Dwarves Dwarven Dwarves Dwarven");
    }
    {
        const Rows r = chrono_api::stage(shipped(), Params{}, 42, {"The Age of Elves"});
        CHECK(r.ages[0].name == "The Age of Elves II");
    }
    {
        const Rows r = chrono_api::stage(shipped(), Params{}, 42, {"The Age of Elves", "The Age of Elves II"});
        CHECK(r.ages[0].name == "The Age of Elves III");
    }
    {
        // Two races with the same age text (goblins' and orcs' adjective form): the second takes II.
        const wd::ContentSet c = edited_content(
            {{"history", "chronology.toml", "age_name_form = \"The Age of {plural}\"", "age_name_form = \"The Dark {adjective} Age\""},
             {"races", "orc.toml", "adjective = \"Orcish\"", "adjective = \"Goblin\""}});
        const Rows r = chrono_api::stage(c, Params{}, 42, {});
        REQUIRE(r.ages.size() == 6U);
        CHECK(r.ages[1].name == "The Dark Goblin Age");
        CHECK(r.ages[5].name == "The Dark Goblin Age II");
    }
}

// --- 14.1 item 5: entry checks of C1 -----------------------------------------------------------

TEST_CASE("P3-03 C1, 14.1 item 5: settings checks of the seam, each with its message") {
    CHECK(stage_error(params_with(1999, six())) == "chronology: settings: history_span 1999 outside 2000 to 12000");
    CHECK(stage_error(params_with(12001, six())) == "chronology: settings: history_span 12001 outside 2000 to 12000");
    CHECK(stage_error(params_with(6000, {})) == "chronology: settings: no race present");
    CHECK(stage_error(params_with(6000, {"elf", "troll", "gnome"})) == "chronology: settings: unknown race 'troll'");
    CHECK(stage_error(params_with(6000, {"orc", "elf"})) == "chronology: settings: races not in race order at 'elf'");
    CHECK(stage_error(params_with(6000, {"elf", "elf"})) == "chronology: settings: races not in race order at 'elf'");
    CHECK(stage_error(params_with(6000, {"elf", "dwarf", "goblin"})) ==
          "chronology: settings: races not in race order at 'goblin'");
    // Order of the checks: span before races, unknown before order.
    CHECK(stage_error(params_with(1999, {})) == "chronology: settings: history_span 1999 outside 2000 to 12000");
    CHECK(stage_error(params_with(6000, {"orc", "elf", "troll"})) == "chronology: settings: unknown race 'troll'");
    CHECK(stage_error(params_with(2000, six())).empty());
    CHECK(stage_error(params_with(12000, six())).empty());
}

TEST_CASE("P3-03 C1, 14.1 item 5: the stage entry without region rows is fatal") {
    dbtest::TempDir dir("chronology-noregions");
    CHECK(run_pipeline(dir.file("a.db"), shipped(), 42, Params{}, {}) == "chronology: input precondition: no region rows");
    CHECK_FALSE(dbtest::exists(dir.file("a.db")));
    CHECK(run_pipeline(dir.file("b.db"), shipped(), 42, Params{}, {"Somewhere"}).empty());
    // The settings checks also hold at the stage entry (a seam guard).
    CHECK(run_pipeline(dir.file("c.db"), shipped(), 42, params_with(6000, {"orc", "elf"}), {"Somewhere"}) ==
          "chronology: settings: races not in race order at 'elf'");
}

// --- 14.1 item 6: data checks of section 6 ------------------------------------------------------

TEST_CASE("P3-03 6, 14.1 item 6: the shipped and fixture data pass check_history_data") {
    CHECK(errors_text(chrono_api::check_data(shipped())).empty());
    CHECK(errors_text(chrono_api::check_data(fixture_geo())).empty());
    CHECK(errors_text(chrono_api::check_data(fixture_geo_small())).empty());
}

TEST_CASE("P3-03 6, 14.1 item 6: each condition fails once: library, then the stage seam and entry") {
    for (const DataCase& dc : data_cases()) {
        CAPTURE(dc.label);
        const wd::ContentSet c = edited_content(dc.edits);
        const std::vector<wd::LoadError> errors = chrono_api::check_data(c);
        REQUIRE(errors.size() == dc.messages.size());
        for (std::size_t i = 0; i < errors.size(); ++i) {
            CHECK(errors[i].path == "races");
            CHECK(errors[i].line == 0U);
            CHECK(errors[i].column == 0U);
            CHECK(errors[i].message == dc.messages[i]);
        }
        CHECK(stage_error(Params{}, c) == "chronology: data: " + dc.messages.front());
        dbtest::TempDir dir("chronology-data");
        CHECK(run_pipeline(dir.file("w.db"), c, 42, Params{}, {"Somewhere"}) == "chronology: data: " + dc.messages.front());
    }
}

// D0 cannot come from the loader (an empty required kind is a loader
// error), so the content set is built in memory from the shipped one.
wd::ContentSet without_races(bool drop_kind) {
    wd::ContentSet c = shipped();
    for (std::size_t i = 0; i < c.kinds.size(); ++i) {
        if (c.kinds[i].kind == wd::RACE_KIND) {
            if (drop_kind) {
                c.kinds.erase(c.kinds.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                c.kinds[i].definitions.clear();
            }
            break;
        }
    }
    return c;
}

TEST_CASE("P3-03 6 D0 (lead's decision 2026-11-19): a content set without race definitions") {
    for (const bool drop_kind : {false, true}) {
        CAPTURE(drop_kind);
        const wd::ContentSet c = without_races(drop_kind);
        const std::vector<wd::LoadError> errors = chrono_api::check_data(c);
        REQUIRE_FALSE(errors.empty());
        // First in the D order (the cultures still name their races: D3 cannot
        // fire without race definitions, so D0 is the only error).
        CHECK(errors[0].path == "races");
        CHECK(errors[0].line == 0U);
        CHECK(errors[0].column == 0U);
        CHECK(errors[0].message == D0_MESSAGE);
        CHECK(errors.size() == 1U);
        // Stage seam and entry: C1 item 3 (settings) comes before item 4 (data).
        CHECK(stage_error(params_with(6000, {"elf"}), c) == "chronology: settings: unknown race 'elf'");
        dbtest::TempDir dir("chronology-d0");
        CHECK(run_pipeline(dir.file("a.db"), c, 42, params_with(6000, {"elf"}), {"Somewhere"}) ==
              "chronology: settings: unknown race 'elf'");
        // An empty race list does not reach D0 either: C1 item 3 refuses it at
        // the seam, and generate_world refuses params.history before any stage
        // (4.1 step 2). So D0 is reported by check_history_data (and by the
        // CLI's input check) but never by the stage guard with these inputs.
        CHECK(stage_error(params_with(6000, {}), c) == "chronology: settings: no race present");
        CHECK(run_pipeline(dir.file("b.db"), c, 42, params_with(6000, {}), {"Somewhere"}) ==
              "generate_world: params.history.races is invalid");
        CHECK_FALSE(dbtest::exists(dir.file("a.db")));
        CHECK_FALSE(dbtest::exists(dir.file("b.db")));
    }
}

TEST_CASE("P3-03 6, 14.1 item 6: two failures at once are sorted by message; absent races are checked too") {
    const wd::ContentSet c = edited_content(
        {{"races", "orc.toml", "  { biome = \"alpine\", weight = -300 },",
          "  { biome = \"alpine\", weight = -300 },\n  { biome = \"ocean\", weight = 1 },"},
         {"races", "elf.toml", "  { biome = \"alpine\", weight = -600 },",
          "  { biome = \"alpine\", weight = -600 },\n  { biome = \"lake\", weight = 1 },"}});
    const std::vector<wd::LoadError> errors = chrono_api::check_data(c);
    REQUIRE(errors.size() == 2U);
    CHECK(errors[0].message == "race 'elf' prefers water biome 'lake'");
    CHECK(errors[1].message == "race 'orc' prefers water biome 'ocean'");
    // The orc is absent from the settings, the stage still refuses the data.
    CHECK(stage_error(params_with(6000, {"goblin"}), c) == "chronology: data: race 'elf' prefers water biome 'lake'");
    // One race with D1 and D3 at once: two messages, by bytes.
    const wd::ContentSet c2 = edited_content(
        {{"races", "human.toml", "culture = \"human\"", "culture = \"orcish\""},
         {"races", "human.toml", "  { biome = \"alpine\", weight = -500 },",
          "  { biome = \"alpine\", weight = -500 },\n  { biome = \"lake\", weight = 5 },"}});
    const std::vector<wd::LoadError> e2 = chrono_api::check_data(c2);
    REQUIRE(e2.size() == 2U);
    CHECK(e2[0].message == "race 'human' has culture 'orcish' of race 'orc'");
    CHECK(e2[1].message == "race 'human' prefers water biome 'lake'");
}

// --- 14.3 items 18 and 19 -----------------------------------------------------------------------

TEST_CASE("P3-03 14.3 item 19: chronology_stage twice gives the same rows; no registry is shared") {
    const Rows a = chrono_api::stage(shipped(), Params{}, 42, collision_regions());
    const Rows b = chrono_api::stage(shipped(), Params{}, 42, collision_regions());
    CHECK(a == b);
    // A call in between with other region names does not leak into the next call.
    (void)chrono_api::stage(shipped(), Params{}, 42, {"Fielie", "Krokzit", "Bronhak", "Syfiash", "Wealstuld", "Kruu'mold"});
    const Rows c = chrono_api::stage(shipped(), Params{}, 42, {});
    CHECK(god_names(c) == std::vector<std::string>{"Fielie", "Krokzit", "Bronhak", "Syfiash", "Wealstuld", "Kruu'mold"});
    CHECK(chrono_api::stage(shipped(), Params{}, 42, collision_regions()) == a);
}

TEST_CASE("P3-03 14.3 item 18: an extra keyed draw on another stream changes nothing") {
    const Rows before = chrono_api::stage(shipped(), Params{}, 7, {});
    const std::uint64_t other = worldgen::stream_key(7, "test.chronology.extra");
    std::uint64_t sink = 0;
    for (std::uint64_t k = 0; k < 100; ++k) {
        sink ^= names_api::draw(other, 0, 2, 0, k);  // the race id of goblins, year 0, as C3's own draw
    }
    CHECK(sink != 0);
    CHECK(chrono_api::stage(shipped(), Params{}, 7, {}) == before);
    CHECK(years_of_rows(before) == years_of(own_shipped_schedule(7, 6000, six())));
}


TEST_CASE("P3-03 r3, P3-02 11.3 and 11.7 item 4: names_form_text refuses a form whose fixed text holds a filtered fragment") {
    // The stage check of P3-02 r5 11.3 (r6: every fixed word too), after the data checks and before
    // any name is drawn. Each fixture form is written as a concatenation, so the source holds no
    // offensive word whole. The message is 11.3's text, prefix `history:` (an assumption: P3-03 r3
    // gives the check, not a chronology wording).
    struct FormCase {
        std::string form;
        std::string entry;
    };
    const std::vector<FormCase> cases = {{"{root}" + std::string("co") + "ck", std::string("co") + "ck"},
                                         {std::string("Sh") + "it" + "{root}", std::string("sh") + "it"},
                                         {"The Scrap" + std::string("yard of {root}"), std::string("cr") + "ap"},
                                         {"The Snot" + std::string("ty Court of {root}"), std::string("sn") + "ot"}};
    for (const FormCase& fc : cases) {
        CAPTURE(fc.entry.size());
        const wd::ContentSet c = edited_content(
            {{"cultures", "human.toml", "settlement_forms = [\"{root}\", \"{root}ford\"",
              "settlement_forms = [\"" + fc.form + "\", \"{root}ford\""}});
        CHECK(stage_error(Params{}, c) == "history: culture 'human' settlement_forms[0] '" + fc.form +
                                              "' holds the filtered text '" + fc.entry + "'");
    }
    // A data check failing as well wins: names_form_text runs after D1-D5.
    const wd::ContentSet both = edited_content(
        {{"cultures", "human.toml", "settlement_forms = [\"{root}\", \"{root}ford\"",
          "settlement_forms = [\"" + cases[1].form + "\", \"{root}ford\""},
         {"races", "orc.toml", "  { biome = \"alpine\", weight = -300 },",
          "  { biome = \"alpine\", weight = -300 },\n  { biome = \"ocean\", weight = 1 },"}});
    CHECK(stage_error(Params{}, both) == "chronology: data: race 'orc' prefers water biome 'ocean'");
    // {root}ford passes: its fused letters are inside the root word, which the filter tests per attempt.
    const wd::ContentSet ok = edited_content(
        {{"cultures", "human.toml", "settlement_forms = [\"{root}\", \"{root}ford\"",
          "settlement_forms = [\"{root}ford\", \"{root}ford\""}});
    CHECK(stage_error(Params{}, ok).empty());
    // The shipped forms pass (P3-02 11.3 lists their fixed words).
    CHECK(stage_error(Params{}, shipped()).empty());
}

}  // TEST_SUITE
