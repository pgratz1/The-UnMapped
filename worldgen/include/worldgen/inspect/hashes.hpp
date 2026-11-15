#pragma once

// File and logical hashes for worldgen-cli hash (spec P0-04 section 8).
//
// All hashes are SHA-256 (P0-01 10). The logical hashes are taken over byte
// streams built with: u8, u32, u64, i64 little-endian; str(s) = u64 byte
// length then the bytes; a column value = tag byte then content (NULL 0x00;
// INTEGER 0x01, i64; TEXT 0x03, str; BLOB 0x04, str).

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/sha256.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::inspect {

// SHA-256 of a file's bytes exactly as stored (P0-04 8, default output). The
// file need not be a world. file_unreadable if it cannot be opened or read
// (or is not a regular file).
[[nodiscard]] db::Result<Sha256Digest> hash_file(std::string_view path_utf8);

// Logical hash of a layer (P0-04 8, "layer"): u32 width, u32 height, u8
// element type code (P0-02 7.1), then every element in index order,
// little-endian at its natural size. Independent of the codec. FatalError if
// the grid is invalid.
[[nodiscard]] Sha256Digest layer_hash(const db::LayerGrid& grid);

// A name and its logical hash.
struct NamedHash {
    std::string name;
    Sha256Digest digest{};
};

// The logical hashes of an open world (P0-04 8, --detail).
struct DetailHashes {
    // sqlite_schema rows ordered by type then name, each as the column values
    // of type, name, tbl_name, sql (rootpage excluded).
    Sha256Digest schema{};
    // One per table except sqlite_* tables, ascending name: u64 column count,
    // then every row in primary-key order, each column in declared order.
    std::vector<NamedHash> tables;
    // One per layer, in id order: layer_hash of the decoded grid.
    std::vector<NamedHash> layers;
};

// Computes every logical hash of an open world (P0-04 8). Errors: real_value
// for a REAL anywhere, no_primary_key for a table without one,
// corrupt_layer for a layer that does not decode, and the reader's other
// errors.
[[nodiscard]] db::Result<DetailHashes> detail_hashes(const db::WorldReader& world);

// The --detail output (P0-04 8), each line ending in a line feed:
//   file <hex>
//   schema <hex>
//   table <name> <hex>    (one per table)
//   layer <name> <hex>    (one per layer)
// Names are escaped as in the report (escape_report_text).
[[nodiscard]] std::string format_detail(const Sha256Digest& file, const DetailHashes& detail);

// True if text is 64 hex digits of either case (P0-04 8, --expect).
[[nodiscard]] bool is_sha256_hex(std::string_view text) noexcept;

}  // namespace worldgen::inspect
