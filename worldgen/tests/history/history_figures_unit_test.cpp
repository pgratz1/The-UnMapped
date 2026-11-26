// Spec P5-06 (docs/specs/p5-06-figures.md r3) 18.1: the spec's arithmetic by hand (items 3, 9, 10, 11, 17, 22 and
// 11.4's glyph), the data of 4 (item 1) and the version, schema, catalogue, groups and glyph the library gives.
//
// The cases of the first suite compute the spec's formulas here, from the spec's text, on hand-checked values
// (the same values as worldgen/tests/reference/p5_06_figures_reference.py units): they run at 0.26.0 too and give
// the engineer the expected numbers. The cases that read the library (the data loader, the glyph, the groups, the
// version) start with P506_REQUIRE_INTEGRATED() (p5_06_figures_api.hpp) and run from integration on. Written from
// the spec before the implementation.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p5_03_relations_checks.hpp"
#include "p5_06_figures_api.hpp"
#include "p5_06_figures_data.hpp"
#include "worldgen/core/roman.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/data_checks.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/markers.hpp"
#include "worldgen/version.hpp"

namespace {

namespace wd = worldgen::data;
namespace h = worldgen::history;
namespace ins = worldgen::inspect;
using namespace history_test;

// --- the spec's formulas (6.3, 6.9, 7.2, 7.4, 7.6, 7.7, 25.3), integers only --------------------------------

// pick(u, n) of P3-02 4.1: ((u >> 32) n) >> 32.
std::int64_t pick(std::uint64_t u, std::int64_t n) {
    return static_cast<std::int64_t>(((u >> 32U) * static_cast<std::uint64_t>(n)) >> 32U);
}
constexpr std::uint64_t U_MIN = 0U;
constexpr std::uint64_t U_MAX = ~std::uint64_t{0};

// 6.3: a reign's length and accession age from two draws.
std::int64_t reign_len(std::int64_t g, std::uint64_t u) { return std::max<std::int64_t>(1, g * (500 + pick(u, 1001)) / 1000); }
std::int64_t accession_age(std::int64_t lam, std::uint64_t u) { return lam * (250 + pick(u, 251)) / 1000; }

// 6.9: (born, death) of a non-ruler entering at y with A, S its age fields.
std::pair<std::int64_t, std::int64_t> life(std::int64_t lam, std::int64_t aw, std::int64_t y, std::int64_t A,
                                           std::int64_t S, std::uint64_t u0, std::uint64_t u1) {
    const std::int64_t age0 = lam * (A + pick(u0, S + 1)) / 1000;
    const std::int64_t born = std::max(aw, y - age0);
    return {born, std::max(y + 1, born + lam * (700 + pick(u1, 501)) / 1000)};
}

// 7.2: mean_r and strain_pm with the spec's starting temper_strain_permille (1000, 1300, 700, 1000, 1000), and with
// the shipped (calibrated, 15.2 step 4) one (1000, 2000, 0, 1000, 1000).
constexpr std::array<std::int64_t, 5> TSP = {1000, 1300, 700, 1000, 1000};
constexpr std::array<std::int64_t, 5> TSP_SHIPPED = {1000, 2000, 0, 1000, 1000};
std::int64_t mean_r(const std::array<std::int64_t, 5>& w, const std::array<std::int64_t, 5>& f) {
    std::int64_t a = 0, b = 0;
    for (std::size_t i = 0; i < 5; ++i) {
        a += w[i] * f[i];
        b += w[i];
    }
    return a / b;
}
std::int64_t strain_pm(std::size_t tau, const std::array<std::int64_t, 5>& w, const std::array<std::int64_t, 5>& f) {
    return f[tau] * 1000 / std::max<std::int64_t>(1, mean_r(w, f));
}

// 7.6: the led win chance.
std::int64_t led_p(std::int64_t A, std::int64_t D, std::int64_t dv, std::int64_t sx, std::int64_t sy) {
    const std::int64_t A1 = A * (1000 + sx) / 1000;
    const std::int64_t D1 = D * dv / 1000 * (1000 + sy) / 1000;
    return A1 + D1 > 0 ? A1 * 1000 / (A1 + D1) : 500;
}

// 25.3: the undead threshold on S(r).
std::int64_t undead_T(std::int64_t min_dead, std::int64_t permille, std::int64_t P_W, std::int64_t M, std::int64_t R,
                      std::int64_t ref) {
    std::int64_t tpop = P_W * permille / 1000;
    if (ref > 0) tpop = tpop * ref / std::max(ref, R);
    return std::max(min_dead, tpop) * 1000 / std::max<std::int64_t>(1000, M);
}

const p506data::RaceFields& race(std::string_view k) {
    for (const auto& r : p506data::RACES) {
        if (r.race == k) return r;
    }
    FAIL("no race " << std::string(k));
    return p506data::RACES[0];
}

// 6.10: one figure's epithet over the shipped entries (role, deed, min, substantial, text); borne: earlier bearers.
struct Entry {
    const char* role;
    const char* deed;
    std::int64_t min, substantial;
    const char* text;
};
const std::vector<Entry>& entries() {
    static const std::vector<Entry> e = {
        {"ruler", "conquests", 1, 2, "the Conqueror"},     {"ruler", "betrayals", 1, 1, "the Faithless"},
        {"ruler", "realm_fell", 1, 0, "the Last"},         {"ruler", "wars_won", 2, 4, "the Victorious"},
        {"ruler", "reign_permille", 1300, 1450, "the Old"}, {"ruler", "founded_years", 1, 3000, "the Founder"},
        {"general", "battles_unbeaten", 3, 6, "the Unbeaten"}, {"general", "battles_won", 5, 10, "the Hammer"},
        {"hero", "slew_might", 1, 1200, "{slayer}"},      {"claimant", "exiled", 1, 0, "the Exile"},
        {"caster", "blights", 1, 1, "the Accursed"},       {"caster", "books", 3, 0, "the Learned"}};
    return e;
}
std::string epithet(const std::string& role, const std::map<std::string, std::int64_t>& v, const std::string& slayer,
                    const std::map<std::string, std::int64_t>& borne) {
    for (const Entry& e : entries()) {
        if (role != e.role) continue;
        const auto it = v.find(e.deed);
        const std::int64_t x = it == v.end() ? 0 : it->second;
        const std::string t = std::string(e.text) == "{slayer}" ? slayer : std::string(e.text);
        if (t.empty()) continue;
        if (e.substantial > 0 && x >= e.substantial) return t;
        const auto b = borne.find(t);
        if (x >= e.min && (b == borne.end() ? 0 : b->second) < 2) return t;
    }
    return "";
}

std::string display(const std::string& name, std::int64_t regnal, const std::string& ep) {
    std::string out = name;
    if (regnal >= 2) out += " " + worldgen::roman_numeral(regnal);
    if (!ep.empty()) out += " " + ep;
    return out;
}

// 11.4: the tower glyph from its rule (2 fill, 3 ink, then P4-04 5's halo: the empty cells next to a fill or ink
// cell, eight-neighbourhood, inside radius 3), row-major from (-3, -3).
std::vector<std::uint8_t> tower_rule() {
    std::vector<std::uint8_t> g(49, 0);
    const auto at = [&](int dx, int dy) -> std::uint8_t& { return g[static_cast<std::size_t>((dy + 3) * 7 + dx + 3)]; };
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            if (dx == 0 && dy >= -1 && dy <= 1) {
                at(dx, dy) = 2;
            } else if ((std::abs(dx) == 1 && dy >= -1 && dy <= 2) || (dx == 0 && (dy == -2 || dy == 2))) {
                at(dx, dy) = 3;
            }
        }
    }
    std::vector<std::uint8_t> h = g;
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            if (at(dx, dy) != 0) continue;
            bool near = false;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const int x = dx + i, y = dy + j;
                    if (x >= -3 && x <= 3 && y >= -3 && y <= 3 && (at(x, y) == 2 || at(x, y) == 3)) near = true;
                }
            }
            if (near) h[static_cast<std::size_t>((dy + 3) * 7 + dx + 3)] = 1;
        }
    }
    return h;
}
const char* const TOWER_ROWS[7] = {"..hhh..", ".hhKhh.", ".hKFKh.", ".hKFKh.", ".hKFKh.", ".hKKKh.", ".hhhhh."};

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-06 7.2, 18.1 item 11: the temper normalization by race and the zero tuning's neutral factors") {
    CHECK(mean_r(race("goblin").temper, TSP) == 1075);
    CHECK(strain_pm(1, race("goblin").temper, TSP) == 1209);
    CHECK(strain_pm(2, race("goblin").temper, TSP) == 651);
    CHECK(mean_r(race("elf").temper, TSP) == 955);
    CHECK(mean_r(race("orc").temper, TSP) == 1135);
    // the calibrated values, by hand: goblin (300 + 600 + 0 + 300 + 50) = 1250, warlike 2000000 div 1250 = 1600
    CHECK(mean_r(race("goblin").temper, TSP_SHIPPED) == 1250);
    CHECK(strain_pm(1, race("goblin").temper, TSP_SHIPPED) == 1600);
    CHECK(strain_pm(2, race("goblin").temper, TSP_SHIPPED) == 0);
    CHECK(mean_r(race("elf").temper, TSP_SHIPPED) == 850);
    CHECK(mean_r(race("orc").temper, TSP_SHIPPED) == 1450);
    CHECK(contains(std::string(p506data::FIGURES_TOML),
                   "temper_strain_permille = { steady = 1000, warlike = 2000, peaceable = 0, grasping = 1000, just = 1000 }"));
    for (const auto& r : p506data::RACES) {
        CAPTURE(std::string(r.race));
        std::int64_t a = 0, b = 0;
        for (std::size_t t = 0; t < 5; ++t) {
            a += r.temper[t] * strain_pm(t, r.temper, TSP);
            b += r.temper[t];
        }
        CHECK(std::abs(a / b - 1000) <= 2);   // each race's average rate as at 0.26.0 (rounding only)
        std::int64_t a2 = 0;
        for (std::size_t t = 0; t < 5; ++t) a2 += r.temper[t] * strain_pm(t, r.temper, TSP_SHIPPED);
        CHECK(std::abs(a2 / b - 1000) <= 2);
        for (std::size_t t = 0; t < 5; ++t) CHECK(strain_pm(t, r.temper, {1000, 1000, 1000, 1000, 1000}) == 1000);
    }
}

TEST_CASE("P5-06 6.3, 6.9, 18.1 items 3 and 9: reigns and lives at the extremes of their draws") {
    // elf g 150, lambda 700; goblin 15, 40; human 25, 70 (P3-01's shipped races)
    CHECK(reign_len(150, U_MIN) == 75);
    CHECK(reign_len(150, U_MAX) == 225);
    CHECK(reign_len(15, U_MIN) == 7);
    CHECK(reign_len(15, U_MAX) == 22);
    CHECK(reign_len(1, U_MIN) == 1);   // at least one year
    CHECK(accession_age(70, U_MIN) == 17);
    CHECK(accession_age(70, U_MAX) == 35);
    // 6.9's statement: an elven general commands at 210 to 420 and dies at 490 to 840, a goblin one at 12 to 24
    // and 28 to 48 (ages at entry and at death)
    for (const auto& [lam, a0, a1, d0, d1] : std::vector<std::array<std::int64_t, 5>>{{700, 210, 420, 490, 840},
                                                                                     {40, 12, 24, 28, 48}}) {
        CAPTURE(lam);
        const auto lo = life(lam, 0, 5000, 300, 300, U_MIN, U_MIN);
        const auto hi = life(lam, 0, 5000, 300, 300, U_MAX, U_MAX);
        CHECK(5000 - lo.first == a0);
        CHECK(5000 - hi.first == a1);
        CHECK(lo.second - lo.first == d0);
        CHECK(hi.second - hi.first == d1);
    }
    CHECK(life(700, 4900, 5000, 300, 300, U_MAX, U_MIN).first == 4900);   // born clamped at the awakening
}

TEST_CASE("P5-06 7.4, 7.6, 7.7: the split bonus, the generals' win chance, the hero's hunt") {
    // 7.4: shipped short_reign_permille 500, short_reign_split_permille 100; human g 25: short below 12 years
    const auto bonus = [](std::int64_t Y, std::int64_t s) { return Y - s < 500 * 25 / 1000 ? 100 : 0; };
    CHECK(bonus(1011, 1000) == 100);
    CHECK(bonus(1012, 1000) == 0);
    CHECK(bonus(1000, 1000) == 100);   // a crisis in the accession year (6.3)
    // 7.6
    CHECK(led_p(1000, 1000, 1000, 0, 0) == 500);
    CHECK(led_p(1000, 1000, 1000, 300, 0) == 565);
    CHECK(led_p(1000, 1000, 1500, 100, 200) == 379);
    CHECK(led_p(0, 0, 1000, 300, 300) == 500);
    // 7.7: p_hunt = min(max + h, base + (R - 1) raid + h), S = H prowess div 1000 (1000 + might) div 1000
    const auto p_hunt = [](std::int64_t base, std::int64_t mx, std::int64_t raid, std::int64_t R, std::int64_t h) {
        return std::min(mx + h, base + (std::max<std::int64_t>(1, R) - 1) * raid + h);
    };
    CHECK(p_hunt(20, 200, 5, 3, 30) == 60);
    CHECK(p_hunt(20, 200, 100, 9, 30) == 230);
    CHECK(1000 * 1200 / 1000 * (1000 + 500) / 1000 == 1800);
}

TEST_CASE("P5-06 25.3, 18.1 item 22: the undead threshold at M 500, 1000, 1600 and R 64, 65, 200") {
    const std::int64_t P = 2000000;   // P_W x 25 div 1000 = 50000 above the floor 1000
    CHECK(undead_T(1000, 25, P, 500, 40, 64) == 50000);    // low: as normal (max(1000, M))
    CHECK(undead_T(1000, 25, P, 1000, 40, 64) == 50000);
    CHECK(undead_T(1000, 25, P, 1600, 40, 64) == 31250);   // high: lowered as at 0.26.0
    CHECK(undead_T(1000, 25, P, 1000, 64, 64) == 50000);
    CHECK(undead_T(1000, 25, P, 1000, 65, 64) == 49230);
    CHECK(undead_T(1000, 25, P, 1000, 200, 64) == 16000);
    CHECK(undead_T(1000, 25, P, 1000, 200, 0) == 50000);   // ref 0: 0.26.0's rule
    CHECK(undead_T(1000, 25, 1000, 1000, 200, 64) == 1000);   // the floor undead_min_dead
    CHECK(undead_T(1000, 25, 1000, 1600, 200, 64) == 625);
}

TEST_CASE("P5-06 6.10, 18.1 item 10: the epithet rule (order, min, the cap on ordinary deeds, substantial deeds)") {
    using M = std::map<std::string, std::int64_t>;
    CHECK(epithet("ruler", {}, "", {}).empty());
    CHECK(epithet("ruler", M{{"conquests", 1}}, "", {}) == "the Conqueror");
    CHECK(epithet("ruler", M{{"conquests", 1}, {"realm_fell", 1}}, "", M{{"the Conqueror", 2}}) == "the Last");
    CHECK(epithet("ruler", M{{"conquests", 2}}, "", M{{"the Conqueror", 2}}) == "the Conqueror");   // a third
    CHECK(epithet("ruler", M{{"conquests", 2}}, "", M{{"the Conqueror", 3}}) == "the Conqueror");   // a fourth
    CHECK(epithet("ruler", M{{"conquests", 1}}, "", M{{"the Conqueror", 2}}).empty());
    CHECK(epithet("ruler", M{{"realm_fell", 1}}, "", M{{"the Last", 2}}).empty());   // substantial 0
    CHECK(epithet("ruler", M{{"conquests", 1}, {"wars_won", 5}}, "", {}) == "the Conqueror");
    CHECK(epithet("ruler", M{{"reign_permille", 1300}}, "", {}) == "the Old");
    CHECK(epithet("ruler", M{{"reign_permille", 1299}}, "", {}).empty());
    CHECK(epithet("hero", M{{"slew_might", 6000}}, "Dragonslayer", M{{"Dragonslayer", 5}}) == "Dragonslayer");
    CHECK(epithet("hero", M{{"slew_might", 700}}, "Trollbane", M{{"Trollbane", 2}}).empty());
    CHECK(epithet("general", M{{"battles_won", 10}, {"battles_unbeaten", 10}}, "", {}) == "the Unbeaten");
    CHECK(epithet("general", M{{"battles_won", 5}}, "", {}) == "the Hammer");
    CHECK(epithet("caster", M{{"books", 3}, {"blights", 1}}, "", M{{"the Accursed", 9}}) == "the Accursed");
}

TEST_CASE("P5-06 6.14, 18.1 item 17: roman numerals II to XXXIX and display names with numbers and epithets") {
    CHECK(worldgen::roman_numeral(2) == "II");
    CHECK(worldgen::roman_numeral(4) == "IV");
    CHECK(worldgen::roman_numeral(9) == "IX");
    CHECK(worldgen::roman_numeral(14) == "XIV");
    CHECK(worldgen::roman_numeral(39) == "XXXIX");
    std::size_t longest = 0;
    for (std::int64_t k = 2; k <= 39; ++k) longest = std::max(longest, worldgen::roman_numeral(k).size());
    CHECK(longest == 7U);   // XXXVIII
    CHECK(display("Balin", 2, "the Victorious") == "Balin II the Victorious");
    CHECK(display("Maela", 0, "Dragonslayer") == "Maela Dragonslayer");
    CHECK(display("Aldric", 3, "") == "Aldric III");
    CHECK(display("Balin", 0, "") == "Balin");
    // FG9: 48 + 8 + 1 + 24 = 81 bytes at most
    CHECK(display(std::string(48, 'a'), 38, std::string(24, 'b')).size() == 81U);
}

TEST_CASE("P5-06 11.4: the tower glyph's grid from its rule equals the drawing; 20 halo, 3 fill, 10 ink") {
    const std::vector<std::uint8_t> g = tower_rule();
    std::array<int, 5> n{};
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 7; ++x) {
            const std::uint8_t v = g[static_cast<std::size_t>(y * 7 + x)];
            ++n[v];
            const char c = TOWER_ROWS[y][x];
            CHECK(v == (c == 'h' ? 1 : c == 'F' ? 2 : c == 'K' ? 3 : 0));
        }
    }
    CHECK(n[1] == 20);
    CHECK(n[2] == 3);
    CHECK(n[3] == 10);
    CHECK(n[4] == 0);
}

TEST_CASE("P5-06 11.4: the library's glyph upper left of its town, a mine's upper right, a wild tower at its cell") {
    const auto offset = [](int kind, std::int64_t settlement) {
        if (kind == 3) return std::pair<int, int>{7, -7};
        if (kind == 4) return settlement != 0 ? std::pair<int, int>{-7, -7} : std::pair<int, int>{0, 0};
        return std::pair<int, int>{0, 0};
    };
    CHECK(offset(3, 5) == std::pair<int, int>{7, -7});
    CHECK(offset(4, 5) == std::pair<int, int>{-7, -7});
    CHECK(offset(4, 0) == std::pair<int, int>{0, 0});
}

// --- the library (from integration on) ----------------------------------------------------------------------

TEST_CASE("P5-06 9.4, 3, 8.1: generator 0.27.0, schema 12") {
    P506_REQUIRE_INTEGRATED();
    CHECK(worldgen::GENERATOR_VERSION == std::string_view(p506api::GENERATOR_SPEC));
    CHECK(static_cast<std::int64_t>(worldgen::db::SCHEMA_VERSION) == p506api::SCHEMA_SPEC);
}

TEST_CASE("P5-06 11.3: kinds 79 to 92 in their browser groups; ALL_GROUPS 0x1FF; the word figures") {
    P506_REQUIRE_INTEGRATED();
    const std::map<std::string, std::uint8_t> want = {
        {"came_to_rule", 8}, {"figure_died", 8},  {"succession_disputed", 8}, {"exiled", 8},  {"reigned_at_end", 8},
        {"general_appointed", 8}, {"battle_led", 2}, {"fell_in_battle", 8}, {"hero_rose", 8},
        {"monster_slain_by_hero", 5}, {"hero_slain", 8}, {"tower_founded", 8}, {"book_written", 8}, {"blight", 3},
        {"ruin_faded", 1}};   // 25.1: kind 19 stays in settlements
    for (const auto& [k, g] : want) {
        CAPTURE(k);
        CHECK(static_cast<int>(ins::event_group(k)) == static_cast<int>(g));
    }
    CHECK(ins::ALL_GROUPS == p506api::ALL_GROUPS_SPEC);
    CHECK(ins::event_group_word(p506api::figures_group()) == "figures");
}

TEST_CASE("P5-06 11.4: marker_glyph(20) is the tower glyph of radius 3") {
    P506_REQUIRE_INTEGRATED();
    ins::GlyphKey k;
    k.base = p506api::GLYPH_TOWER_SPEC;
    const ins::Glyph g = ins::marker_glyph(k);
    CHECK(g.radius == 3);
    CHECK(g.cells == tower_rule());
}

TEST_CASE("P5-06 4, 18.1 item 1: the 0.27.0 data load; each new figures field required; the named checks") {
    P506_REQUIRE_INTEGRATED();
    history_data::Files f = history_data::shipped_files();
    for (const auto& [path, text] : p506data::files_027()) f[path] = text;
    f.erase("history/figures.toml");
    REQUIRE_FALSE(load(f).ok());   // history.figures is required
    f["history/figures.toml"] = std::string(p506data::FIGURES_TOML);
    const Load l = load(f);
    REQUIRE_MESSAGE(l.ok(), dump(l));
    CHECK(h::check_history_data(*l.content).empty());
    const auto with = [&](const std::string& from, const std::string& to) {
        history_data::Files g = f;
        std::string& t = g["history/figures.toml"];
        const std::size_t at = t.find(from);
        REQUIRE(at != std::string::npos);
        t.replace(at, from.size(), to);
        return load(g);
    };
    const Load bad = with("skill_min = 500", "skill_min = 801");   // the shipped 500 and 800 (15.2 step 3)
    REQUIRE_FALSE(bad.ok());
    CHECK(contains(dump(bad), "check 'figures_skill_ordered' failed for 'history.figures': skill_min 801 is above "
                              "skill_max 800"));
    CHECK(with("skill_min = 500", "skill_min = 800").ok());
    const Load th = with("min = 1, substantial = 2, text = \"the Conqueror\"", "min = 3, substantial = 2, text = \"the "
                                                                              "Conqueror\"");
    REQUIRE_FALSE(th.ok());
    CHECK(contains(dump(th), "epithets[0]: substantial 2 is below min 3"));
    const Load sl = with("deed = \"slew_might\"", "deed = \"books\"");
    REQUIRE_FALSE(sl.ok());
    CHECK(contains(dump(sl), "epithets[8]: {slayer} needs the deed slew_might"));
    CHECK_FALSE(with("regnal_max = 9", "regnal_max = 1").ok());     // int(2, 39)
    CHECK_FALSE(with("regnal_max = 9", "regnal_max = 40").ok());
    CHECK(with("regnal_max = 9", "regnal_max = 39").ok());
    CHECK_FALSE(with("name_rulers = true", "name_rulers = 1").ok());   // bool
    // 25.1 (a), (b): a root that still has ruin_fade_years or ghost_fade_years is refused as an unknown key
    for (const auto& n : p506data::PEOPLES_REMOVED) {
        history_data::Files g = f;
        g["history/peoples.toml"] += std::string(n) + " = 0\n";
        const Load u = load(g);
        REQUIRE_FALSE(u.ok());
        CHECK(contains(dump(u), "unknown key 'history.peoples." + std::string(n) + "'"));
    }
}

TEST_CASE("P5-06 4.1 to 4.3: the race, culture and monster fields: values and the named checks") {
    P506_REQUIRE_INTEGRATED();
    history_data::Files f = history_data::shipped_files();
    for (const auto& [path, text] : p506data::files_027()) f[path] = text;
    const Load l = load(f);
    REQUIRE_MESSAGE(l.ok(), dump(l));
    for (const auto& r : p506data::RACES) {
        const wd::Definition* d = p503check::definition(*l.content, "race", std::string(r.race).c_str());
        REQUIRE(d != nullptr);
        CHECK(d->record.find("general_permille")->as_int() == r.general);
        CHECK(d->record.find("hero_permille")->as_int() == r.hero);
        CHECK(d->record.find("magic_permille")->as_int() == r.magic);
    }
    for (const auto& c : p506data::CULTURES) {
        const wd::Definition* d = p503check::definition(*l.content, "culture", std::string(c.culture).c_str());
        REQUIRE(d != nullptr);
        CHECK(d->record.find("naming")->as_record().find("regnal_permille")->as_int() == c.regnal_permille);
    }
    for (const auto& s : p506data::SLAYERS) {
        const wd::Definition* d = p503check::definition(*l.content, "monster", std::string(s.kind).c_str());
        REQUIRE(d != nullptr);
        CHECK(d->record.find("slayer_epithet")->as_string() == s.epithet);
    }
    // race_temper_weights: the sum above 0
    history_data::Files g = f;
    std::string& elf = g["races/elf.toml"];
    const std::size_t at = elf.find("temper_weights = ");
    REQUIRE(at != std::string::npos);
    elf.replace(at, elf.find('\n', at) - at,
                "temper_weights = { steady = 0, warlike = 0, peaceable = 0, grasping = 0, just = 0 }");
    const Load z = load(g);
    std::string msgs = dump(z);
    if (z.ok()) {
        for (const auto& e : h::check_history_data(*z.content)) msgs += e.message + "\n";
    }
    CHECK(contains(msgs, "temper_weights must not all be 0"));
}

}  // TEST_SUITE
