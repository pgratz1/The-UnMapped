#pragma once
// Internal: pieces shared by the settings loader and the content loader
// (P0-03 sections 4, 5, 9).
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "data/document.hpp"
#include "worldgen/data/load_error.hpp"

namespace worldgen::data::detail {

// Maximum size of one TOML file (P0-03 section 5).
inline constexpr std::uint64_t MAX_FILE_BYTES = 1048576;

// Collects errors; sorted once at the end (P0-03 9).
class ErrorList {
public:
    void add(std::string_view path, Position at, std::string message);
    [[nodiscard]] bool empty() const noexcept { return errors_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return errors_.size(); }
    // Sorted and deduplicated.
    [[nodiscard]] std::vector<LoadError> take_sorted();

private:
    std::vector<LoadError> errors_;
};

// Reads a whole file in binary mode, at most MAX_FILE_BYTES (P0-03 5).
// On failure adds an error at display_path and returns std::nullopt.
[[nodiscard]] std::optional<std::string> read_file(const std::filesystem::path& path, std::string_view display_path,
                                                   ErrorList& errors);

// Size check, parse and value scan of one file's bytes (P0-03 4, 5). A
// syntax or nesting error stops this file only: it is reported and false
// returned. Otherwise every floating-point value, date/time value and
// string with a forbidden control character anywhere in the document is
// reported (the document is still returned, and validation skips those
// nodes: see is_rejected_scalar), and true is returned.
[[nodiscard]] bool parse_file(std::string_view display_path, std::string_view bytes, ErrorList& errors,
                              Node& root);

// True for a node that the value scan already reported: a float, a
// date/time, or a string holding a forbidden control character. Validators
// fail on such a node without a second message.
[[nodiscard]] bool is_rejected_scalar(const Node& node) noexcept;

// "a string", "an integer", ... for "found ..." in messages.
[[nodiscard]] std::string_view kind_phrase(const Node& node) noexcept;

// key + "." + child (dotted key, P0-03 9) and key + "[i]" for array elements.
[[nodiscard]] std::string child_key(std::string_view key, std::string_view child);
[[nodiscard]] std::string element_key(std::string_view key, std::uint64_t index);

// The UTF-8 bytes of a path in its native format (as given).
[[nodiscard]] std::string path_utf8(const std::filesystem::path& path);

}  // namespace worldgen::data::detail
