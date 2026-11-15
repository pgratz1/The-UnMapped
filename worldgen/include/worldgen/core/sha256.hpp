#pragma once

// SHA-256, FIPS 180-4 (spec P0-01 section 10).

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace worldgen {

// A SHA-256 digest: 32 bytes in the standard (big-endian) output order.
using Sha256Digest = std::array<std::uint8_t, 32>;

// Incremental SHA-256 (P0-01 10). Feed bytes with update() in any number of
// pieces; final() returns the digest of everything fed since construction
// (or since the previous final()) and resets the object to the empty state,
// so it can be reused.
class Sha256 {
public:
    Sha256() noexcept;

    // Appends bytes to the message.
    void update(std::span<const std::uint8_t> bytes) noexcept;
    // Appends text to the message, each char as one uint8 byte.
    void update(std::string_view text) noexcept;

    // Digest of the message so far; then resets to the empty state.
    [[nodiscard]] Sha256Digest final() noexcept;

private:
    void reset() noexcept;
    void compress_block(const std::uint8_t* block) noexcept;

    std::array<std::uint32_t, 8> h_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t buffer_len_ = 0;   // bytes in buffer_, 0..63
    std::uint64_t total_len_ = 0;    // message length in bytes
};

// One-shot SHA-256 of bytes or text (P0-01 10).
[[nodiscard]] Sha256Digest sha256(std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] Sha256Digest sha256(std::string_view text) noexcept;

// 64 lowercase hex digits of a digest (P0-01 10).
[[nodiscard]] std::string to_hex(const Sha256Digest& digest);

// Lowercase hex of the one-shot SHA-256 (P0-01 10).
[[nodiscard]] std::string sha256_hex(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string sha256_hex(std::string_view text);

}  // namespace worldgen
