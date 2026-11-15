#include "worldgen/db/utf8_path.hpp"

namespace worldgen::db {

std::filesystem::path path_from_utf8(std::string_view utf8) {
    std::u8string text;
    text.reserve(utf8.size());
    for (const char c : utf8) {
        text.push_back(static_cast<char8_t>(static_cast<unsigned char>(c)));
    }
    return std::filesystem::path(text);
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    std::string out;
    out.reserve(text.size());
    for (const char8_t c : text) {
        out.push_back(static_cast<char>(c));
    }
    return out;
}

}  // namespace worldgen::db
