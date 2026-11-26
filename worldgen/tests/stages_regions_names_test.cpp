// Region names with identity (spec P1-06 r3 and r4, generator 0.17.0;
// RG11, section 4.1 kind `placename`, check 4a, 10.5, 14.8) and the stream
// registry of P0-01 amendment 20.
//
// Every expected value is written in the test from the spec (5.1, 14.3,
// 14.4, 14.8) or from a hand-made fixture. Each unit case runs the test's
// own RG11 (p1_06_regions_names.hpp, written from the spec) against those
// values, and, with the P3-08 API (p3_08_switch.hpp), the stage's seams
// (p1_06_regions_api.hpp, interim names of 10.5) against the same values.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "p1_06_regions_api.hpp"
#include "p1_06_regions_names.hpp"
#include "p1_06_regions_support.hpp"
#include "p3_08_switch.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"

namespace rn = region_names;

namespace {

const rn::PlaceData& pd() { return rn::shipped(); }
const rn::PlaceDef& common() { return pd().common(); }
const rn::PlaceDef& land(std::string_view id) { return pd().by_id(id); }

std::vector<const rn::PlaceDef*> traits(const std::vector<std::string>& t) { return rn::trait_defs(pd(), t); }

std::vector<rn::Token> toks(std::string_view form) {
    const auto t = rn::parse_form(form);
    REQUIRE_MESSAGE(t.has_value(), "not a form: " << std::string(form));
    return *t;
}

std::string rule_of(std::string_view form, const std::vector<std::string>& values, const rn::PlaceDef& c = common()) {
    return rn::invalid_rule(toks(form), values, c);
}

// A common definition for hand-made cases: no words, the given forbidden
// and excluded lists.
rn::PlaceDef fixture_common(std::vector<std::string> forbidden = {}, std::vector<std::string> excluded = {}) {
    rn::PlaceDef d;
    d.id = "common";
    d.role = "common";
    d.forbidden = std::move(forbidden);
    d.excluded = std::move(excluded);
    return d;
}

rn::PlaceDef fixture_land(std::string id, std::vector<rn::Form> forms) {
    rn::PlaceDef d;
    d.id = std::move(id);
    d.role = "land";
    d.special = "none";
    d.biomes = {"temperate_forest"};
    d.forms = std::move(forms);
    return d;
}

#if UNMAPPED_P3_08_API
const region_api::Placenames& seam_pn() { return region_api::placenames(); }
#endif

}  // namespace

// --- 10.5 unit 1: the draw examples of 5.1 --------------------------------------------------------

TEST_CASE("P1-06 r3 5.1, 10.5 unit 1: K_name and every listed u(k) of seed 42, N 1024") {
    CHECK(worldgen::stream_key(42, "geo.regions.name") == 0x224A148B98263929ULL);
    const std::uint64_t key = rn::k_name(42, 1024);
    CHECK(key == 0xE1A9F22A266E243AULL);
    struct U {
        std::int64_t r, t, k;
        std::uint64_t v;
    };
    for (const U& u : std::vector<U>{{1, 0, 0, 0x6859A960857622CBULL},   {1, 0, 1, 0x7EE3FBE2B3DAA4E4ULL},
                                     {1, 0, 2, 0xA731588330C4201DULL},   {1, 0, 3, 0xE72102C4301D4A38ULL},
                                     {1, 0, 4, 0x26E0EFE6AE26CED5ULL},   {10, 0, 0, 0x42002C340A52E948ULL},
                                     {10, 0, 1, 0x2056F9F4076F2CC2ULL},  {10, 0, 2, 0x80DC08382FB40AEEULL},
                                     {10, 0, 3, 0xA85791F9B2BC6F64ULL},  {10, 0, 4, 0x672EF6DA06D6A3DAULL},
                                     {39, 0, 2, 0x673CC715D61964E8ULL},  {39, 0, 4, 0x99255536F43A47E7ULL},
                                     {40, 0, 0, 0x1DAD0A4F3DA9416DULL},  {40, 0, 1, 0xD767ECABF011F7C6ULL},
                                     {40, 0, 2, 0x2574A736E868C8E5ULL},  {40, 0, 3, 0x21EE6D0295292197ULL},
                                     {40, 0, 4, 0xA4FFEA0405079A69ULL},  {40, 1, 0, 0xF8C4AD91FCA3D645ULL},
                                     {40, 1, 1, 0xB4FB47D898CFA306ULL},  {40, 1, 2, 0xA07474902FABD7AEULL},
                                     {40, 1, 3, 0xDC51F8D891CC3346ULL},  {40, 1, 4, 0xF07596C36D36F72FULL}}) {
        CAPTURE(u.r);
        CAPTURE(u.t);
        CAPTURE(u.k);
        CHECK(worldgen::hash_at3(key, u.r, u.t, u.k) == u.v);
    }
}

// The draws of 5.1 (r3) on the 0.19.0 lists (P1-06 r6 15.1 to 15.3 shrink the common lists, so every
// pool size and index of 5.1 moves). Values: the test writer's reference p1_regions_reference.py at 0.19.0
// (5.1's own values, The Scarred Downs, Knelldown, Harrowholt and Prince's Vow, are those of the 0.18.0 lists
// and are checked there with --version 0.18.0).
TEST_CASE("P1-06 r3 5.1, r10 16: the 5.1 draws on the 0.21.0 lists: The Sighing Downs, Kitedown, Hawhurst, Queen's Wake (through Birchhurst)") {
    const std::uint64_t key = rn::k_name(42, 1024);
    const rn::NameTuning t{};
    struct Want {
        std::int64_t region, attempt;
        std::string land;
        std::vector<std::string> traits;
        std::size_t form;
        // (slot, weights or empty, pool, pool size, index, word)
        struct S {
            std::string slot;
            std::vector<std::int64_t> weights;
            std::size_t pool, size;
            std::uint64_t index;
            std::string word;
        };
        std::vector<S> steps;
        bool valid;
        std::string name;
    };
    const std::vector<Want> cases = {
        // the 0.21.0 lists (P1-06 r10 16.1: 34 grim words moved from common to trait_barren): common prefixes 128,
        // adjectives 115, possessors 79, places 40 (values from the test writer's reference at 0.21.0; 0.19.0:
        // The Shuddering Downs, Kitedown, Harthurst, Birchhurst, Queen's Wake)
        {1, 0, "downs", {"coast"}, 1,
         {{"adjective", {6, 3, 2}, 0, 115, 75, "Sighing"}, {"noun", {0, 1, 0}, 1, 8, 1, "Downs"}}, true,
         "The Sighing Downs"},
        {10, 0, "downs", {"river", "fertile"}, 0,
         {{"prefix", {6, 3, 2}, 0, 128, 64, "Kite"}, {"suffix", {0, 4, 1}, 1, 6, 2, "down"}}, true, "Kitedown"},
        {39, 0, "wood", {"coast"}, 0, {{"prefix", {}, 0, 128, 51, "Haw"}, {"suffix", {}, 1, 7, 4, "hurst"}}, true,
         "Hawhurst"},
        {40, 0, "wood", {}, 0, {{"prefix", {6, 3, 0}, 1, 23, 3, "Birch"}, {"suffix", {0, 4, 0}, 1, 7, 4, "hurst"}},
         false, "Birchhurst"},
        {40, 1, "wood", {}, 5,
         {{"possessor", {8, 0, 0}, 0, 79, 49, "Queen"}, {"place", {8, 0, 0}, 0, 40, 37, "Wake"}}, true,
         "Queen's Wake"},
    };
    for (const Want& w : cases) {
        CAPTURE(w.region);
        CAPTURE(w.attempt);
        const rn::Attempt a = rn::attempt(key, w.region, w.attempt, land(w.land), common(), traits(w.traits), t);
        CHECK(a.form == w.form);
        REQUIRE(a.steps.size() == w.steps.size());
        for (std::size_t i = 0; i < w.steps.size(); ++i) {
            CAPTURE(i);
            CHECK(a.steps[i].slot == w.steps[i].slot);
            if (!w.steps[i].weights.empty()) {
                CHECK(a.steps[i].weights == w.steps[i].weights);
            }
            CHECK(a.steps[i].pool == w.steps[i].pool);
            CHECK(a.steps[i].size == w.steps[i].size);
            CHECK(a.steps[i].index == w.steps[i].index);
            CHECK(a.steps[i].word == w.steps[i].word);
        }
        CHECK(a.valid() == w.valid);
        CHECK(a.name == w.name);
        if (!w.valid) {
            CHECK(a.rule == "V1");  // h, h
        }
#if UNMAPPED_P3_08_API
        const auto s = region_api::name_attempt(seam_pn(), 42, 1024, w.region, w.attempt, w.land, w.traits,
                                                region_test::tuning());
        CHECK(s.first == w.valid);
        if (w.valid) {
            CHECK(s.second == w.name);  // an invalid attempt's name is unspecified
        }
#endif
    }
    // 5.1: attempt 1 of region 1 (Fletcher's Wolds on the 0.18.0 lists, Ferryman's Wolds on the 0.19.0 lists).
    CHECK(rn::attempt(key, 1, 1, land("downs"), common(), traits({"coast"}), t).name == "Exile's Wolds");
}

// --- 10.5 unit 2: keepers -----------------------------------------------------------------------

TEST_CASE("P1-06 r3 RG11 4 (r7 15.7), 10.5 unit 2: the largest region of a land noun keeps it; ties to the smaller id") {
    const rn::NameTuning t{};
    auto regions = [](const std::vector<std::pair<std::int64_t, std::int64_t>>& id_cells, std::string base) {
        std::vector<rn::NamedRegion> v;
        for (const auto& [id, cells] : id_cells) {
            rn::NamedRegion r;
            r.id = id;
            r.cells = cells;
            r.base = base;
            r.land = &land("downs");
            v.push_back(r);
        }
        return v;
    };
    // The North Downs case of 5.1: ids 1, 2, 6, 16 with 4443, 6898, 14036, 5593 cells.
    const auto a = rn::name_regions(regions({{1, 4443}, {2, 6898}, {6, 14036}, {16, 5593}}, "North Downs"), 42,
                                    1024, pd(), t);
    REQUIRE(a.has_value());
    CHECK(a->how == std::vector<std::string>{"0", "0", "K", "0"});
    CHECK(a->names[2] == "North Downs");
    CHECK(std::count(a->names.begin(), a->names.end(), "North Downs") == 1);
    // Region 1's first valid free attempt (attempt 0, The Scarred Downs on the 0.18.0 lists).
    const rn::Attempt a0 = rn::attempt(rn::k_name(42, 1024), 1, 0, land("downs"), common(),
                                       traits(rn::traits_of(rn::TraitRow{}, t)), t);
    CHECK(a0.valid());
    CHECK(a->names[0] == a0.name);
    // Equal cells: the smaller id keeps it.
    const auto b = rn::name_regions(regions({{3, 500}, {4, 500}}, "East Downs"), 0, 128, pd(), t);
    REQUIRE(b.has_value());
    CHECK(b->names[0] == "East Downs");
    CHECK(b->how[0] == "K");
    CHECK(b->names[1] != "East Downs");
    // A base name used once is kept.
    const auto c = rn::name_regions(regions({{7, 1}}, "West Downs"), 0, 128, pd(), t);
    REQUIRE(c.has_value());
    CHECK(c->names == std::vector<std::string>{"West Downs"});
}

// --- P1-06 r7 15.7 and r6 15.4 (generator 0.19.0): the keeper per land noun and V7 (15.6) --------------

namespace {

std::vector<rn::NamedRegion> rows_of(const std::vector<std::tuple<std::int64_t, std::int64_t, std::string,
                                                                  const rn::PlaceDef*>>& v) {
    std::vector<rn::NamedRegion> out;
    for (const auto& [id, cells, base, ld] : v) {
        rn::NamedRegion r;
        r.id = id;
        r.cells = cells;
        r.base = base;
        r.land = ld;
        out.push_back(r);
    }
    return out;
}

}  // namespace

TEST_CASE("P1-06 r7 15.6: four North Downs and one South Downs (cells 4443, 6898, 14036, 5593, 20000): only the South Downs keeps its name") {
    const rn::PlaceDef* d = &land("downs");
    const auto got = rn::name_regions(rows_of({{1, 4443, "North Downs", d}, {2, 6898, "North Downs", d},
                                               {3, 14036, "North Downs", d}, {4, 5593, "North Downs", d},
                                               {5, 20000, "South Downs", d}}),
                                      42, 1024, pd(), rn::NameTuning{});
    REQUIRE(got.has_value());
    CHECK(got->how[4] == "K");
    CHECK(got->names[4] == "South Downs");
    for (std::size_t i = 0; i < 4; ++i) {
        CAPTURE(i);
        CHECK(got->how[i] != "K");
        CHECK(got->names[i] != "North Downs");
        CHECK(got->names[i] != "South Downs");
    }
    CHECK(rn::land_noun("North Downs") == "Downs");
    CHECK(rn::land_noun("Central Wood") == "Wood");
}

TEST_CASE("P1-06 r7 15.6: equal cells keep the plain name on the smaller id; a noun used once is kept") {
    const rn::PlaceDef* d = &land("downs");
    const auto a = rn::name_regions(rows_of({{8, 700, "West Downs", d}, {9, 700, "East Downs", d}}), 0, 128, pd(),
                                    rn::NameTuning{});
    REQUIRE(a.has_value());
    CHECK(a->how[0] == "K");
    CHECK(a->names[0] == "West Downs");
    CHECK(a->how[1] != "K");
    const rn::PlaceDef* f = &land("fen");
    const auto b = rn::name_regions(rows_of({{1, 30, "Southwest Fen", f}}), 0, 128, pd(), rn::NameTuning{});
    REQUIRE(b.has_value());
    CHECK(b->names == std::vector<std::string>{"Southwest Fen"});
}

TEST_CASE("P1-06 r7 15.6: West Isle and East Isle share one keeper; an upland forest region (Fells) does not compete with the Woods") {
    const rn::PlaceDef* isle = pd().special("isle");
    const rn::PlaceDef* fells = pd().special("upland");
    const rn::PlaceDef* wood = pd().land_of_biome("temperate_forest");
    REQUIRE(isle != nullptr);
    REQUIRE(fells != nullptr);
    REQUIRE(wood != nullptr);
    const auto got = rn::name_regions(rows_of({{1, 300, "West Isle", isle}, {2, 900, "East Isle", isle},
                                               {3, 4000, "North Wood", wood}, {4, 7000, "South Wood", wood},
                                               {5, 100, "North Fells", fells}}),
                                      7, 512, pd(), rn::NameTuning{});
    REQUIRE(got.has_value());
    CHECK(got->how == std::vector<std::string>{got->how[0], "K", got->how[2], "K", "K"});
    CHECK(got->how[0] != "K");
    CHECK(got->how[2] != "K");
    CHECK(got->names[1] == "East Isle");
    CHECK(got->names[3] == "South Wood");
    CHECK(got->names[4] == "North Fells");
    for (std::size_t i : {std::size_t{0}, std::size_t{2}}) {
        CAPTURE(i);
        for (const char* k : {"East Isle", "South Wood", "North Fells"}) {
            CHECK(got->names[i] != k);
        }
    }
}

TEST_CASE("P1-06 r6 15.4, 15.6: V7 stems: The Weald of Floods after The Flooded Thicket is invalid; the next valid attempt names it") {
    CHECK(rn::v7_stem("Flooded") == "flood");
    CHECK(rn::v7_stem("Floods") == "flood");
    CHECK(rn::v7_stem("the Forsaken") == "forsa");
    CHECK(rn::v7_stem("Ash") == "ash");
    rn::PlaceData d;
    rn::PlaceDef c = fixture_common();
    rn::PlaceDef a = fixture_land("a", {{"The {adjective} {noun}", 1}});
    a.lists[3] = {"Flooded"};
    a.lists[0] = {"Thicket"};
    rn::PlaceDef b = fixture_land("b", {{"The {noun} of {object}", 1}});
    b.lists[0] = {"Weald"};
    b.lists[7] = {"Crows", "Floods"};
    d.defs = {a, b, c};
    const rn::PlaceDef* pa = &d.defs[0];
    const rn::PlaceDef* pb = &d.defs[1];
    const rn::NameTuning t{};
    const std::uint64_t key = rn::k_name(0, 64);
    // The first region id whose attempt 0 in land b is The Weald of Floods, and its first attempt giving
    // The Weald of Crows.
    std::int64_t rid = 2;
    while (rn::attempt(key, rid, 0, *pb, d.common(), {}, t).name != "The Weald of Floods") {
        ++rid;
        REQUIRE(rid < 400);
    }
    std::int64_t want = 1;
    while (rn::attempt(key, rid, want, *pb, d.common(), {}, t).name != "The Weald of Crows") {
        ++want;
        REQUIRE(want < 64);
    }
    // Keepers of the two nouns (larger), so that both small regions draw.
    const auto got = rn::name_regions(rows_of({{1, 10, "North Thicket", pa}, {rid, 10, "North Weald", pb},
                                               {rid + 1, 99, "North Thicket", pa}, {rid + 2, 99, "North Weald", pb}}),
                                      0, 64, d, t);
    REQUIRE(got.has_value());
    CHECK(got->names[0] == "The Flooded Thicket");
    CHECK(got->how[0] == "0");
    CHECK(got->names[1] == "The Weald of Crows");
    CHECK(got->how[1] == std::to_string(want));
    // A keeper adds nothing to S.
    const auto k = rn::name_regions(rows_of({{rid, 10, "North Weald", pb}, {rid + 1, 99, "South Weald", pb}}), 0, 64,
                                    d, t);
    REQUIRE(k.has_value());
    CHECK(k->names[0] == "The Weald of Floods");
    CHECK(k->how[0] == "0");
}

// 15.6 (r9): "Thornholt then Elmholt".
TEST_CASE("P1-06 r6 15.4, 15.6: V7 lets nouns and suffixes repeat (Thornholt, Elmholt); a fallback adds nothing to S") {
    rn::PlaceData d;
    rn::PlaceDef c = fixture_common();
    c.lists[2] = {"Flood"};  // common prefixes: reached by the fallback only (land d's forms draw land prefixes)
    rn::PlaceDef holt = fixture_land("c", {{"{prefix}{suffix}", 1}});
    holt.lists[2] = {"Elm", "Thorn"};
    holt.lists[1] = {"holt"};
    rn::PlaceDef dn = fixture_land("d", {{"{prefix}{suffix}", 1}});
    dn.lists[2] = {"Birch"};
    dn.lists[1] = {"hurst"};  // Birch + hurst fails V1 at every attempt
    rn::PlaceDef b = fixture_land("e", {{"The {noun} of {object}", 1}});
    b.lists[0] = {"Weald"};
    b.lists[7] = {"Crows", "Floods"};
    d.defs = {holt, c, dn, b};
    rn::NameTuning t{};
    t.pools[2] = {0, 3, 0};  // prefixes from the land pool only
    const auto h = rn::name_regions(rows_of({{1, 10, "North Holt", &d.defs[0]}, {2, 10, "East Holt", &d.defs[0]},
                                             {3, 99, "South Holt", &d.defs[0]}}),
                                    0, 64, d, t);
    REQUIRE(h.has_value());
    std::vector<std::string> two{h->names[0], h->names[1]};
    std::sort(two.begin(), two.end());
    CHECK(two == std::vector<std::string>{"Elmholt", "Thornholt"});
    const std::uint64_t key = rn::k_name(0, 64);
    std::int64_t rid = 2;
    while (rn::attempt(key, rid, 0, d.defs[3], d.common(), {}, t).name != "The Weald of Floods") {
        ++rid;
        REQUIRE(rid < 400);
    }
    const auto f = rn::name_regions(rows_of({{1, 10, "North Dene", &d.defs[2]}, {rid, 10, "North Weald", &d.defs[3]},
                                             {rid + 1, 99, "South Dene", &d.defs[2]},
                                             {rid + 2, 99, "South Weald", &d.defs[3]}}),
                                    0, 64, d, t);
    REQUIRE(f.has_value());
    CHECK(f->names[0] == "Floodhurst");
    CHECK(f->how[0] == "F");
    CHECK(f->names[1] == "The Weald of Floods");
    CHECK(f->how[1] == "0");
}

TEST_CASE("P1-06 r6 15.1 to 15.3, r10 16.1, 15.6 data: church words out, grim words moved, hanger, High, Gap and Cleft") {
    const rn::PlaceDef& c = common();
    auto has = [](const std::vector<std::string>& v, const std::string& w) {
        return std::find(v.begin(), v.end(), w) != v.end();
    };
    // 15.1
    for (const char* w : {"Abbess", "Anchorite", "Martyr", "Penitent", "Pilgrim", "Prophet", "Saint", "Sexton"}) {
        CHECK_MESSAGE(!has(c.lists[5], w), w);
    }
    for (const char* w : {"Martyrs", "Penance", "Pilgrims", "Prayers", "Saints", "the Unshriven"}) {
        CHECK_MESSAGE(!has(c.lists[7], w), w);
    }
    CHECK(!has(c.lists[2], "Rood"));
    CHECK(!has(c.lists[2], "Tithe"));
    CHECK(!has(c.lists[6], "Penance"));
    CHECK(!has(c.lists[6], "Tithe"));
    CHECK(!has(land("trait_fertile").lists[7], "Tithes"));
    CHECK(has(c.lists[2], "Hallow"));
    CHECK(has(c.lists[5], "Hermit"));
    CHECK(has(c.lists[5], "Bellringer"));
    CHECK(!has(c.lists[3], "Godless"));   // r10 16.1: moved to trait_barren
    // 15.2 with r10 16.1: the grim words kept in common (the six of spirits and curses)
    const std::vector<std::pair<std::size_t, std::vector<std::string>>> kept = {
        {2, {"Ghost", "Wraith"}}, {3, {"Accursed", "Cursed"}}, {6, {}}, {5, {}}, {7, {"Ghosts", "Shades"}}};
    // r10 16.1: the 34 words moved from common to trait_barren (the 40 r6 kept but the six above)
    const std::map<std::size_t, std::vector<std::string>> moved10 = {
        {2, {"Bale", "Charnel", "Dread", "Grief", "Lich", "Pall", "Sorrow", "Wight"}},
        {3, {"Bloodless", "Doleful", "Drear", "Fallen", "Forsaken", "Godless", "Grim", "Joyless", "Moaning",
             "Orphaned", "Shunned", "Stricken", "Weeping", "Wretched"}},
        {6, {"Bane", "Grief", "Shame"}},
        {5, {"Gravedigger", "Mourner", "Widower"}},
        {7, {"Bones", "Grief", "Widows", "the Damned", "the Forsaken", "the Slain"}}};
    const std::map<std::size_t, std::vector<std::string>> moved = {
        {2, {"Bane", "Bier", "Dirge", "Dole", "Drear", "Gallow", "Gibbet", "Grave", "Grim", "Knell", "Lorn", "Mourn",
             "Pyre", "Shroud", "Tomb", "Weep", "Wither", "Woe"}},
        {3, {"Baleful", "Bereft", "Cheerless", "Crumbling", "Desolate", "Dismal", "Dolorous", "Dread", "Dreary",
             "Dying", "Forgotten", "Forlorn", "Friendless", "Ghostly", "Grieving", "Grievous", "Grisly", "Haunted",
             "Lamenting", "Lost", "Mouldering", "Mourning", "Pitiless", "Ruined", "Sorrowing", "Spectral",
             "Unhallowed", "Wailing", "Widowed", "Woeful"}},
        {6, {"Dole", "Grave", "Lament", "Ruin", "Sorrow"}},
        {5, {"Hangman", "Headsman", "Orphan", "Widow"}},
        {7, {"Curses", "Gallows", "Gibbets", "Graves", "Mourners", "Ruin", "Sorrows", "Tombs", "Wights", "Wraiths",
             "the Fallen", "the Forgotten", "the Hanged", "the Lost", "the Unburied", "the Weeping"}}};
    const rn::PlaceDef& barren = land("trait_barren");
    for (const auto& [li, words] : kept) {
        CAPTURE(li);
        for (const std::string& w : words) {
            CHECK_MESSAGE(has(c.lists[li], w), w);
        }
        const auto& mv = moved.at(li);
        const std::map<std::size_t, std::size_t> counts = {{2, 18}, {3, 30}, {6, 5}, {5, 4}, {7, 16}};
        CHECK(mv.size() == counts.at(li));
        for (const std::string& w : mv) {
            CHECK_MESSAGE(!has(c.lists[li], w), w);
            CHECK_MESSAGE(has(barren.lists[li], w), w);
        }
        for (const std::string& w : moved10.at(li)) {
            CHECK_MESSAGE(!has(c.lists[li], w), w);
            CHECK_MESSAGE(has(barren.lists[li], w), w);
        }
        // trait_barren's lists stay byte-sorted without repeats after the merge (16.1)
        std::vector<std::string> sorted_list = barren.lists[li];
        std::sort(sorted_list.begin(), sorted_list.end());
        sorted_list.erase(std::unique(sorted_list.begin(), sorted_list.end()), sorted_list.end());
        CHECK(sorted_list == barren.lists[li]);
    }
    std::size_t n10 = 0;
    for (const auto& [li, words] : moved10) n10 += words.size();
    CHECK(n10 == 34U);
    // 16.1: the land lists are not edited; bleak and dark words stay in common
    CHECK(has(land("carr").lists[3], "Weeping"));
    CHECK(has(land("steppe").lists[7], "Bones"));
    for (const char* w : {"Black", "Gloom", "Murk"}) CHECK_MESSAGE(has(c.lists[2], w), w);
    CHECK(has(c.lists[3], "Ashen"));
    // trait_barren keeps its own words, sorted, Dole once
    for (const char* w : {"Dearth", "Famine", "Hunger", "Starve", "Want"}) {
        CHECK(has(barren.lists[2], w));
    }
    CHECK(std::count(barren.lists[6].begin(), barren.lists[6].end(), "Dole") == 1);
    // 15.3
    CHECK(!has(land("wood").lists[1], "hanger"));
    CHECK(!has(land("trait_high").lists[3], "High"));
    CHECK(!has(c.lists[6], "Cleft"));
    CHECK(!has(c.lists[6], "Gap"));
    CHECK(has(land("trait_high").lists[6], "Cleft"));
    CHECK(has(land("trait_pass").lists[6], "Gap"));
}

// --- 10.5 unit 3: validity ----------------------------------------------------------------------

TEST_CASE("P1-06 r3 RG11 7, 10.5 unit 3: V1 to V6, each with a passing and a failing case") {
    // V1 joins: the same letter; two vowels; more than 12 bytes.
    CHECK(rule_of("{prefix}{suffix}", {"Birch", "hurst"}) == "V1");
    CHECK(rule_of("{prefix}{suffix}", {"Bramble", "alder"}) == "V1");
    CHECK(rule_of("{prefix}{suffix}", {"Hemlock", "hollow"}) == "V1");  // 13
    CHECK(rule_of("{prefix}{suffix}", {"Bramble", "edge"}) == "V1");
    CHECK(rule_of("{prefix}{suffix}", {"Hallow", "well"}) == "V1");
    CHECK(rule_of("{prefix}{suffix}", {"Adder", "bog"}).empty());
    CHECK(rule_of("{prefix}{suffix}", {"Thistle", "horn"}).empty());   // 11
    CHECK(rule_of("{prefix}{suffix}", {"Bracken", "stone"}).empty());  // 12
    // V2 no triple letters (a fixture prefix; V1 rules out a triple across a join).
    CHECK(rule_of("{prefix}{suffix}", {"Brrr", "holt"}) == "V2");
    CHECK(rule_of("{prefix}{suffix}", {"Brr", "holt"}).empty());
    // V3 no element inside another.
    CHECK(rule_of("The {adjective} {noun}", {"Hollow", "Hollow"}) == "V3");
    CHECK(rule_of("{prefix}{feature} {noun}", {"Fen", "moss", "Moss"}) == "V3");
    CHECK(rule_of("The {noun} of {object}", {"Wood", "Woods"}) == "V3");
    CHECK(rule_of("The {noun} of {object}", {"Wood", "Crows"}).empty());
    // V4 forbidden: inside a compound, inside a word with an apostrophe.
    const rn::PlaceDef c4 = fixture_common({"rot"});
    CHECK(rule_of("{prefix}{suffix}", {"Ferrot", "wood"}, c4) == "V4");
    CHECK(rule_of("{possessor}'s {place}", {"Rotter", "Reach"}, c4) == "V4");
    CHECK(rule_of("{possessor}'s {place}", {"Otter", "Reach"}, c4).empty());
    // V5 excluded: the whole name; a one-word entry as a word; a two-word
    // entry only as the whole name.
    CHECK(rule_of("The {adjective} {noun}", {"Haunted", "Forest"}) == "V5");
    CHECK(rule_of("{prefix}{feature} {noun}", {"Black", "rock", "Downs"}) == "V5");
    CHECK(rule_of("{prefix}{suffix}", {"Black", "wood"}) == "V5");
    CHECK(rule_of("{prefix}{suffix}", {"Holly", "wood"}) == "V5");
    const rn::PlaceDef c5 = fixture_common({}, {"The Grey Downs"});
    CHECK(rule_of("The {adjective} {noun}", {"Grey", "Downs"}, c5) == "V5");
    CHECK(rule_of("The {adjective} {noun} of {object}", {"Grey", "Downs", "Crows"}, c5).empty());
    // V6 at most 48 bytes: a literal of 45 bytes and a slot of 3 or 4.
    const std::string lit = "Ab cd ef gh ij kl mn op qr st uv wx yz ab cd e";  // 46 bytes, no triple letter
    REQUIRE(lit.size() == 46U);
    const std::vector<rn::Token> t48 = {rn::Token{false, lit}, rn::Token{true, "noun"}};
    CHECK(rn::invalid_rule(t48, {"Fo"}, fixture_common()).empty());   // 48
    CHECK(rn::invalid_rule(t48, {"Fox"}, fixture_common()) == "V6");  // 49
#if UNMAPPED_P3_08_API
    CHECK_FALSE(region_api::name_valid(seam_pn(), "{prefix}{suffix}", {"Birch", "hurst"}));
    CHECK_FALSE(region_api::name_valid(seam_pn(), "{prefix}{suffix}", {"Hemlock", "hollow"}));
    CHECK(region_api::name_valid(seam_pn(), "{prefix}{suffix}", {"Bracken", "stone"}));
    CHECK_FALSE(region_api::name_valid(seam_pn(), "The {adjective} {noun}", {"Hollow", "Hollow"}));
    CHECK_FALSE(region_api::name_valid(seam_pn(), "The {adjective} {noun}", {"Haunted", "Forest"}));
    CHECK_FALSE(region_api::name_valid(seam_pn(), "{prefix}{feature} {noun}", {"Black", "rock", "Downs"}));
    CHECK(region_api::name_valid(seam_pn(), "{prefix}{suffix}", {"Adder", "bog"}));
#endif
}

// --- 14.8: boner and dung ----------------------------------------------------------------------

TEST_CASE("P1-06 r4 14.8: common.forbidden gains two entries; they reject compounds such as B*nereach and Dungrove") {
    const rn::PlaceDef& c = common();
    CHECK(c.forbidden.size() == 21U);
    CHECK(std::is_sorted(c.forbidden.begin(), c.forbidden.end()));
    CHECK(std::count(c.forbidden.begin(), c.forbidden.end(), std::string("bo") + "ner") == 1);
    CHECK(std::count(c.forbidden.begin(), c.forbidden.end(), std::string("du") + "ng") == 1);
    // Fixture prefix and suffix lists: Bone + reach, Dun + grove.
    CHECK(rule_of("{prefix}{suffix}", {"Bone", "reach"}) == "V4");
    CHECK(rule_of("{prefix}{suffix}", {"Dun", "grove"}) == "V4");
    CHECK(rule_of("{prefix}{suffix}", {"Bone", "ridge"}) == "V4");
    CHECK(rule_of("{prefix}{suffix}", {"Dun", "gate"}) == "V4");
    // Without the r4 entries (r3's 19) they passed.
    rn::PlaceDef r3 = c;
    r3.forbidden.erase(std::remove_if(r3.forbidden.begin(), r3.forbidden.end(),
                                      [](const std::string& f) { return f == "bo" "ner" || f == "du" "ng"; }),
                       r3.forbidden.end());
    CHECK(r3.forbidden.size() == 19U);
    CHECK(rule_of("{prefix}{suffix}", {"Bone", "reach"}, r3).empty());
    CHECK(rule_of("{prefix}{suffix}", {"Dun", "grove"}, r3).empty());
    // No single list word contains either.
    for (const rn::PlaceDef& d : pd().defs) {
        for (const auto& l : d.lists) {
            for (const std::string& w : l) {
                const std::string low = rn::lower(w);
                CHECK_MESSAGE(low.find("bo" "ner") == std::string::npos, w);
                CHECK_MESSAGE(low.find("du" "ng") == std::string::npos, w);
            }
        }
    }
#if UNMAPPED_P3_08_API
    CHECK_FALSE(region_api::name_valid(seam_pn(), "{prefix}{suffix}", {"Bone", "reach"}));
    CHECK_FALSE(region_api::name_valid(seam_pn(), "{prefix}{suffix}", {"Dun", "grove"}));
#endif
}

// --- 10.5 unit 4: traits ------------------------------------------------------------------------

TEST_CASE("P1-06 r3 RG11 5, 10.5 unit 4: each trait one step either side of its threshold; never values") {
    const rn::NameTuning t{};
    auto has = [&](const rn::TraitRow& r, const std::string& tr) {
        const auto v = rn::traits_of(r, t);
        return std::find(v.begin(), v.end(), tr) != v.end();
    };
    rn::TraitRow r;  // defaults: no trait (habitability 255, fertility 0)
    CHECK(rn::traits_of(r, t).empty());
    r = {};
    r.coast_cells = 128;
    CHECK(has(r, "coast"));
    r.coast_cells = 127;
    CHECK_FALSE(has(r, "coast"));
    r = {};
    r.river_class_max = 7;
    CHECK(has(r, "river"));
    r.river_class_max = 6;
    CHECK_FALSE(has(r, "river"));
    r = {};
    r.cells = 1000;
    r.lake_cells = 125;
    CHECK(has(r, "lake"));
    r.lake_cells = 124;
    CHECK_FALSE(has(r, "lake"));
    r = {};
    r.elev_mean_dm = 4000;
    CHECK(has(r, "high"));
    r.elev_mean_dm = 3999;
    CHECK_FALSE(has(r, "high"));
    r = {};
    r.ore_rich_cells = 1000;
    CHECK(has(r, "ore"));
    r.ore_rich_cells = 999;
    CHECK_FALSE(has(r, "ore"));
    r = {};
    r.pass = true;
    CHECK(has(r, "pass"));
    r = {};
    r.habitability_mean = 20;
    CHECK(has(r, "barren"));
    r.habitability_mean = 21;
    CHECK_FALSE(has(r, "barren"));
    r = {};
    r.fertility_mean = 200;
    CHECK(has(r, "fertile"));
    r.fertility_mean = 199;
    CHECK_FALSE(has(r, "fertile"));
    // The "never" values.
    rn::NameTuning nv{};
    nv.lake_permille = 1;
    nv.river_class = 16;
    nv.barren_hab_max = -1;
    nv.fertile_min = 256;
    rn::TraitRow z;
    z.lake_cells = 0;
    z.cells = 1;
    z.river_class_max = 15;
    z.habitability_mean = 0;
    z.fertility_mean = 255;
    CHECK(rn::traits_of(z, nv).empty());
    z.lake_cells = 1;
    z.cells = 1000;
    CHECK(rn::traits_of(z, nv) == std::vector<std::string>{"lake"});  // 1000 x 1 >= 1 x 1000
    // Every trait at once comes in trait order.
    rn::TraitRow all;
    all.coast_cells = 200;
    all.river_class_max = 9;
    all.cells = 100;
    all.lake_cells = 50;
    all.elev_mean_dm = 5000;
    all.ore_rich_cells = 2000;
    all.pass = true;
    all.habitability_mean = 0;
    all.fertility_mean = 255;
    CHECK(rn::traits_of(all, t) ==
          std::vector<std::string>{"coast", "river", "lake", "high", "ore", "pass", "barren", "fertile"});
}

TEST_CASE("P1-06 r3 RG11 6, 10.5 unit 4: the trait pool concatenates in trait order, not in id order") {
    rn::PlaceData d;
    rn::PlaceDef a;  // id sorts first, trait last in trait order
    a.id = "a_fertile";
    a.role = "trait";
    a.trait = "fertile";
    a.lists[2] = {"Wheat"};
    rn::PlaceDef z;
    z.id = "z_coast";
    z.role = "trait";
    z.trait = "coast";
    z.lists[2] = {"Gull", "Tern"};
    d.defs = {a, z};
    const auto td = rn::trait_defs(d, {"coast", "fertile"});
    const rn::PlaceDef l = fixture_land("l", {{"{prefix}{suffix}", 1}});
    const rn::Pools p = rn::pools_of(2, l, fixture_common(), td, rn::NameTuning{});
    CHECK(p.p[2] == std::vector<std::string>{"Gull", "Tern", "Wheat"});
    // The shipped traits too.
    const auto st = traits({"coast", "river"});
    REQUIRE(st.size() == 2U);
    CHECK(st[0]->trait == "coast");
    CHECK(st[1]->trait == "river");
}

// --- 10.5 unit 5: pools ------------------------------------------------------------------------

TEST_CASE("P1-06 r3 RG11 6, 10.5 unit 5: an empty pool weighs 0; weighted at a prefix-sum boundary") {
    rn::PlaceDef l = fixture_land("l", {{"The {adjective} {noun}", 1}});
    l.lists[0] = {"Wold"};
    // No land adjectives: weights [6, 0, 0] (no traits): every adjective from the common pool.
    const rn::Pools p = rn::pools_of(3, l, common(), {}, rn::NameTuning{});
    CHECK(p.w == std::vector<std::int64_t>{6, 0, 0});
    for (std::int64_t r = 1; r <= 20; ++r) {
        const rn::Attempt a = rn::attempt(rn::k_name(7, 128), r, 0, l, common(), {}, rn::NameTuning{});
        REQUIRE(a.steps.size() == 2U);
        CHECK(a.steps[0].pool == 0U);
        CHECK(a.steps[1].pool == 1U);  // nouns [0, 1, 0]
    }
    // P3-02 4.1: the smallest i with (u >> 32) W < 2^32 (w0 + ... + wi).
    const std::vector<std::int64_t> w = {1, 1};
    CHECK(rn::weighted(((std::uint64_t{1} << 31U) - 1U) << 32U, w) == 0U);
    CHECK(rn::weighted(std::uint64_t{1} << 63U, w) == 1U);  // high word exactly 2^31: the boundary
    const std::vector<std::int64_t> w3 = {6, 3, 2};
    // (u >> 32) = ceil(6 x 2^32 / 11) is the first high word of pool 1.
    const std::uint64_t b = ((std::uint64_t{6} << 32U) + 10U) / 11U;
    CHECK(rn::weighted((b - 1U) << 32U, w3) == 0U);
    CHECK(rn::weighted(b << 32U, w3) == 1U);
    CHECK(rn::weighted(~std::uint64_t{0}, w3) == 2U);
    CHECK(rn::weighted(0, std::vector<std::int64_t>{0, 5}) == 1U);
    CHECK(rn::pick(~std::uint64_t{0}, 156) == 155U);
    CHECK(rn::pick(0, 156) == 0U);
}

// --- 10.5 unit 6: collision and fallback -----------------------------------------------------------

TEST_CASE("P1-06 r3 RG11 8, 10.5 unit 6: three names for four drawn regions: the fourth falls back; fatal when full") {
    rn::PlaceData d;
    rn::PlaceDef c = fixture_common();
    c.lists[2] = {"Ash"};  // common prefixes
    rn::PlaceDef l = fixture_land("tiny", {{"{noun}", 1}});
    l.lists[0] = {"Alpha", "Beta", "Gamma"};
    l.lists[1] = {"ford"};
    l.lists[2] = {"Elm"};
    d.defs = {c, l};
    const rn::PlaceDef* lp = &d.defs[1];
    std::vector<rn::NamedRegion> regions;
    for (std::int64_t id = 1; id <= 5; ++id) {
        rn::NamedRegion r;
        r.id = id;
        r.cells = id == 1 ? 100 : 10;
        r.base = "East Tiny";
        r.land = lp;
        regions.push_back(r);
    }
    const auto got = rn::name_regions(regions, 0, 128, d, rn::NameTuning{});
    REQUIRE(got.has_value());
    CHECK(got->names[0] == "East Tiny");
    const std::set<std::string> three(got->names.begin() + 1, got->names.begin() + 4);
    CHECK(three == std::set<std::string>{"Alpha", "Beta", "Gamma"});
    CHECK(got->names[4] == "Ashford");  // P = Ash, Elm; S = ford: the first free compound
    CHECK(got->how[4] == "F");
    // Two more regions: Elmford, then nothing is free: fatal (no free name).
    for (std::int64_t id = 6; id <= 7; ++id) {
        rn::NamedRegion r = regions.back();
        r.id = id;
        regions.push_back(r);
    }
    CHECK_FALSE(rn::name_regions(regions, 0, 128, d, rn::NameTuning{}).has_value());
    regions.pop_back();
    const auto six = rn::name_regions(regions, 0, 128, d, rn::NameTuning{});
    REQUIRE(six.has_value());
    CHECK(six->names[5] == "Elmford");
}

TEST_CASE("P1-06 r3 RG11 8, 10.5 unit 6: a drawn name equal to a keeper's base name is rejected") {
    rn::PlaceData d;
    rn::PlaceDef c = fixture_common();
    c.lists[2] = {"Ash"};
    rn::PlaceDef l = fixture_land("wood", {{"East {noun}", 1}});
    l.lists[0] = {"Wood"};
    l.lists[1] = {"wold"};
    d.defs = {c, l};
    std::vector<rn::NamedRegion> regions(2);
    regions[0].id = 1;
    regions[0].cells = 50;
    regions[0].base = "East Wood";
    regions[0].land = &d.defs[1];
    regions[1] = regions[0];
    regions[1].id = 2;
    regions[1].cells = 40;
    const auto got = rn::name_regions(regions, 3, 128, d, rn::NameTuning{});
    REQUIRE(got.has_value());
    CHECK(got->names == std::vector<std::string>{"East Wood", "Ashwold"});
    CHECK(got->how == std::vector<std::string>{"K", "F"});
}

// --- 10.5 unit 9: the fallback order on the shipped fen land ---------------------------------------

TEST_CASE("P1-06 r3 5.1, 10.5 unit 9: the fallback of land fen with an empty U is Adderbog") {
    CHECK(land("fen").lists[1] == std::vector<std::string>{"bog", "fen", "mire", "moss", "quag", "sike"});
    CHECK(land("fen").lists[2].size() == 11U);
    CHECK(rn::fallback(land("fen"), common(), {}) == std::optional<std::string>("Adderbog"));
    CHECK(rn::fallback(land("fen"), common(), {"Adderbog"}) == std::optional<std::string>("Adderfen"));
#if UNMAPPED_P3_08_API
    CHECK(region_api::name_fallback(seam_pn(), "fen", {}) == "Adderbog");
#endif
}

// --- 14.4: every region of 1024 seed 42 -----------------------------------------------------------

// The drawn rows of 14.4 (1024 seed 42), attempt 0 with each row's land and traits, on the 0.21.0 lists
// (values from the test writer's reference at 0.21.0; 14.4's names are those of the 0.18.0 lists).
TEST_CASE("P1-06 r3 14.4, r10 16: attempt 0 of the drawn rows of 1024 seed 42 on the 0.21.0 lists") {
    // (id, base, cells rank is irrelevant here: the drawn rows of 14.4 with
    // their land, traits and accepted attempt; keepers are not drawn.)
    struct Row {
        std::int64_t id;
        std::string land;
        std::vector<std::string> traits;
        std::int64_t attempt;
        std::string name;
    };
    const std::uint64_t key = rn::k_name(42, 1024);
    for (const Row& r : std::vector<Row>{{1, "downs", {"coast"}, 0, "The Sighing Downs"},
                                         {2, "downs", {"coast"}, 0, "The Starved Plain"},
                                         {3, "downs", {}, 0, "The Rises of Hares"},
                                         {4, "downs", {"fertile"}, 0, "Warlord's Rises"},
                                         {12, "downs", {"river", "fertile"}, 0, "Witchgate Barrows"},
                                         {17, "wood", {"coast"}, 0, "Wrecker's Reach"},
                                         {19, "wood", {}, 0, "The Shaw of Thieves"},
                                         {30, "downs", {"river", "lake", "fertile"}, 0, "Umberdown"},
                                         {37, "wood", {}, 0, "The Brake of Worms"},
                                         {41, "downs", {"high", "ore", "pass", "barren"}, 0, "Larkfell"},
                                         {49, "wood", {"coast", "barren"}, 0, "The Gnarled Forest"}}) {
        CAPTURE(r.id);
        const rn::Attempt a = rn::attempt(key, r.id, r.attempt, land(r.land), common(), traits(r.traits), rn::NameTuning{});
        CHECK(a.valid());
        CHECK(a.name == r.name);
    }
}

// --- Data (10.5 "Data tests", 4.1) -------------------------------------------------------------

TEST_CASE("P1-06 r3 4.1: the shipped placename files: 15 files, one common, 13 lands, 8 traits, sizes") {
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(rn::shipped_dir())) {
        if (e.path().extension() == ".toml") {
            files.push_back(e.path().filename().string());
        }
    }
    std::sort(files.begin(), files.end());
    CHECK(files == std::vector<std::string>{"carr.toml",  "common.toml", "downs.toml",  "fells.toml",
                                            "fen.toml",   "heath.toml",  "isle.toml",   "marsh.toml",
                                            "mere.toml",  "moor.toml",   "peaks.toml",  "steppe.toml",
                                            "traits.toml", "waste.toml", "wood.toml"});
    std::map<std::string, int> roles;
    for (const rn::PlaceDef& d : pd().defs) {
        ++roles[d.role];
    }
    CHECK(roles == std::map<std::string, int>{{"common", 1}, {"land", 13}, {"trait", 8}});
    // placename_common_sizes: at least 100 prefixes, 100 adjectives, 16
    // features, 32 possessors, 16 places, 32 objects.
    const rn::PlaceDef& c = common();
    CHECK(c.lists[2].size() >= 100U);
    CHECK(c.lists[3].size() >= 100U);
    CHECK(c.lists[4].size() >= 16U);
    CHECK(c.lists[5].size() >= 32U);
    CHECK(c.lists[6].size() >= 16U);
    CHECK(c.lists[7].size() >= 32U);
    // The shipped sizes from 0.21.0 (P1-06 r10 16.1 after r6 15.1 to 15.3; 4.1's 156, 159, 33, 94, 52, 78 before
    // r6): prefixes 156 - 2 church - 18 grim (r6) - 8 (r10) = 128, adjectives 159 - 30 - 14 = 115, possessors
    // 94 - 8 - 4 - 3 = 79, places 52 - 2 - 5 - 2 (Cleft, Gap) - 3 = 40, objects 78 - 6 - 16 - 6 = 50.
    CHECK(c.lists[2].size() == 128U);
    CHECK(c.lists[3].size() == 115U);
    CHECK(c.lists[4].size() == 33U);
    CHECK(c.lists[5].size() == 79U);
    CHECK(c.lists[6].size() == 40U);
    CHECK(c.lists[7].size() == 50U);
    CHECK(c.excluded.size() == 90U);
    // Every land has the six forms with weights 30, 22, 22, 12, 9, 5; each
    // trait definition names its trait; every non-water biome in one land;
    // the three specials.
    for (const rn::PlaceDef& d : pd().defs) {
        CAPTURE(d.id);
        if (d.role == "land") {
            std::vector<std::int64_t> w;
            for (const rn::Form& f : d.forms) {
                w.push_back(f.weight);
                CHECK(rn::parse_form(f.form).has_value());
            }
            CHECK(w == std::vector<std::int64_t>{30, 22, 22, 12, 9, 5});
            CHECK((d.biomes.empty() != (d.special == "none")));
        }
        if (d.role == "trait") {
            CHECK(d.id == "trait_" + d.trait);
        }
    }
    for (const char* s : {"isle", "lake", "upland"}) {
        CHECK(pd().special(s) != nullptr);
    }
    for (const auto& b : region_test::definitions().biomes) {
        if (!b.water) {
            CAPTURE(b.key);
            CHECK(pd().land_of_biome(b.key) != nullptr);
        }
    }
}

TEST_CASE("P1-06 r3 10.5 data: every list sorted (entries with a space after the others); no duplicates") {
    auto check_sorted = [](const std::string& where, const std::vector<std::string>& v) {
        std::vector<std::string> one, more;
        for (const std::string& s : v) {
            (s.find(' ') == std::string::npos ? one : more).push_back(s);
        }
        std::vector<std::string> want = one;
        std::sort(want.begin(), want.end());
        std::vector<std::string> m = more;
        std::sort(m.begin(), m.end());
        want.insert(want.end(), m.begin(), m.end());
        CHECK_MESSAGE(v == want, where);
        CHECK_MESSAGE(std::set<std::string>(v.begin(), v.end()).size() == v.size(), where);
    };
    for (const rn::PlaceDef& d : pd().defs) {
        for (std::size_t i = 0; i < rn::LISTS.size(); ++i) {
            check_sorted(d.id + "." + std::string(rn::LISTS[i]), d.lists[i]);
        }
        check_sorted(d.id + ".forbidden", d.forbidden);
        check_sorted(d.id + ".excluded", d.excluded);
    }
}

TEST_CASE("P1-06 r3 4.1, 10.5 data: the fixture roots hold byte-identical copies of game/data/placenames") {
    namespace fs = std::filesystem;
    const fs::path root(UNMAPPED_TEST_SOURCE_ROOT);
    for (const char* fixture : {"geo", "geo-small"}) {
        CAPTURE(fixture);
        const fs::path dir = root / "worldgen" / "tests" / "data" / fixture / "placenames";
        REQUIRE_MESSAGE(fs::is_directory(dir), "missing " << dir.string() << " (the engineer adds it, P1-06 r4 14.8)");
        std::set<std::string> a, b;
        for (const auto& e : fs::directory_iterator(rn::shipped_dir())) {
            a.insert(e.path().filename().string());
        }
        for (const auto& e : fs::directory_iterator(dir)) {
            b.insert(e.path().filename().string());
        }
        CHECK(a == b);
        for (const std::string& f : a) {
            if (b.count(f) != 0U) {
                CAPTURE(f);
                CHECK(rn::file_text(rn::shipped_dir() / f) == rn::file_text(dir / f));
            }
        }
    }
}

// --- P0-01 amendment 20: the stream registry ---------------------------------------------------------

TEST_CASE("P0-01 amendment 20, P1-06 r4 14.8: geo.regions.name directly after geo.regions.cost, 52 names") {
    const auto& names = worldgen::streams::all_stream_names;
    CHECK(names.size() == 107U)  /* P5-04 3: five hist.alliances streams (0.26.0) */;  // P5-03a 2: hist.relations.settlers; P3-06 r10 20.17: hist.conflict.search (0.18.0); r22 24.5 hist.conflict.toll (0.21.0); P5-12 3 two; P5-02 3 sixteen
    // P5-12 3 (0.22.0): hist.renaming.chance and hist.renaming.name appended after hist.conflict.toll, in that order;
    // P5-02 3 (0.23.0) fifteen more after them, then (r1) hist.monsters.raid_target.
    // P5-03 3 (0.24.0): hist.beasts.bold_kind, hist.monsters.fear, hist.peoples.abandon_loss, hist.relations.expel.
    // P5-03a 2 (0.25.0): hist.relations.settlers after hist.relations.expel.
    // P5-04 3 (0.26.0): hist.alliances.form, .join, .trade, .spoils and .mine after hist.relations.settlers.
    CHECK(names[names.size() - 54] == "hist.conflict.toll");
    CHECK(names[names.size() - 53] == "hist.renaming.chance");
    CHECK(names[names.size() - 52] == "hist.renaming.name");
    CHECK(names[names.size() - 36] == "hist.monsters.raid_target");
    CHECK(names[names.size() - 32] == "hist.relations.expel");
    CHECK(names[names.size() - 31] == "hist.relations.settlers");
    CHECK(names[names.size() - 30] == "hist.alliances.form");
    CHECK(names[names.size() - 29] == "hist.alliances.join");
    CHECK(names[names.size() - 28] == "hist.alliances.trade");
    CHECK(names[names.size() - 27] == "hist.alliances.spoils");
    CHECK(names[names.size() - 26] == "hist.alliances.mine");
    CHECK(names[names.size() - 10] == "hist.alliances.renew");   // P5-04a 3 (0.28.0)
    CHECK(names[names.size() - 9] == "hist.deep.delve");   // P5-07 r2 3 (0.29.0): nine streams last
    CHECK(names[names.size() - 1] == "hist.deep.warren_kind");
    CHECK(names[names.size() - 11] == "hist.monsters.hero_fall");   // P5-06 3 (0.27.0): fifteen after hist.alliances.mine
    std::size_t at = names.size();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == "geo.regions.cost") {
            at = i;
        }
    }
    REQUIRE(at + 1 < names.size());
    CHECK(names[at + 1] == "geo.regions.name");
    CHECK(std::count(names.begin(), names.end(), std::string_view("geo.regions.name")) == 1);
}

#if UNMAPPED_P3_08_API

// --- 10.5 unit 7: the loader (P0-03 forms of each message) ------------------------------------------

namespace {

worldgen::data::MemoryDirectory shipped_pn_dir(const std::vector<std::pair<std::string, std::string>>& replace = {},
                                              const std::vector<std::pair<std::string, std::string>>& add = {}) {
    namespace wd = worldgen::data;
    wd::MemoryDirectory d{"placenames", {}};
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(rn::shipped_dir())) {
        if (e.path().extension() == ".toml") {
            files.push_back(e.path().filename().string());
        }
    }
    std::sort(files.begin(), files.end());
    for (const std::string& f : files) {
        std::string text = rn::file_text(rn::shipped_dir() / f);
        for (const auto& [name, t] : replace) {
            if (name == f) {
                text = t;
            }
        }
        d.entries.push_back(wd::MemoryEntry{f, text, false});
    }
    for (const auto& [name, t] : add) {
        d.entries.push_back(wd::MemoryEntry{name, t, false});
    }
    return d;
}

std::string errors_of(const region_api::Load& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

void rejected_with(const region_api::Load& l, const std::string& text) {
    REQUIRE_FALSE(l.ok());
    CHECK_MESSAGE(errors_of(l).find(text) != std::string::npos, errors_of(l) << " lacks " << text);
}

// One extra definition (any role) in its own file.
std::string def_toml(const std::string& id, const std::string& body) {
    return "[placename." + id + "]\n" + body;
}

const std::string LAND_OK =
    "role = \"land\"\nbiomes = []\nspecial = \"none\"\ntrait = \"none\"\nforms = [{ form = \"{noun}\", weight = 1 }]\n"
    "nouns = [\"Holt\"]\nsuffixes = [\"holt\"]\nprefixes = []\nadjectives = []\nfeatures = []\npossessors = []\n"
    "places = []\nobjects = []\nforbidden = []\nexcluded = []\n";

std::string land_with(const std::string& field, const std::string& value) {
    std::string out;
    std::size_t pos = 0;
    while (pos < LAND_OK.size()) {
        const std::size_t nl = LAND_OK.find('\n', pos);
        const std::string line = LAND_OK.substr(pos, nl - pos);
        pos = nl + 1;
        out += (line.rfind(field + " = ", 0) == 0 ? field + " = " + value : line) + "\n";
    }
    return out;
}

}  // namespace

TEST_CASE("P1-06 r3 4.1, 10.5 unit 7: the shipped placename files load; a root without placenames/ is refused") {
    const region_api::Load ok = region_api::load_with_placenames(shipped_pn_dir());
    INFO(errors_of(ok));
    CHECK(ok.ok());
    rejected_with(region_api::load_with_placenames(std::nullopt), "placename");
}

TEST_CASE("P1-06 r3 4.1, 10.5 unit 7: each grammar fails once") {
    const std::string wood = rn::file_text(rn::shipped_dir() / "wood.toml");
    // wood.toml with a string put first in one of its arrays.
    auto wood_with = [&](const std::string& field, const std::string& first) {
        const std::size_t p = wood.find(field + " = [");
        REQUIRE(p != std::string::npos);
        const std::size_t q = p + field.size() + 4;
        return wood.substr(0, q) + first + ", " + wood.substr(q);
    };
    // `Wood land` as a word, `Holt` as an element, `The Crows` as an object,
    // `{noun} {noun}` and `{nouns}` as forms.
    for (const auto& [field, first] : std::vector<std::pair<std::string, std::string>>{
             {"nouns", "\"Wood land\""},
             {"suffixes", "\"Holt\""},
             {"objects", "\"The Crows\""},
             {"forms", "{ form = \"{noun} {noun}\", weight = 1 }"},
             {"forms", "{ form = \"{nouns}\", weight = 1 }"}}) {
        CAPTURE(field);
        CAPTURE(first);
        const region_api::Load l = region_api::load_with_placenames(shipped_pn_dir({{"wood.toml", wood_with(field, first)}}));
        CHECK_FALSE(l.ok());
        CHECK(errors_of(l).find("wood.toml") != std::string::npos);
    }
    // A double space in an excluded name.
    const std::string common_text = rn::file_text(rn::shipped_dir() / "common.toml");
    const std::size_t at = common_text.find("\"Ashdown\"");
    REQUIRE(at != std::string::npos);
    const std::string dbl = common_text.substr(0, at) + "\"Ash  down\", " + common_text.substr(at);
    const region_api::Load l = region_api::load_with_placenames(shipped_pn_dir({{"common.toml", dbl}}));
    CHECK_FALSE(l.ok());
    CHECK(errors_of(l).find("common.toml") != std::string::npos);
}

TEST_CASE("P1-06 r3 4.1, 10.5 unit 7: each named and kind check of 4.1 fails once with its message") {
    const std::string pre = "failed for 'placename.zz': ";
    // A land with both biomes and a special.
    {
        std::string t = land_with("biomes", "[\"temperate_forest\"]");
        t = t.substr(0, t.find("special")) + "special = \"isle\"\n" + t.substr(t.find("trait"));
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({}, {{"zz.toml", def_toml("zz", t)}})),
                      pre + "a land definition must name biomes or a special, not both");
    }
    // A trait with nouns.
    {
        const std::string t =
            "role = \"trait\"\nbiomes = []\nspecial = \"none\"\ntrait = \"coast\"\nforms = []\nnouns = [\"Holt\"]\n"
            "suffixes = []\nprefixes = []\nadjectives = []\nfeatures = []\npossessors = []\nplaces = []\nobjects = []\n"
            "forbidden = []\nexcluded = []\n";
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({}, {{"zz.toml", def_toml("zz", t)}})),
                      pre + "a trait definition must have nouns empty");
    }
    // A common with forms.
    {
        std::string c = rn::file_text(rn::shipped_dir() / "common.toml");
        const std::size_t f = c.find("forms = []");
        REQUIRE(f != std::string::npos);
        c = c.substr(0, f) + "forms = [{ form = \"{noun}\", weight = 1 }]" + c.substr(f + 10);
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({{"common.toml", c}})),
                      "failed for 'placename.common': a common definition must have forms empty");
    }
    // A word twice in prefixes.
    {
        std::string w = rn::file_text(rn::shipped_dir() / "wood.toml");
        const std::size_t p = w.find("prefixes = [");
        REQUIRE(p != std::string::npos);
        const std::size_t q = w.find('"', p);
        const std::size_t e = w.find('"', q + 1);
        const std::string first = w.substr(q, e - q + 1);
        const std::string word = first.substr(1, first.size() - 2);
        w = w.substr(0, q) + first + ", " + w.substr(q);
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({{"wood.toml", w}})),
                      "failed for 'placename.wood': prefixes[1] '" + word + "' repeats prefixes[0]");
    }
    // Two common definitions.
    {
        std::string c = rn::file_text(rn::shipped_dir() / "common.toml");
        c.replace(c.find("[placename.common]"), 18, "[placename.common2]");
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({}, {{"zz.toml", c}})),
                      "there must be exactly one common definition, found 2");
    }
    // `fen` in two lands; `special` `isle` twice.
    {
        const std::string t = land_with("biomes", "[\"fen\"]");
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({}, {{"zz.toml", def_toml("zz", t)}})),
                      "biome 'fen' is in 'fen' and 'zz'");
        std::string s = LAND_OK;
        s.replace(s.find("special = \"none\""), 16, "special = \"isle\"");
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({}, {{"zz.toml", def_toml("zz", s)}})),
                      "special 'isle' is in 'isle' and 'zz'");
    }
    // A common with 99 prefixes.
    {
        std::string c = rn::file_text(rn::shipped_dir() / "common.toml");
        const std::size_t p = c.find("prefixes = [");
        const std::size_t e = c.find(']', p);
        std::string list = "prefixes = [";
        for (int i = 0; i < 99; ++i) {
            std::string w = "Pa";
            w += static_cast<char>('a' + i / 26);
            w += static_cast<char>('a' + i % 26);
            list += (i ? ", \"" : "\"") + w + "\"";
        }
        c = c.substr(0, p) + list + c.substr(e);
        rejected_with(region_api::load_with_placenames(shipped_pn_dir({{"common.toml", c}})),
                      "the common definition needs at least 100 prefixes, has 99");
    }
}

TEST_CASE("P1-06 r3 4, 10.5 unit 7: the tuning records name_pools and name_traits, one step outside, wrong lengths") {
    auto load = [](const std::string& field, const std::string& value) {
        return region_api::load(region_data::tuning_toml({{field, value}}), biome_data::directory());
    };
    CHECK(load("name_traits", UNMAPPED_P1_06_R3_TRAITS).ok());
    for (const auto& [f, lo, hi] : std::vector<std::tuple<std::string, std::int64_t, std::int64_t>>{
             {"coast_cells", 0, 100000000}, {"river_class", 1, 16},       {"lake_permille", 1, 1000},
             {"high_mean_m", 0, 10000},     {"ore_rich_cells", 0, 100000000}, {"barren_hab_max", -1, 255},
             {"fertile_min", 0, 256}}) {
        CAPTURE(f);
        const std::string fld = f;
        auto with = [fld](std::int64_t v) {
            std::string t = UNMAPPED_P1_06_R3_TRAITS;
            const std::size_t p = t.find(fld + " = ");
            const std::size_t e = t.find_first_of(",}", p);
            return t.substr(0, p) + fld + " = " + std::to_string(v) + t.substr(e);
        };
        CHECK(load("name_traits", with(lo)).ok());
        CHECK(load("name_traits", with(hi)).ok());
        CHECK_FALSE(load("name_traits", with(lo - 1)).ok());
        CHECK_FALSE(load("name_traits", with(hi + 1)).ok());
    }
    const std::string pools = std::string(UNMAPPED_P1_06_R3_POOLS).substr(20);
    CHECK(load("name_pool_weights", pools).ok());
    std::string two = pools, four = pools, big = pools;
    two.replace(two.find("[0, 1, 0]"), 9, "[0, 1]");
    four.replace(four.find("[0, 1, 0]"), 9, "[0, 1, 0, 0]");
    big.replace(big.find("[0, 1, 0]"), 9, "[0, 1001, 0]");
    CHECK_FALSE(load("name_pool_weights", two).ok());
    CHECK_FALSE(load("name_pool_weights", four).ok());
    CHECK_FALSE(load("name_pool_weights", big).ok());
}

// --- 10.5 unit 8: stage check 4a --------------------------------------------------------------------

TEST_CASE("P1-06 r3 2 check 4a, 10.5 unit 8: each message") {
    using region_test::case_inputs;
    const region_test::GoldenCase* gc = region_test::find_case("designed-0");
    REQUIRE(gc != nullptr);
    const region_test::CaseInputs ci = case_inputs(*gc);
    auto stage_error = [&](const worldgen::data::MemoryDirectory& dir,
                           const region_api::Tuning& tuning) -> std::string {
        const region_api::Load l = region_api::load_with_placenames(dir);
        REQUIRE_MESSAGE(l.ok(), errors_of(l));
        const region_api::Placenames pn = region_api::placenames_of(*l.content);
        const worldgen::stages::RegionInputs ri{ci.in.grid,  ci.in.elevation, ci.in.water,       ci.in.water_depth,
                                                ci.in.flow_dir, ci.in.river,  ci.in.biome,       ci.in.fertility,
                                                ci.in.ore,   ci.in.timber,    ci.in.fresh_water, ci.in.forage,
                                                ci.in.habitability, ci.in.walk_area};
        try {
            static_cast<void>(worldgen::stages::regions_stage(ri, region_test::definitions(), pn, {},
                                                              tuning, gc->seed, 1));
        } catch (const worldgen::FatalError& e) {
            return e.what();
        }
        return "";
    };
    const region_api::Tuning t = region_test::tuning(gc->overrides);
    // A biome with no land definition (marsh.toml emptied).
    CHECK(stage_error(shipped_pn_dir({{"marsh.toml", ""}}), t) ==
          "regions: placename: biome 'marsh' has no land definition");
    // No upland land.
    CHECK(stage_error(shipped_pn_dir({{"fells.toml", ""}}), t) ==
          "regions: placename: no land definition for 'upland'");
    // name_pool_weights.features = [0, 1, 1]: downs (the first land in id
    // order without features) has a {feature} slot in its form 2.
    std::string pools = std::string(UNMAPPED_P1_06_R3_POOLS).substr(20);
    pools.replace(pools.find("features = [5, 1, 2]"), 20, "features = [0, 1, 1]");
    const region_api::Load tl =
        region_api::load(region_data::tuning_toml({{"name_pool_weights", pools}}), biome_data::directory());
    REQUIRE(tl.ok());
    CHECK(stage_error(shipped_pn_dir(), region_api::tuning_from(*tl.content)) ==
          "regions: placename: 'downs' form 2 slot '{feature}' has no words");
    CHECK(stage_error(shipped_pn_dir(), t).empty());
}

// --- 10.5 property: data layout changes no name ------------------------------------------------------

TEST_CASE("P1-06 r3 10.5, P0-03 7.4: renaming a placename file, moving a definition or reordering traits changes no name") {
    const region_test::GoldenCase* gc = region_test::find_case("shipped-128-s0-t");
    REQUIRE(gc != nullptr);
    const region_test::CaseInputs ci = region_test::case_inputs(*gc);
    auto names_with = [&](const worldgen::data::MemoryDirectory& dir) {
        const region_api::Load l = region_api::load_with_placenames(dir);
        REQUIRE_MESSAGE(l.ok(), errors_of(l));
        const region_api::Placenames pn = region_api::placenames_of(*l.content);
        const worldgen::stages::RegionInputs ri{ci.in.grid,  ci.in.elevation, ci.in.water,       ci.in.water_depth,
                                                ci.in.flow_dir, ci.in.river,  ci.in.biome,       ci.in.fertility,
                                                ci.in.ore,   ci.in.timber,    ci.in.fresh_water, ci.in.forage,
                                                ci.in.habitability, ci.in.walk_area};
        const auto r = worldgen::stages::regions_stage(ri, region_test::definitions(), pn,
                                                       region_api::pass_rows(ci.passes),
                                                       region_test::tuning(gc->overrides), gc->seed, 2);
        std::vector<std::string> out;
        for (const auto& x : r.regions) {
            out.push_back(x.name);
        }
        return out;
    };
    const std::vector<std::string> base = names_with(shipped_pn_dir());
    // Renamed file.
    {
        worldgen::data::MemoryDirectory d = shipped_pn_dir();
        for (auto& e : d.entries) {
            if (e.name == "wood.toml") {
                e.name = "aaa-wood.toml";
            }
        }
        CHECK(names_with(d) == base);
    }
    // The fen definition moved into wood.toml.
    {
        const std::string fen = rn::file_text(rn::shipped_dir() / "fen.toml");
        const std::string wood = rn::file_text(rn::shipped_dir() / "wood.toml");
        CHECK(names_with(shipped_pn_dir({{"fen.toml", ""}, {"wood.toml", wood + "\n" + fen}})) == base);
    }
    // traits.toml with its definitions in reverse order.
    {
        const std::string t = rn::file_text(rn::shipped_dir() / "traits.toml");
        std::vector<std::string> blocks;
        std::size_t p = t.find("[placename.");
        while (p != std::string::npos) {
            const std::size_t q = t.find("[placename.", p + 1);
            blocks.push_back(t.substr(p, q == std::string::npos ? std::string::npos : q - p));
            p = q;
        }
        std::string rev;
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            rev += *it + "\n";
        }
        CHECK(names_with(shipped_pn_dir({{"traits.toml", rev}})) == base);
    }
}

#endif  // UNMAPPED_P3_08_API
