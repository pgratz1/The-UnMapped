// Spec P4-02 section 9 test 9 (invariants 2, 6 and 7 on generated worlds):
// the shipped 512 world of seed 42 (every stage), a --until hydrology world
// of the same seed, and the test-pattern world. Each is generated once per
// process with the checkout's game/data into a scratch directory under the
// build tree.
//
// Checked: every layer has a catalogue entry of the stored element type and
// every cell lies in the entry's range (at most width x height for
// flow_accum); every entry is present in the full world, at layer id
// index + 1; the region names of table `region` through RegionRecord and
// region_name; the available composites; and the test-pattern layers, which
// the catalogue does not know (group Other, plain decimals, gray legend).
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "db_test_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"
#include "worldgen/inspect/render.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

namespace wd = worldgen::data;
namespace wdb = worldgen::db;
namespace wi = worldgen::inspect;

const wd::ContentSet& shipped() {
    static const wd::ContentSet c = [] {
        auto r = wd::load_content(wd::builtin_kinds(),
                                  std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / "game" / "data");
        REQUIRE_MESSAGE(r.ok(), "game/data does not load");
        return std::move(*r.value);
    }();
    return c;
}

const dbtest::TempDir& world_dir() {
    static const dbtest::TempDir d("p4-02-layer-catalogue");
    return d;
}

// generate --seed 42 --map-size 512 [--until <until>] with the shipped data.
const std::string& shipped_world(const std::optional<std::string>& until) {
    static std::string full_path;
    static std::string hydrology_path;
    std::string& path = until.has_value() ? hydrology_path : full_path;
    if (!path.empty()) return path;
    REQUIRE((!until.has_value() || *until == "hydrology"));
    worldgen::WorldParams p;
    p.seed = 42;
    p.map_size = 512;
    p.content_hash = wd::content_hash(shipped());
    p.until = until;
    const std::string out = world_dir().file(until.has_value() ? "seed42-512-hydrology.db" : "seed42-512.db");
    const wdb::Status s = worldgen::generate_world(p, shipped(), out, wdb::Overwrite::replace);
    REQUIRE_MESSAGE(s.ok(), "generate_world: " << (s.ok() ? std::string() : s.error().message));
    path = out;
    return path;
}

const std::string& test_pattern_world() {
    static std::string path;
    if (!path.empty()) return path;
    worldgen::WorldParams p;
    p.seed = 42;
    p.content_hash = worldgen::sha256(std::string_view{});
    p.test_pattern = true;
    const std::string out = world_dir().file("seed42-pattern.db");
    const wdb::Status s = worldgen::generate_world(p, out, wdb::Overwrite::replace);
    REQUIRE_MESSAGE(s.ok(), "generate_world: " << (s.ok() ? std::string() : s.error().message));
    path = out;
    return path;
}

wdb::WorldReader open_world(const std::string& path) {
    auto r = wdb::WorldReader::open(path);
    REQUIRE_MESSAGE(r.ok(), "open " << path);
    return std::move(r.value());
}

std::vector<wdb::LayerInfo> layers_of(const wdb::WorldReader& w) {
    auto r = w.layers();
    REQUIRE(r.ok());
    return std::move(r.value());
}

wdb::LayerGrid grid_of(const wdb::WorldReader& w, std::string_view name) {
    auto r = w.read_layer(name);
    REQUIRE_MESSAGE(r.ok(), "read_layer " << name);
    return std::move(r.value().grid);
}

struct Extremes {
    bool any = false;
    std::int64_t lo = 0;  // smallest cell as int64 (every catalogue type fits)
    std::int64_t hi = 0;
    bool fits = true;     // false when a u64 cell is above INT64_MAX
};

Extremes extremes(const wdb::LayerGrid& g) {
    Extremes e;
    std::visit(
        [&e](const auto& cells) {
            using T = typename std::decay_t<decltype(cells)>::value_type;
            for (const T c : cells) {
                std::int64_t v = 0;
                if constexpr (std::is_same_v<T, std::uint64_t>) {
                    if (c > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                        e.fits = false;
                        continue;
                    }
                    v = static_cast<std::int64_t>(c);
                } else {
                    v = static_cast<std::int64_t>(c);
                }
                if (!e.any || v < e.lo) e.lo = v;
                if (!e.any || v > e.hi) e.hi = v;
                e.any = true;
            }
        },
        g.values);
    return e;
}

// Invariant 2 for one world: every layer known, of the catalogue type, in range.
void check_layers_known(const wdb::WorldReader& w) {
    for (const wdb::LayerInfo& info : layers_of(w)) {
        CAPTURE(info.name);
        const wi::LayerSpec* s = wi::find_layer_spec(info.name);
        REQUIRE(s != nullptr);
        CHECK(s->type == info.type);
        CHECK(wi::layer_group(info.name) == s->group);
        CHECK(wi::layer_group(info.name) != wi::LayerGroup::other);
        CHECK_FALSE(wi::layer_unit(info.name).empty());
        const wdb::LayerGrid g = grid_of(w, info.name);
        CHECK(g.type() == s->type);
        const Extremes e = extremes(g);
        CHECK(e.fits);
        REQUIRE(e.any);
        CHECK(e.lo >= s->min);
        CHECK(e.hi <= s->max);
        if (s->max_is_cell_count) {
            CHECK(static_cast<std::uint64_t>(e.hi) <= g.cell_count());
        }
        // cell_value agrees with the stored cells at the corners and the centre
        const std::uint32_t xs[] = {0, g.width / 2, g.width - 1};
        const std::uint32_t ys[] = {0, g.height / 2, g.height - 1};
        for (const std::uint32_t x : xs) {
            for (const std::uint32_t y : ys) {
                std::int64_t v = 0;
                REQUIRE(wi::cell_value(g, x, y, v));
                CHECK(v >= s->min);
                CHECK(v <= s->max);
                CHECK_NOTHROW((void)wi::format_layer_value(info.name, v));
            }
        }
        std::int64_t v = 0;
        CHECK_FALSE(wi::cell_value(g, g.width, 0, v));
        CHECK_FALSE(wi::cell_value(g, 0, g.height, v));
        // the auto palette's legend never throws and is not empty here
        const wi::Palette p = wi::auto_palette(info.name, g);
        CHECK(p == s->palette);
        CHECK_NOTHROW((void)wi::layer_legend(info.name, p, g, {}));
    }
}

std::vector<wi::BiomeRecord> biome_records(const wdb::WorldReader& w) {
    auto rows = w.table_rows(wdb::BIOME_TABLE);
    REQUIRE(rows.ok());
    const wi::BiomeTableResult b = wi::read_biome_rows(rows.value());
    REQUIRE_FALSE(b.problem.has_value());
    return b.records;
}

}  // namespace

TEST_SUITE("layer-catalogue-worlds") {

TEST_CASE("P4-02 invariant 2: every layer of the shipped seed-42 512 world is in the catalogue, every entry in the world") {
    const wdb::WorldReader w = open_world(shipped_world(std::nullopt));
    check_layers_known(w);
    const auto layers = layers_of(w);
    const auto cat = wi::layer_catalogue();
    REQUIRE(layers.size() == cat.size());  // no stale entry, no unknown layer
    for (std::size_t i = 0; i < cat.size(); ++i) {
        CAPTURE(i);
        CHECK(layers[i].name == cat[i].name);
        CHECK(layers[i].id == static_cast<std::int64_t>(i) + 1);  // P1-00 3 layer ids 1 to 19
    }
}

TEST_CASE("P4-02 invariant 2: the --until hydrology world's layers are known, a prefix of the catalogue") {
    const wdb::WorldReader w = open_world(shipped_world(std::string("hydrology")));
    check_layers_known(w);
    const auto layers = layers_of(w);
    const auto cat = wi::layer_catalogue();
    REQUIRE_FALSE(layers.empty());
    REQUIRE(layers.size() < cat.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        CAPTURE(i);
        CHECK(layers[i].name == cat[i].name);
    }
    // the hydrology layers of P1-00 4 are all there
    for (const std::string_view n : {"elevation", "water", "water_depth", "flow_dir", "flow_accum", "river", "lake"}) {
        bool found = false;
        for (const wdb::LayerInfo& l : layers) found = found || l.name == n;
        CAPTURE(n);
        CHECK(found);
    }
}

TEST_CASE("P4-02 4.5, invariant 7: available composites of the generated worlds") {
    using wi::Composite;
    const wdb::WorldReader full = open_world(shipped_world(std::nullopt));
    CHECK(wi::available_composites(layers_of(full), true) ==
          std::vector<Composite>{Composite::map, Composite::biomes, Composite::regions});
    CHECK(wi::available_composites(layers_of(full), false) == std::vector<Composite>{Composite::map});
    const wdb::WorldReader hyd = open_world(shipped_world(std::string("hydrology")));
    CHECK(wi::available_composites(layers_of(hyd), true) == std::vector<Composite>{Composite::map});
    const wdb::WorldReader pat = open_world(test_pattern_world());
    CHECK(wi::available_composites(layers_of(pat), true).empty());
}

TEST_CASE("P4-02 4.4: the biome legend and composite legends of the shipped world follow table biome") {
    const wdb::WorldReader w = open_world(shipped_world(std::nullopt));
    const auto recs = biome_records(w);
    REQUIRE_FALSE(recs.empty());
    const wdb::LayerGrid g = grid_of(w, "biome");
    const auto legend = wi::layer_legend("biome", wi::Palette::biome, g, recs);
    REQUIRE(legend.size() == recs.size());
    std::size_t land = 0;
    for (std::size_t i = 0; i < recs.size(); ++i) {
        CAPTURE(i);
        CHECK(legend[i].label == (recs[i].name.empty() ? recs[i].key : recs[i].name));
        REQUIRE(legend[i].colours.size() == 1U);
        CHECK(legend[i].colours[0] == static_cast<std::uint32_t>(recs[i].colour & 0xFFFFFF));
        if (recs[i].water == 0) ++land;
    }
    // biomes: land biome rows, then the three water rows; regions: one more row first
    CHECK(wi::composite_legend(wi::Composite::biomes, recs).size() == land + 3U);
    const auto regions = wi::composite_legend(wi::Composite::regions, recs);
    REQUIRE(regions.size() == land + 4U);
    CHECK(regions[0].label == "region border");
    CHECK(wi::composite_legend(wi::Composite::map, recs).size() == wi::HYPSOMETRIC_LAND_STOPS.size() + 3U);
}

TEST_CASE("P4-02 4.6, invariant 6: region names of the shipped world through RegionRecord and region_name") {
    const wdb::WorldReader w = open_world(shipped_world(std::nullopt));
    auto rows = w.table_rows(wdb::REGION_TABLE);
    REQUIRE(rows.ok());
    const wi::RegionTableResult r = wi::read_region_rows(rows.value());
    REQUIRE_FALSE(r.problem.has_value());
    REQUIRE_FALSE(r.records.empty());
    // the same names and biomes as the reader's typed rows (an independent read)
    auto typed = wdb::read_region_rows(w);
    REQUIRE(typed.ok());
    REQUIRE(typed.value().size() == r.records.size());
    std::int64_t largest = 0;
    for (std::size_t i = 0; i < r.records.size(); ++i) {
        const wi::RegionRecord& rec = r.records[i];
        CAPTURE(rec.id);
        CHECK(rec.id == typed.value()[i].id);
        CHECK(rec.name == typed.value()[i].name);
        CHECK(rec.biome == typed.value()[i].biome);
        CHECK_FALSE(rec.name.empty());
        CHECK(wi::region_name(r.records, rec.id) == rec.name);
        if (i > 0) CHECK(r.records[i - 1].id < rec.id);
        largest = rec.id > largest ? rec.id : largest;
    }
    CHECK(wi::region_name(r.records, 0).empty());
    CHECK(wi::region_name(r.records, largest + 1).empty());
    CHECK(wi::region_name(r.records, -1).empty());
    // every region id of the layer has a name; 0 is the ocean
    const wdb::LayerGrid g = grid_of(w, "region");
    const auto* cells = g.get_if<std::uint16_t>();
    REQUIRE(cells != nullptr);
    std::vector<bool> seen(65536, false);
    for (const std::uint16_t c : *cells) seen[c] = true;
    CHECK(seen[0]);
    for (std::size_t id = 1; id < seen.size(); ++id) {
        if (!seen[id]) continue;
        CAPTURE(id);
        CHECK_FALSE(wi::region_name(r.records, static_cast<std::int64_t>(id)).empty());
    }
}

TEST_CASE("P4-02 5, 9 test 9: the test-pattern world's layers are unknown: Other, plain decimals, gray") {
    const wdb::WorldReader w = open_world(test_pattern_world());
    const auto layers = layers_of(w);
    REQUIRE(layers.size() == 3U);
    for (const wdb::LayerInfo& info : layers) {
        CAPTURE(info.name);
        CHECK(wi::find_layer_spec(info.name) == nullptr);
        CHECK(wi::layer_group(info.name) == wi::LayerGroup::other);
        CHECK(wi::layer_unit(info.name).empty());
        CHECK(wi::registered_palette(info.name) == wi::Palette::gray);
        const wdb::LayerGrid g = grid_of(w, info.name);
        CHECK(wi::auto_palette(info.name, g) == wi::Palette::gray);
        std::int64_t v = 0;
        REQUIRE(wi::cell_value(g, 3, 5, v));
        CHECK(wi::format_layer_value(info.name, v) == wdb::format_i64(v));
        const Extremes e = extremes(g);
        REQUIRE(e.any);
        const auto legend = wi::layer_legend(info.name, wi::Palette::gray, g, {});
        REQUIRE(legend.size() == 3U);
        CHECK(legend[0].label == wdb::format_i64(e.lo) + " (min)");
        CHECK(legend[1].label.empty());
        CHECK(legend[2].label == wdb::format_i64(e.hi) + " (max)");
    }
}

}  // TEST_SUITE
