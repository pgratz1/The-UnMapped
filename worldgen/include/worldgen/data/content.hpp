#pragma once
// Loading the data directory and the content hash (spec P0-03 sections 3,
// 7, 8 and 9).
//
// For each registered kind (ascending name), the loader lists
// <root>/<directory>, keeps the entries named [a-z0-9][a-z0-9_-]*\.toml
// (ignoring names starting with '.' and names not ending in ".toml"),
// parses them in byte order of their names, validates every definition
// against the kind's schema, applies defaults, checks refs, and returns the
// definitions of each kind in ascending id order (P0-03 7.2 to 7.5).
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/sha256.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/data/value.hpp"

namespace worldgen::data {

// One validated definition (P0-03 7.3 to 7.5): its id and its record (every
// schema field, in declaration order, defaults applied).
struct Definition {
    std::string id;
    Record record;

    friend bool operator==(const Definition&, const Definition&) = default;
};

// The definitions of one kind, strictly ascending id by bytes (P0-03 7.4).
struct KindContent {
    std::string kind;
    std::vector<Definition> definitions;

    // The definition with that id, or nullptr.
    [[nodiscard]] const Definition* find(std::string_view id) const noexcept;

    friend bool operator==(const KindContent&, const KindContent&) = default;
};

// The loaded content (P0-03 section 3): one entry per registered kind,
// ascending kind name, including kinds with zero definitions. Contains only
// validated values: no file names, positions or comments, so two data sets
// with the same validated values compare equal.
struct ContentSet {
    std::vector<KindContent> kinds;

    // The kind of that name, or nullptr.
    [[nodiscard]] const KindContent* find_kind(std::string_view kind) const noexcept;
    // The definition kind/id, or nullptr.
    [[nodiscard]] const Definition* find(std::string_view kind, std::string_view id) const noexcept;

    friend bool operator==(const ContentSet&, const ContentSet&) = default;
};

// Loads every registered kind from the data root (P0-03 sections 5, 7).
// Kind directories are matched by exact name (bytes), so a directory
// `Biomes` does not stand in for `biomes` even on a case-insensitive
// filesystem. Symbolic links are rejected, for kind directories and for data
// files. With no kinds registered nothing is read and the root is not
// checked. Error paths are relative to the root with '/' separators, except
// an error about the root itself, which uses the root as given.
// Raises FatalError if a ref field targets a kind that is not registered.
[[nodiscard]] LoadResult<ContentSet> load_content(const KindRegistry& registry,
                                                  const std::filesystem::path& data_root);

// The content when no data root is given (P0-03 7.6, P0): every registered
// kind with zero definitions; a required kind is an error (its directory is
// missing). With the P0 registry the result is an empty ContentSet.
[[nodiscard]] LoadResult<ContentSet> load_content_without_data(const KindRegistry& registry);

// --- In-memory data roots, for tests (P0-03 section 13) ---------------------

// One directory entry in memory. `name` is the entry name as a filesystem
// would list it (e.g. "a.toml", "Upper.toml", "notes.md") and goes through
// the same discovery rules (P0-03 7.2). An entry with is_directory = true
// stands for a subdirectory (e.g. "x.toml/"), which is an error when its
// name would otherwise be loaded; bytes is then ignored.
struct MemoryEntry {
    std::string name;
    std::string bytes;
    bool is_directory = false;
};

// One kind directory in memory, e.g. name "alphas". A directory that is
// listed exists (possibly empty); one that is not listed is missing.
struct MemoryDirectory {
    std::string name;
    std::vector<MemoryEntry> entries;
};

// A data root in memory: its directories, in any order (they are matched by
// exact name).
struct MemoryDataRoot {
    std::vector<MemoryDirectory> directories;
};

// As load_content, from memory. Error paths are "<directory>/<name>".
[[nodiscard]] LoadResult<ContentSet> load_content_from_memory(const KindRegistry& registry,
                                                              const MemoryDataRoot& root);

// --- Canonical encoding and content hash (P0-03 section 8) ------------------

// The canonical byte stream of P0-03 section 8:
//   for each kind (ascending name): str(name) u64(count)
//     for each definition (ascending id): str(id) record(definition)
//   record(r) = for each field in declaration order: str(field name) value
//   value: int 0x01 i64 | fx 0x02 i64(raw) | bool 0x03 0x00/0x01 |
//          string/enum/ref 0x04 str | array 0x05 u64(count) values |
//          record 0x06 record
//   str(s) = u64(byte length) bytes; u64, i64 little-endian.
// Empty for an empty ContentSet.
[[nodiscard]] std::vector<std::uint8_t> canonical_encoding(const ContentSet& content);

// SHA-256 of canonical_encoding(content) (P0-03 8, P0-01 10). For an empty
// ContentSet: e3b0c442...7852b855.
[[nodiscard]] Sha256Digest content_hash(const ContentSet& content);

// content_hash as 64 lowercase hex digits, as stored in meta.content_hash
// (P0-02).
[[nodiscard]] std::string content_hash_hex(const ContentSet& content);

}  // namespace worldgen::data
