// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2) test 16 item 1 on the shipped data root
// (game/data): every new field of history.alliances (3.7, 4.4, 5), history.relations (7.2, 8) and history.figures
// (14.2a) present in the spec's order after the field it follows, at its shipped value, refused one below its low
// bound and one above its high bound (the loader's message `'<key>' is <v>; expected an integer from <lo> to <hi>`)
// and refused when absent (`missing required field`); the changed values (withheld_to_break 2, withheld_grudge 250;
// the Old 1400 and 1480, skill 600 to 900, grasping allied strain 3000: 14.1, 14.2a, the lead's rulings L-7 and L-8).
// Shipped values: those of the calibration (docs/reviews/p5-04a-calibration.md, data only), where it moved the
// spec's start value (the start value is named in a comment): withheld_grudge 150, spoils_large_share 475,
// spoils_large_part_permille 401, soured_renew_permille 250, join_weary_permille 750, join_busy_permille 150,
// join_fear_floor 600, join_warmth_floor 700, refuse_grudge 80, turn_cooldown_years 800, temper_allied_permille
// 300/300/300/3000/150.
// The data root is copied to a scratch directory under the build tree and one file is edited per case.
//
// Not here (named for the integration agent): the two fixture roots of 14.1 and 14.2 (alliances-small, relations-
// small and zero are in-memory edits in these tests and the reference, p3_data_reference.ALLIANCES_SMALL_028,
// RELATIONS_SMALL_028); "the code defaults reproduce 0.26.0 on the fixtures' alliance histories" needs a tuning
// built in code, which is the engineer's (the reference's defaults are p3_data_reference.ALLIANCES_DEFAULTS_028).
// Every case starts with P504A_REQUIRE_INTEGRATED(): the 0.27.0 data have none of these fields.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../db_test_support.hpp"
#include "../p3_01_history_support.hpp"
#include "p5_03_relations_checks.hpp"
#include "p5_04a_api.hpp"

namespace {

namespace fs = std::filesystem;
namespace wd = worldgen::data;
using history_test::Load;

struct Field {
    const char* name;
    std::int64_t lo, hi, shipped;
};

// 14.1: appended after mine_forms, in this order.
const std::vector<Field>& alliance_fields() {
    static const std::vector<Field> v = {
        {"spoils_large_share", 1, 1000000, 475}, {"spoils_large_part_permille", 0, 1001, 401},   // start 600, 300
        {"soured_renew_permille", 0, 1000, 250}, {"soured_join_permille", 0, 1000, 500},   // start 500
        {"weary_years", 0, 100000, 50},          {"join_weary_permille", 0, 1000, 750},   // start 600
        {"join_busy_permille", 0, 1000, 150},    {"join_fear_floor", 0, 1000, 600},   // start 250, 300
        {"join_warmth_floor", 0, 1000, 700},     {"grasping_join_permille", 0, 1000, 600},   // start 400
        {"join_claim_permille", 0, 1000, 500},   {"excused_memory", 0, 1000, 15},
        {"refuse_grudge", 0, 2000, 80},          {"betray_cooled_disposition", -2000, 2000, 0},   // start 150
        {"betray_weak_permille", 0, 1000, 500}};
    return v;
}
// 14.2: appended after expel_gap_years.
const std::vector<Field>& relation_fields() {
    static const std::vector<Field> v = {{"turn_cooldown_years", 0, 100000, 800},   // start 300
                                         {"expel_settled_years", 0, 100000, 800},
                                         {"expel_fresh_years", 0, 100000, 150}};
    return v;
}
// 14.2a: appended after epithet_repeat_max.
const std::vector<Field>& figure_fields() {
    static const std::vector<Field> v = {{"tower_spacing_permille", 0, 1000, 30}, {"hero_return_years", 1, 100000, 100}};
    return v;
}

fs::path data_root() { return history_test::source_root() / "game" / "data"; }

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// A copy of game/data with `file` (relative) replaced by `text`; loaded.
Load load_edited(const std::string& file, const std::string& text) {
    static int n = 0;
    static dbtest::TempDir dir("p504a-data");
    const fs::path root = dir.path() / ("root-" + std::to_string(++n));
    fs::copy(data_root(), root, fs::copy_options::recursive);
    {
        std::ofstream out(root / file, std::ios::binary | std::ios::trunc);
        out << text;
    }
    Load l = history_test::load_disk(root);
    fs::remove_all(root);
    return l;
}

std::string with_value(std::string text, const std::string& name, std::int64_t v) {
    const std::size_t a = text.find("\n" + name + " = ");
    REQUIRE_MESSAGE(a != std::string::npos, name << " not in the file");
    const std::size_t e = text.find('\n', a + 1);
    text.replace(a, e - a, "\n" + name + " = " + std::to_string(v));
    return text;
}
std::string without(std::string text, const std::string& name) {
    const std::size_t a = text.find("\n" + name + " = ");
    REQUIRE_MESSAGE(a != std::string::npos, name << " not in the file");
    text.erase(a, text.find('\n', a + 1) - a);
    return text;
}

const wd::Record& record(const wd::ContentSet& c, const char* id) {
    const wd::Definition* d = p503check::definition(c, "history", id);
    REQUIRE(d != nullptr);
    return d->record;
}

const wd::ContentSet& shipped() {
    static const wd::ContentSet c = [] {
        Load l = history_test::load_disk(data_root());
        REQUIRE_MESSAGE(l.ok(), history_test::dump(l));
        return std::move(*l.content);
    }();
    return c;
}

// The fields follow `after` in order, at their shipped values.
void check_order(const char* id, const char* after, const std::vector<Field>& fs_) {
    const wd::Record& r = record(shipped(), id);
    std::size_t at = r.size();
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r.name_at(i) == after) at = i;
    }
    REQUIRE_MESSAGE(at < r.size(), after << " not in history." << id);
    for (std::size_t i = 0; i < fs_.size(); ++i) {
        CAPTURE(fs_[i].name);
        REQUIRE(at + 1 + i < r.size());
        CHECK(r.name_at(at + 1 + i) == fs_[i].name);
        CHECK(r.get_int(fs_[i].name) == fs_[i].shipped);
    }
}

void check_bounds(const std::string& file, const std::vector<Field>& fs_) {
    const std::string text = read(data_root() / file);
    for (const Field& f : fs_) {
        CAPTURE(f.name);
        const std::string expected = "expected an integer from " + std::to_string(f.lo) + " to " + std::to_string(f.hi);
        for (const std::int64_t v : {f.lo - 1, f.hi + 1}) {
            CAPTURE(v);
            const Load l = load_edited(file, with_value(text, f.name, v));
            REQUIRE_FALSE(l.ok());
            REQUIRE_FALSE(l.errors.empty());
            CHECK(history_test::contains(l.errors[0].message, f.name));
            CHECK(history_test::contains(l.errors[0].message, " is " + std::to_string(v) + "; " + expected));
        }
        for (const std::int64_t v : {f.lo, f.hi}) {
            CAPTURE(v);
            const Load l = load_edited(file, with_value(text, f.name, v));
            // a bound may fail a named check of the record (as P5-04's); the field's own range accepts it
            for (const wd::LoadError& e : l.errors) CHECK_FALSE(history_test::contains(e.message, "expected an integer"));
        }
        const Load l = load_edited(file, without(text, f.name));
        REQUIRE_FALSE(l.ok());
        CHECK(history_test::contains(l.errors[0].message, "missing required field"));
        CHECK(history_test::contains(l.errors[0].message, f.name));
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04a 14.1: history.alliances: the fifteen fields after mine_forms; withheld_to_break 2, withheld_grudge 150") {
    P504A_REQUIRE_INTEGRATED();
    check_order("alliances", "mine_forms", alliance_fields());
    const wd::Record& r = record(shipped(), "alliances");
    CHECK(r.get_int("withheld_to_break") == 2);
    CHECK(r.get_int("withheld_grudge") == 150);   // 14.1: 250, calibrated to 150
    CHECK(r.get_int("refuse_memory") == 40);       // 4.4: kept
    CHECK(r.get_int("refusals_to_break") == 2);    // 4.4: kept (now of counted refusals)
}

TEST_CASE("P5-04a 14.2, 14.2a: history.relations and history.figures: the appended fields; the changed figures values") {
    P504A_REQUIRE_INTEGRATED();
    check_order("relations", "expel_gap_years", relation_fields());
    check_order("figures", "epithet_repeat_max", figure_fields());
    const wd::Record& f = record(shipped(), "figures");
    CHECK(f.get_int("skill_min") == 600);   // 21.3, L-7
    CHECK(f.get_int("skill_max") == 900);
    const wd::Value* allied = f.find("temper_allied_permille");
    REQUIRE(allied != nullptr);
    CHECK(allied->as_record().get_int("grasping") == 3000);   // 21.3, L-8
    CHECK(allied->as_record().get_int("steady") == 300);      // calibration (from 1000)
    CHECK(allied->as_record().get_int("warlike") == 300);
    CHECK(allied->as_record().get_int("peaceable") == 300);
    CHECK(allied->as_record().get_int("just") == 150);        // calibration (from 500)
    const wd::Value* ep = f.find("epithets");
    REQUIRE(ep != nullptr);
    bool old = false;
    for (const wd::Value& e : ep->as_array()) {
        const wd::Record& x = e.as_record();
        if (x.find("deed") != nullptr && x.find("deed")->as_string() == "reign_permille") {
            old = true;
            CHECK(x.get_int("min") == 1400);           // 21.1
            CHECK(x.get_int("substantial") == 1480);
            CHECK(x.find("text")->as_string() == "the Old");
        }
    }
    CHECK(old);
}

TEST_CASE("P5-04a 3.7, 4.4, 5: each history.alliances field refused below and above its range and when absent") {
    P504A_REQUIRE_INTEGRATED();
    check_bounds("history/alliances.toml", alliance_fields());
}

TEST_CASE("P5-04a 7.2, 8, 14.2a: each history.relations and history.figures field refused out of range and absent") {
    P504A_REQUIRE_INTEGRATED();
    check_bounds("history/relations.toml", relation_fields());
    check_bounds("history/figures.toml", figure_fields());
}

}  // TEST_SUITE
