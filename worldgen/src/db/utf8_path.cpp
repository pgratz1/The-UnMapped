#include "worldgen/db/utf8_path.hpp"

#include <cstddef>
#include <cstdint>

namespace worldgen::db {

bool is_valid_utf8_path(std::string_view utf8) noexcept {
    if (utf8.empty()) {
        return false;
    }
    std::size_t i = 0;
    while (i < utf8.size()) {
        const auto b0 = static_cast<std::uint8_t>(utf8[i]);
        if (b0 == 0U) {
            return false;
        }
        if (b0 < 0x80U) {
            ++i;
            continue;
        }
        std::size_t length = 0;
        std::uint32_t lo = 0x80U;  // allowed range of the second byte
        std::uint32_t hi = 0xBFU;
        if (b0 >= 0xC2U && b0 <= 0xDFU) {
            length = 2;
        } else if (b0 >= 0xE0U && b0 <= 0xEFU) {
            length = 3;
            if (b0 == 0xE0U) lo = 0xA0U;  // no overlong forms
            if (b0 == 0xEDU) hi = 0x9FU;  // no surrogates
        } else if (b0 >= 0xF0U && b0 <= 0xF4U) {
            length = 4;
            if (b0 == 0xF0U) lo = 0x90U;  // no overlong forms
            if (b0 == 0xF4U) hi = 0x8FU;  // nothing above U+10FFFF
        } else {
            return false;
        }
        if (utf8.size() - i < length) {
            return false;
        }
        for (std::size_t k = 1; k < length; ++k) {
            const auto b = static_cast<std::uint8_t>(utf8[i + k]);
            const std::uint32_t min = k == 1 ? lo : 0x80U;
            const std::uint32_t max = k == 1 ? hi : 0xBFU;
            if (b < min || b > max) {
                return false;
            }
        }
        i += length;
    }
    return true;
}

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
