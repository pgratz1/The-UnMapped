// Engineer-side checks of the P3-08 change (generator 0.17.0), written by the
// generator engineer while implementing; the change's tests proper are the
// test writer's (docs/specs/p3-08-followups.md section 7).

#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/history/peoples.hpp"
#include "worldgen/stages/regions.hpp"

namespace {

namespace wh = worldgen::history;
namespace ws = worldgen::stages;

// An environment variable, portably (MSVC deprecates std::getenv, C4996).
std::optional<std::string> engineer_env(const char* name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

}  // namespace

TEST_CASE("P3-08 engineer: root_word and name_filtered (P3-02 11.3)") {
    CHECK(wh::root_word("The Hold of {root}", "durin") == "durin");
    CHECK(wh::root_word("{root}ford", "cock") == "cockford");
    CHECK(wh::root_word("Ny{root} Pit", "gat") == "nygat");
    wh::NameFilter f;
    f.anywhere = {"ab"};
    f.at_start = {"cd"};
    f.whole = {"ef"};
    CHECK(wh::name_filtered(f, "xaby"));
    CHECK(wh::name_filtered(f, "a-by"));
    CHECK_FALSE(wh::name_filtered(f, "xcdy"));
    CHECK(wh::name_filtered(f, "xx-cdy"));
    CHECK(wh::name_filtered(f, "cd"));
    CHECK(wh::name_filtered(f, "ef"));
    CHECK_FALSE(wh::name_filtered(f, "efx"));
    CHECK(wh::name_filtered(f, "x'ef"));
    CHECK(wh::name_filtered(f, wh::root_word("{root}", "") + "ef"));
    CHECK_FALSE(wh::name_filtered(wh::NameFilter{}, "anything"));
}

TEST_CASE("P3-08 engineer: the growth rule drains above the kind cap (P3-05 r6 9.1)") {
    // A faerie village of 2,277 with share 3,000 and cap 1,499 loses 77.
    CHECK(wh::grow_population(2277, 3000, 4, 1499, 100) == 2277 - 77);
    CHECK(wh::grow_population(1499, 3000, 4, 1499, 100) == 1499);
    // Below the cap the r5 values: growth toward the share, decline above it.
    CHECK(wh::grow_population(1000, 900, 4, 1499, 100) == 990);
    CHECK(wh::grow_population(900, 900, 4, 1499, 100) == 900);
}

TEST_CASE("P3-08 engineer: region name validity V1 to V6 (P1-06 RG11 7)") {
    ws::PlacenameDef common;
    common.role = ws::PlaceRole::common;
    common.forbidden = {"wank"};
    common.excluded = {"Blackrock", "The Haunted Forest"};
    const auto compound = [&](const std::string& a, const std::string& b) {
        return ws::region_name_valid("{prefix}{suffix}", {{2, a}, {1, b}}, common);
    };
    CHECK_FALSE(compound("Birch", "hurst"));       // V1 same letter
    CHECK_FALSE(compound("Bramble", "alder"));     // V1 two vowels
    CHECK_FALSE(compound("Hemlock", "hollow"));    // V1 13 letters
    CHECK(compound("Adder", "bog"));
    CHECK(compound("Thistle", "horn"));
    CHECK(compound("Bracken", "stone"));
    CHECK_FALSE(compound("Brrr", "holt"));         // V2
    CHECK_FALSE(compound("Wank", "holt"));         // V4
    CHECK_FALSE(compound("Black", "rock"));        // V5 whole
    CHECK_FALSE(ws::region_name_valid("{prefix}{suffix} {noun}", {{2, "Black"}, {1, "rock"}, {0, "Downs"}}, common));
    CHECK_FALSE(ws::region_name_valid("The {noun} of {object}", {{0, "Wood"}, {7, "Woods"}}, common));  // V3
    CHECK_FALSE(ws::region_name_valid("The {adjective} {noun}", {{3, "Haunted"}, {0, "Forest"}}, common));
}

// Skipped by default: prints every rendered sentence of a world.
//   UNMAPPED_P308_DUMP=<world.db> UNMAPPED_P308_OUT=<file> worldgen-tests -ts=p3-08-dump --no-skip
TEST_SUITE("p3-08-dump" * doctest::skip()) {
TEST_CASE("P3-08 engineer: dump the sentences of a world") {
    const std::optional<std::string> path = engineer_env("UNMAPPED_P308_DUMP");
    const std::optional<std::string> out_path = engineer_env("UNMAPPED_P308_OUT");
    REQUIRE(path.has_value());
    REQUIRE(out_path.has_value());
    auto world = worldgen::db::WorldReader::open(*path);
    REQUIRE(world.ok());
    auto view = wh::HistoryView::load(world.value());
    REQUIRE(view.ok());
    std::ofstream out(*out_path, std::ios::binary);
    for (const wh::HistoryView::Event& e : view.value().events()) {
        out << "e" << e.row.id << " " << e.row.year << " " << wh::render_event(view.value(), e.row.id) << "\n";
    }
}
}
