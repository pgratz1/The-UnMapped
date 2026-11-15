// Internal: pieces shared by the settings and content loaders (P0-03 4, 5, 9).

#include "data/common.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/text.hpp"

namespace worldgen::data::detail {

void ErrorList::add(std::string_view path, Position at, std::string message) {
    errors_.push_back(LoadError{std::string(path), at.line, at.column, std::move(message)});
}

std::vector<LoadError> ErrorList::take_sorted() {
    std::vector<LoadError> out = std::move(errors_);
    errors_.clear();
    sort_errors(out);
    return out;
}

std::optional<std::string> read_file(const std::filesystem::path& path, std::string_view display_path,
                                     ErrorList& errors) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        errors.add(display_path, Position{}, "cannot open the file for reading");
        return std::nullopt;
    }
    // Read at most one byte more than the limit, so that an oversized file
    // is detected without reading it all.
    std::string bytes(MAX_FILE_BYTES + 1, '\0');
    in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (in.bad()) {
        errors.add(display_path, Position{}, "cannot read the file");
        return std::nullopt;
    }
    const std::streamsize got = in.gcount();
    bytes.resize(static_cast<std::size_t>(got));
    return bytes;
}

namespace {

std::string float_message(std::string_view key) {
    return "floating-point value for " + in_quotes(key) +
           "; write it as a decimal string such as \"0.6\", or as an integer";
}

void scan(const Node& node, const std::string& key, std::string_view path, ErrorList& errors) {
    switch (node.kind) {
        case NodeKind::table:
            for (std::size_t i = 0; i < node.children.size(); ++i) {
                scan(node.children[i], key.empty() ? node.keys[i] : child_key(key, node.keys[i]), path, errors);
            }
            return;
        case NodeKind::array:
            for (std::size_t i = 0; i < node.children.size(); ++i) {
                scan(node.children[i], element_key(key, static_cast<std::uint64_t>(i)), path, errors);
            }
            return;
        case NodeKind::floating_point:
            errors.add(path, node.position, float_message(key));
            return;
        case NodeKind::date_time:
            errors.add(path, node.position, "date and time values are not allowed (found one for " + in_quotes(key) + ")");
            return;
        case NodeKind::string: {
            const std::int32_t bad = first_forbidden_control(node.text);
            if (bad >= 0) {
                errors.add(path, node.position,
                           "control character " + code_point_text(static_cast<std::uint32_t>(bad)) +
                               " is not allowed in a string (" + in_quotes(key) + ")");
            }
            return;
        }
        case NodeKind::integer:
        case NodeKind::boolean:
            return;
    }
}

}  // namespace

bool parse_file(std::string_view display_path, std::string_view bytes, ErrorList& errors, Node& root) {
    if (static_cast<std::uint64_t>(bytes.size()) > MAX_FILE_BYTES) {
        errors.add(display_path, Position{}, "file is larger than 1 MiB (1048576 bytes)");
        return false;
    }
    ParsedDocument doc = parse_document(bytes);
    if (!doc.ok) {
        errors.add(display_path, doc.error_position, std::move(doc.error_message));
        return false;
    }
    root = std::move(doc.root);
    scan(root, std::string(), display_path, errors);
    return true;
}

bool is_rejected_scalar(const Node& node) noexcept {
    switch (node.kind) {
        case NodeKind::floating_point:
        case NodeKind::date_time:
            return true;
        case NodeKind::string:
            return first_forbidden_control(node.text) >= 0;
        case NodeKind::table:
        case NodeKind::array:
        case NodeKind::integer:
        case NodeKind::boolean:
            return false;
    }
    return false;
}

std::string_view kind_phrase(const Node& node) noexcept {
    switch (node.kind) {
        case NodeKind::table: return "a table";
        case NodeKind::array: return "an array";
        case NodeKind::string: return "a string";
        case NodeKind::integer: return "an integer";
        case NodeKind::boolean: return "a boolean";
        case NodeKind::floating_point: return "a floating-point value";
        case NodeKind::date_time: return "a date or time";
    }
    return "a value";
}

std::string child_key(std::string_view key, std::string_view child) {
    std::string out(key);
    out.push_back('.');
    out.append(child);
    return out;
}

std::string element_key(std::string_view key, std::uint64_t index) {
    std::string out(key);
    out.push_back('[');
    out.append(uint_text(index));
    out.push_back(']');
    return out;
}

std::string path_utf8(const std::filesystem::path& path) {
    const std::u8string s = path.u8string();
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}

}  // namespace worldgen::data::detail
