#pragma once
// Internal: a TOML document as a project-owned tree (P0-03 sections 2, 4, 5).
//
// toml++ is included by exactly one source file, toml_document.cpp, which
// turns its tree into this one. The conversion copies only what the loader
// may use: node kinds, integers, booleans, string bytes and source
// positions. A floating-point or date/time node is recorded by its kind and
// position only; its value is never read. Table keys are sorted by bytes
// here, so nothing downstream sees the parser's iteration order.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace worldgen::data::detail {

// 1-based line and column from the parser; 0, 0 for "no position".
struct Position {
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

enum class NodeKind : std::uint8_t {
    table,           // standard or inline table
    array,
    string,
    integer,
    boolean,
    floating_point,  // value never read (P0-03 section 4)
    date_time,       // any of the four TOML date/time types; value never read
};

struct Node {
    NodeKind kind = NodeKind::table;
    Position position;
    std::int64_t integer = 0;       // integer
    bool boolean = false;           // boolean
    std::string text;               // string, after CR LF -> LF (P0-03 section 4)
    std::vector<std::string> keys;  // table: keys, ascending by bytes
    std::vector<Node> children;     // table: values in key order; array: elements
};

// Maximum nesting of tables and arrays below the document root (P0-03 5).
inline constexpr std::uint32_t MAX_NESTING = 8;

struct ParsedDocument {
    bool ok = false;
    Node root;               // when ok
    Position error_position; // when !ok
    std::string error_message;
};

// Parses TOML text (P0-03 section 5): syntax errors (including invalid
// UTF-8 and duplicate keys) come back with the parser's position and
// message; a table or array nested deeper than MAX_NESTING below the root
// is an error at that node. A leading UTF-8 byte-order mark is skipped by
// the parser. Size limits are the caller's.
[[nodiscard]] ParsedDocument parse_document(std::string_view bytes);

}  // namespace worldgen::data::detail
