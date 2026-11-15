// The only translation unit in worldgen that includes toml++ (P0-03 sections
// 2, 4, 5). It parses text and copies the result into the project-owned tree
// of document.hpp.
//
// Floating point: toml++ parses a float literal into a double inside
// toml::parse; that cannot be avoided. This file never reads such a node's
// value: it switches on node.type() and, for node_type::floating_point,
// records only the kind and the source position. No toml++ value
// conversion (value<T>, value_or, as_floating_point()->get()), formatter or
// stream output is used anywhere in the loader.

// The toml++ configuration is set once for every translation unit as
// INTERFACE compile definitions of unmapped::tomlplusplus
// (extern/CMakeLists.txt, block "TOML++ CONFIGURATION"). Refuse to build
// with anything else.
#if !defined(TOML_EXCEPTIONS) || TOML_EXCEPTIONS != 1 || !defined(TOML_HEADER_ONLY) || TOML_HEADER_ONLY != 1 \
    || !defined(TOML_ENABLE_FORMATTERS) || TOML_ENABLE_FORMATTERS != 0                                        \
    || !defined(TOML_ENABLE_WINDOWS_COMPAT) || TOML_ENABLE_WINDOWS_COMPAT != 0                                \
    || !defined(TOML_ENABLE_UNRELEASED_FEATURES) || TOML_ENABLE_UNRELEASED_FEATURES != 0                      \
    || !defined(TOML_FLOAT_CHARCONV) || TOML_FLOAT_CHARCONV != 1                                              \
    || !defined(TOML_INT_CHARCONV) || TOML_INT_CHARCONV != 1                                                  \
    || !defined(TOML_ENABLE_SIMD) || TOML_ENABLE_SIMD != 0                                                    \
    || !defined(TOML_ENABLE_FLOAT16) || TOML_ENABLE_FLOAT16 != 0                                              \
    || !defined(TOML_MAX_NESTED_VALUES) || TOML_MAX_NESTED_VALUES != 256
#error "toml++ must be configured by unmapped::tomlplusplus (see extern/CMakeLists.txt)"
#endif

// toml++ silently falls back to string streams when <charconv> is missing,
// so check here that floating-point std::from_chars exists (GCC 11+,
// MSVC 2019 16.4+): TOML_FLOAT_CHARCONV=1 must really mean from_chars.
#include <charconv>
#if !defined(__cpp_lib_to_chars) || __cpp_lib_to_chars < 201611L
#error "the TOML loader needs floating-point std::from_chars (TOML_FLOAT_CHARCONV=1)"
#endif

#include "data/document.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <toml.hpp>
#include <utility>
#include <vector>

#include "data/text.hpp"

namespace worldgen::data::detail {

namespace {

Position position_of(const toml::source_region& region) noexcept {
    return Position{static_cast<std::uint32_t>(region.begin.line), static_cast<std::uint32_t>(region.begin.column)};
}

// Replaces each CR LF pair by LF (P0-03 section 4). Other bytes unchanged.
std::string normalize_newlines(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '\r' && i + 1 < in.size() && in[i + 1] == '\n') {
            continue;  // drop the CR; the LF follows
        }
        out.push_back(in[i]);
    }
    return out;
}

class Converter {
public:
    // Returns false and fills the error if a container is nested too deeply.
    bool convert(const toml::node& in, std::uint32_t depth, Node& out) {
        out.position = position_of(in.source());
        switch (in.type()) {
            case toml::node_type::table:
                out.kind = NodeKind::table;
                return check_depth(depth, out.position) && convert_table(*in.as_table(), depth, out);
            case toml::node_type::array:
                out.kind = NodeKind::array;
                return check_depth(depth, out.position) && convert_array(*in.as_array(), depth, out);
            case toml::node_type::string:
                out.kind = NodeKind::string;
                out.text = normalize_newlines(in.as_string()->get());
                return true;
            case toml::node_type::integer:
                out.kind = NodeKind::integer;
                out.integer = in.as_integer()->get();
                return true;
            case toml::node_type::boolean:
                out.kind = NodeKind::boolean;
                out.boolean = in.as_boolean()->get();
                return true;
            case toml::node_type::floating_point:
                // Kind and position only; the double is never read.
                out.kind = NodeKind::floating_point;
                return true;
            case toml::node_type::date:
            case toml::node_type::time:
            case toml::node_type::date_time:
                out.kind = NodeKind::date_time;
                return true;
            case toml::node_type::none:
                break;
        }
        // toml++ never produces a node of type none in a parsed document.
        error_position = out.position;
        error_message = "internal error: unexpected TOML node type";
        return false;
    }

    Position error_position;
    std::string error_message;

private:
    bool check_depth(std::uint32_t depth, Position at) {
        if (depth > MAX_NESTING) {
            error_position = at;
            error_message = "tables and arrays are nested more than 8 deep";
            return false;
        }
        return true;
    }

    bool convert_table(const toml::table& in, std::uint32_t depth, Node& out) {
        // Copy the keys and sort them by bytes; the parser's order is not used.
        std::vector<std::pair<std::string, const toml::node*>> entries;
        entries.reserve(in.size());
        for (auto&& [key, value] : in) {
            entries.emplace_back(std::string(key.str()), &value);
        }
        // determinism-lint: allow(unstable-sort) -- keys of one TOML table are unique (duplicates are a parse error, and toml::table is a map), so bytes_less on the key is a strict total order on the entries
        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b) { return bytes_less(a.first, b.first); });
        out.keys.reserve(entries.size());
        out.children.resize(entries.size());
        for (std::size_t i = 0; i < entries.size(); ++i) {
            out.keys.push_back(entries[i].first);
            if (!convert(*entries[i].second, depth + 1, out.children[i])) {
                return false;
            }
        }
        return true;
    }

    bool convert_array(const toml::array& in, std::uint32_t depth, Node& out) {
        out.children.resize(in.size());
        for (std::size_t i = 0; i < in.size(); ++i) {
            if (!convert(in[i], depth + 1, out.children[i])) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace

ParsedDocument parse_document(std::string_view bytes) {
    ParsedDocument result;
    toml::table table;
    try {
        table = toml::parse(bytes);
    } catch (const toml::parse_error& e) {
        result.error_position = position_of(e.source());
        result.error_message = std::string(e.description());
        return result;
    }
    // The root is depth 0; its tables and arrays are depth 1 and so on.
    Converter converter;
    if (!converter.convert(table, 0, result.root)) {
        result.error_position = converter.error_position;
        result.error_message = converter.error_message;
        result.root = Node{};
        return result;
    }
    result.ok = true;
    return result;
}

}  // namespace worldgen::data::detail
