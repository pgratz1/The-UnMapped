#pragma once

// Fixture T2 of spec P4-04 10.2 item 3 (worldgen/tests/data/markers/t2-markers.toml: the P3-07 fixture format
// with a [markers] table of region seed cells and pass rows), written through the writer API: the stages
// biomes (biome rows, then the pass rows), regions, chronology and history. Needs nothing of P4-04's API, so
// markers_fixture_test.cpp can compare its table hashes with the reference's SQL copy on its own.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <toml.hpp>

#include "history_report_fixture.hpp"
#include "timeline_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/writer.hpp"

#ifndef UNMAPPED_P4_04_FIXTURE_DIR
#error "UNMAPPED_P4_04_FIXTURE_DIR must name worldgen/tests/data/markers"
#endif

namespace mktest {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace wd = worldgen::data;

inline const std::string T2 = "t2-markers";

// --- Fixture T2 -------------------------------------------------------------------------------------

inline fs::path t2_toml() { return fs::path(UNMAPPED_P4_04_FIXTURE_DIR) / "t2-markers.toml"; }

namespace detail {

inline std::int64_t i64(const toml::node& n) {
    const auto v = n.value<std::int64_t>();
    REQUIRE_MESSAGE(v.has_value(), "T2: expected an integer");
    return *v;
}

inline const std::vector<db::PassRow>*& t2_pass_rows() {
    static const std::vector<db::PassRow>* p = nullptr;
    return p;
}

// The stage "biomes" of T2: the biome rows, then the pass rows.
inline void stage_biomes_and_passes(worldgen::stages::StageContext& ctx) {
    hrtest::detail::stage_biomes(ctx);
    for (const db::PassRow& p : *t2_pass_rows()) ctx.write_pass(p);
}

inline const dbtest::TempDir& scratch() {
    static const dbtest::TempDir d("markers");
    return d;
}

inline toml::table t2_doc() {
    const fs::path path = t2_toml();
    return toml::parse(hrtest::detail::text_of_file(path), path.string());
}

}  // namespace detail

// T2's pass rows ([markers].pass: [id, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm, [cells]]).
inline const std::vector<db::PassRow>& t2_passes() {
    static const std::vector<db::PassRow> v = [] {
        const toml::table doc = detail::t2_doc();
        std::vector<db::PassRow> out;
        const toml::array* rows = doc["markers"]["pass"].as_array();
        REQUIRE(rows != nullptr);
        for (const toml::node& n : *rows) {
            const toml::array& r = *n.as_array();
            db::PassRow p;
            p.id = detail::i64(*r.get(0));
            p.range_id = detail::i64(*r.get(1));
            p.kind = detail::i64(*r.get(2));
            p.capped = detail::i64(*r.get(3));
            p.col_x = detail::i64(*r.get(4));
            p.col_y = detail::i64(*r.get(5));
            p.saddle_dm = detail::i64(*r.get(6));
            p.col_dm = detail::i64(*r.get(7));
            for (const toml::node& c : *r.get(8)->as_array()) {
                const auto cell = static_cast<std::uint32_t>(detail::i64(c));
                for (int k = 0; k < 4; ++k) p.path.push_back(static_cast<std::uint8_t>((cell >> (8 * k)) & 0xFFU));
            }
            out.push_back(std::move(p));
        }
        return out;
    }();
    return v;
}

// The rows of T2: hrtest's expansion of the P3-07 format with the region seed cells of [markers].
inline const hrtest::Fixture& t2_fixture() {
    static const hrtest::Fixture fx = [] {
        hrtest::Fixture f = hrtest::load_fixture(db::path_to_utf8(t2_toml().replace_extension()));
        const toml::table doc = detail::t2_doc();
        const toml::array* seeds = doc["markers"]["region_seed"].as_array();
        REQUIRE(seeds != nullptr);
        for (const toml::node& n : *seeds) {
            const toml::array& r = *n.as_array();
            const std::int64_t id = detail::i64(*r.get(0));
            bool found = false;
            for (db::RegionRow& row : f.regions) {
                if (row.id == id) {
                    row.seed_x = detail::i64(*r.get(1));
                    row.seed_y = detail::i64(*r.get(2));
                    found = true;
                }
            }
            REQUIRE_MESSAGE(found, "T2: region_seed of no region " << id);
        }
        return f;
    }();
    return fx;
}

inline void write_t2_world(const std::string& path) {
    static const wd::ContentSet empty;
    const hrtest::Fixture& fx = t2_fixture();
    worldgen::WorldParams p;
    p.seed = fx.seed;
    p.map_size = fx.map_size;
    p.content_hash = wd::content_hash(empty);
    p.history.span = fx.span;
    p.history.races = fx.races;
    p.threads = 1;
    const worldgen::stages::StageInfo stages[] = {{"biomes", &detail::stage_biomes_and_passes},
                                                  {"regions", &hrtest::detail::stage_regions},
                                                  {"chronology", &hrtest::detail::stage_chronology},
                                                  {"history", &hrtest::detail::stage_history}};
    hrtest::detail::current() = &fx;
    detail::t2_pass_rows() = &t2_passes();
    const db::Status s =
        worldgen::stages::testing::generate_world_with_stages(p, empty, stages, path, db::Overwrite::replace);
    hrtest::detail::current() = nullptr;
    detail::t2_pass_rows() = nullptr;
    REQUIRE_MESSAGE(s.ok(), "T2: " << (s.ok() ? "" : s.error().message));
}

// The T2 world, written once per process.
inline const std::string& t2_world() {
    static const std::string path = [] {
        const std::string p = detail::scratch().file("t2-markers.db");
        write_t2_world(p);
        return p;
    }();
    return path;
}

// A fresh copy of a world file at a new path (to corrupt it).
inline std::string copy_of(const std::string& src, const std::string& label) {
    static int counter = 0;
    const std::string path = detail::scratch().file(label + "-" + std::to_string(++counter) + ".db");
    std::error_code ec;
    fs::copy_file(db::path_from_utf8(src), db::path_from_utf8(path), fs::copy_options::overwrite_existing, ec);
    REQUIRE_MESSAGE(!ec, "cannot copy " << src);
    return path;
}

}  // namespace mktest
