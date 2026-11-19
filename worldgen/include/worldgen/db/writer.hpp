#pragma once

// World database writer (spec P0-02 sections 4.2, 5, 6).
//
// Typical use (P0-04 section 5.1):
//
//     worldgen::db::WorldWriter writer;              // steps 1-2
//     writer.set_seed(seed);                         // or set_seed_phrase(scrubbed)
//     writer.set_content_hash(content_hash);
//     writer.set_param_integer("map_size", 1024);
//     {
//         auto stage = writer.begin_stage("test_pattern");   // step 3: BEGIN
//         stage.write_layer(1, "test.ramp", ramp);
//         stage.write_layer(2, "test.hash", hash);
//         stage.commit();                                     // COMMIT
//     }
//     worldgen::db::Status status =
//         writer.finalize(out_utf8, worldgen::db::Overwrite::refuse);  // steps 4-9
//
// Contract violations (an invalid layer, a layer id out of order, a missing
// required value, SQLite rejecting a statement the writer built, an
// integrity_check or header verification failure) raise FatalError
// (P0-01 4.1). Problems with the output location, which are the user's and
// not a bug, are returned by finalize() as error values. Either way, nothing
// but the finished output file is left behind (P0-02 section 3).

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/sha256.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::db {

namespace testing {
struct WriterAccess;  // test hooks, worldgen/db/testing.hpp
}

namespace detail {
struct WriterState;  // the working database and pending meta values
}

// Whether finalize() may replace an existing output file (P0-02 6 step 9,
// P0-04 4.4 --force).
enum class Overwrite : std::uint8_t {
    refuse,   // an existing <out> is an output_exists error
    replace,  // an existing <out> is replaced by the final rename
};

// The suffix of the temporary output file, <out>.partial (P0-02 6).
inline constexpr std::string_view PARTIAL_SUFFIX = ".partial";

// One row of table `pass` (P1-01 9.1): a natural pass site. path is the
// crossing polyline as u32 little-endian cell indices, 4 bytes per cell.
struct PassRow {
    std::int64_t id = 0;
    std::int64_t range_id = 0;
    std::int64_t kind = 0;    // 1 lowered pass, 0 natural gap
    std::int64_t capped = 0;  // 0 or 1
    std::int64_t col_x = 0;
    std::int64_t col_y = 0;
    std::int64_t saddle_dm = 0;
    std::int64_t col_dm = 0;
    std::vector<std::uint8_t> path;
};

// One row of table `biome` (P1-04 3, B6; schema 3): a loaded biome
// definition. id is the biome code, key the definition id, colour 0xRRGGBB,
// water 0 or 1.
struct BiomeRow {
    std::int64_t id = 0;
    std::string key;
    std::string name;
    std::int64_t colour = 0;
    std::int64_t water = 0;
};

// The one row of table `climate` (P1-03 3, C7; schema 4): id 1, the D8
// code (0 E to 7 NE) the prevailing wind blows toward and the world's
// temperature offset in tenths of a degree (C2b, -50..50).
struct ClimateRow {
    std::int64_t id = 1;
    std::int64_t wind_toward = 0;
    std::int64_t temp_offset_dc = 0;
};

// One row of table `region` (P1-06 3.2; schema 5), columns in declared
// order.
struct RegionRow {
    std::int64_t id = 0;
    std::string name;
    std::int64_t biome = 0;
    std::int64_t x_min = 0;
    std::int64_t y_min = 0;
    std::int64_t x_max = 0;
    std::int64_t y_max = 0;
    std::int64_t seed_x = 0;
    std::int64_t seed_y = 0;
    std::int64_t landmass = 0;
    std::int64_t cells = 0;
    std::int64_t lake_cells = 0;
    std::int64_t river_cells = 0;
    std::int64_t river_class_max = 0;
    std::int64_t coast_cells = 0;
    std::int64_t walk_cells = 0;
    std::int64_t walk_area = 0;
    std::int64_t elev_mean_dm = 0;
    std::int64_t elev_max_dm = 0;
    std::int64_t fertility_mean = 0;
    std::int64_t forage_mean = 0;
    std::int64_t timber_mean = 0;
    std::int64_t ore_mean = 0;
    std::int64_t fresh_water_mean = 0;
    std::int64_t habitability_mean = 0;
    std::int64_t habitability_p90 = 0;
    std::int64_t ore_rich_cells = 0;
    std::int64_t ore_reach_cells = 0;

    friend bool operator==(const RegionRow&, const RegionRow&) = default;
};

// One row of table `region_border` (P1-06 3.3; schema 5): a pair of
// bordering regions a < b.
struct RegionBorderRow {
    std::int64_t a = 0;
    std::int64_t b = 0;
    std::int64_t length_m = 0;
    std::int64_t low_dm = 0;
    std::int64_t low_x = 0;
    std::int64_t low_y = 0;
    std::int64_t walk_pairs = 0;
    std::int64_t river_pairs = 0;
    std::int64_t river_class_max = 0;
    std::int64_t pass_id = 0;
    std::int64_t passes = 0;

    friend bool operator==(const RegionBorderRow&, const RegionBorderRow&) = default;
};

// One row of table `region_biome` (P1-06 3.3; schema 5): the cells of one
// biome code in one region.
struct RegionBiomeRow {
    std::int64_t region = 0;
    std::int64_t biome = 0;
    std::int64_t cells = 0;

    friend bool operator==(const RegionBiomeRow&, const RegionBiomeRow&) = default;
};

// One stage's transaction on the working database (P0-02 6 step 3).
// Obtained from WorldWriter::begin_stage; at most one is open at a time.
// Destroying it without commit() rolls the transaction back and leaves no
// trace (P0-02 14.6): its rows are gone (their layer names can be used
// again), the stage is not recorded in meta.stages (its name can be used
// again), and next_layer_id() returns to its value at begin_stage. Movable, not copyable; it must not
// outlive its WorldWriter.
class StageWriter {
public:
    StageWriter(StageWriter&& other) noexcept;
    StageWriter& operator=(StageWriter&&) = delete;
    StageWriter(const StageWriter&) = delete;
    StageWriter& operator=(const StageWriter&) = delete;
    ~StageWriter();

    // Encodes grid with codec (P0-02 7) and inserts it as layer row `id`
    // (P0-02 5.6). Ids are explicit and must be 1, 2, 3, ... in the order
    // layers are written across all stages, i.e. id must equal
    // WorldWriter::next_layer_id(). FatalError if the stage is closed, the
    // id is out of order, layer_name is not a valid layer name
    // (is_valid_layer_name), the grid is invalid (is_valid_layer_grid), or
    // SQLite rejects the row (for example a duplicate name).
    void write_layer(std::int64_t id, std::string_view layer_name, const LayerGrid& grid,
                     Codec codec = Codec::delta_varint);

    // Inserts one row of table `pass` (P1-01 9.1). Ids are 1, 2, 3, ... in
    // the order rows are written across the world (ascending primary key,
    // P0-02 6 step 3). FatalError if the stage is closed, the id is out of
    // order, kind or capped is not 0 or 1, or SQLite rejects the row.
    void write_pass(const PassRow& row);

    // Inserts one row of table `biome` (P1-04 B6). Rows are written in
    // strictly ascending id (ascending primary key, P0-02 6 step 3).
    // FatalError if the stage is closed, the id is not above every id
    // written before (or below 1), water is not 0 or 1, or SQLite rejects
    // the row (for example a duplicate key).
    void write_biome(const BiomeRow& row);

    // Inserts the row of table `climate` (P1-03 C7). FatalError if the
    // stage is closed, the id is not 1, the table already has its row,
    // wind_toward is outside 0..7, or SQLite rejects the row.
    void write_climate(const ClimateRow& row);

    // Insert one row of tables `region`, `region_border` and `region_biome`
    // (P1-06 RG14). Rows of each table are written in strictly ascending
    // primary key (id; (a, b); (region, biome)), P0-02 6 step 3. FatalError
    // if the stage is closed, a key is not above every key written before
    // to that table, region.id is below 1, region_border has a >= b or a < 1,
    // region_biome has region < 1, or SQLite rejects the row.
    void write_region(const RegionRow& row);
    void write_region_border(const RegionBorderRow& row);
    void write_region_biome(const RegionBiomeRow& row);

    // COMMIT, and append the stage name to meta.stages (P0-02 5.5, 6 step 3).
    // FatalError if already committed or SQLite fails.
    void commit();

    // The stage name given to begin_stage.
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    friend class WorldWriter;
    friend struct testing::WriterAccess;
    StageWriter(detail::WriterState& state, std::string stage_name);

    detail::WriterState* state_;  // nullptr once committed, rolled back or moved from
    std::string name_;
};

// The writer: an in-memory working database plus the meta values the caller
// supplies (P0-02 4.2, 5.5, 6). Move-only. One writer produces one file.
class WorldWriter {
public:
    // Steps 1-2 of P0-02 section 6: opens the in-memory working database,
    // applies the pragmas of 4.2 in order, and creates the schema of 5.4 in
    // one transaction. FatalError if SQLite fails.
    WorldWriter();
    ~WorldWriter();
    WorldWriter(WorldWriter&& other) noexcept;
    WorldWriter& operator=(WorldWriter&& other) noexcept;
    WorldWriter(const WorldWriter&) = delete;
    WorldWriter& operator=(const WorldWriter&) = delete;

    // --- meta values (P0-02 5.5); each may be set again, the last value wins

    // A numeric world seed: meta.seed = canonical decimal, meta.seed_phrase
    // empty. Required (this or set_seed_phrase) before finalize().
    void set_seed(std::uint64_t value);

    // A text-phrase seed: phrase must already be scrubbed, i.e. 1 to 256
    // bytes and scrub_phrase(phrase) == phrase (P0-01 7.3, P0-02 amendment
    // 12); otherwise FatalError. Sets meta.seed_phrase = phrase and
    // meta.seed = phrase_seed(phrase).
    void set_seed_phrase(std::string_view phrase);

    // meta.content_hash = 64 lowercase hex digits of digest (P0-03 8).
    // Required before finalize().
    void set_content_hash(const Sha256Digest& digest);

    // meta.param.<name> = value in canonical decimal. FatalError if
    // "param." + name is not a valid meta key (is_valid_meta_key), or if
    // name is "map_size" and value is not in ALLOWED_MAP_SIZES.
    // param.map_size is required before finalize().
    void set_param_integer(std::string_view name, std::int64_t value);

    // meta.param.<name> = canonical_decimal(decimal_text) (P0-01 5.6), for
    // fixed-point parameters. FatalError if the name is invalid as above,
    // or the text fails the decimal grammar.
    void set_param_decimal(std::string_view name, std::string_view decimal_text);

    // The seed set so far, or 0 if none (stages derive their streams from it).
    [[nodiscard]] std::uint64_t seed() const noexcept;

    // --- stages (P0-02 6 step 3)

    // BEGIN a stage's transaction. FatalError if another stage is open, the
    // writer is finalized, stage_name is not a valid stage name
    // (is_valid_stage_name), or a stage of that name already committed.
    [[nodiscard]] StageWriter begin_stage(std::string_view stage_name);

    // The id the next layer must have: 1 + the number of layers written.
    [[nodiscard]] std::int64_t next_layer_id() const noexcept;

    // Names of the committed stages, in run order (becomes meta.stages).
    [[nodiscard]] const std::vector<std::string>& stages() const noexcept;

    // --- output (P0-02 6 steps 4-9)

    // Checks the output location without writing anything, in this order:
    //   - output_write_failed if out_utf8 is empty, not strict UTF-8, or
    //     contains a NUL byte (is_valid_utf8_path);
    //   - output_exists if out_utf8 is a directory (whatever overwrite says),
    //     or exists and overwrite is refuse;
    //   - output_parent_missing if its parent directory does not exist.
    // finalize() runs the same checks first; a caller may run them before
    // generating to fail early.
    [[nodiscard]] static Status check_output(std::string_view out_utf8, Overwrite overwrite);

    // Steps 4-9 of P0-02 section 6: checks the output location as
    // check_output does; writes meta (ascending key order); runs
    // integrity_check; removes any old <out>.partial and
    // <out>.partial-journal; VACUUM INTO <out>.partial; closes the working
    // database; verifies the header (6.1) and meta of <out>.partial; and
    // renames it to <out>. out_utf8 is a UTF-8 path, relative paths
    // resolving against the current directory.
    //
    // Errors returned for problems with the output location:
    //   - output_write_failed: the path is empty, not strict UTF-8 or holds
    //     a NUL byte (nothing is written); an old <out>.partial or
    //     <out>.partial-journal cannot be removed; VACUUM INTO fails; or,
    //     under refuse, the existence check of <out> just before the rename
    //     reports an error (for example permission denied);
    //   - output_exists: <out> is a directory, or exists under refuse
    //     (checked at the start and again just before the rename);
    //   - output_parent_missing: the parent directory does not exist;
    //   - output_rename_failed: the rename into place did not succeed.
    // Raises FatalError for contract violations: a stage still open, seed /
    // content hash / param.map_size not set, finalize() called twice, a
    // failed integrity_check or header verification.
    //
    // Rename and the existing <out>. Ok is returned only when a rename call
    // succeeded, so the new file is at <out>. On POSIX the rename replaces
    // an existing <out> atomically. Under replace, if the first rename
    // fails and the replace fallback is enabled (always on Windows; on
    // other platforms only while testing::FsFaults are set), an existing
    // regular-file <out> is removed and the rename is tried once more. The
    // fallback is not atomic: if that retry fails, the old <out> is gone
    // and output_rename_failed is returned. If removing <out> fails, the
    // old <out> is left untouched. Apart from this fallback, an existing
    // <out> is only ever replaced by a successful rename.
    //
    // On every failure path, returned or thrown, the working database is
    // closed and <out>.partial and <out>.partial-journal are deleted. After
    // finalize(), the writer accepts no further calls except destruction.
    [[nodiscard]] Status finalize(std::string_view out_utf8, Overwrite overwrite);

private:
    friend struct testing::WriterAccess;
    std::unique_ptr<detail::WriterState> state_;
};

}  // namespace worldgen::db
