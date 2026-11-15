// SHA-256, FIPS 180-4 (P0-01 10).

#include "worldgen/core/sha256.hpp"

#include <cstddef>

namespace worldgen {

namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr std::array<std::uint32_t, 8> kInitial = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
};

constexpr std::uint32_t rotr(std::uint32_t x, unsigned int n) noexcept {
    // 0 < n < 32 at every call site.
    return (x >> n) | (x << (32U - n));
}

std::uint32_t load_be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24U) | (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) | static_cast<std::uint32_t>(p[3]);
}

}  // namespace

Sha256::Sha256() noexcept {
    reset();
}

void Sha256::reset() noexcept {
    h_ = kInitial;
    buffer_.fill(0);
    buffer_len_ = 0;
    total_len_ = 0;
}

void Sha256::compress_block(const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t t = 0; t < 16; ++t) {
        w[t] = load_be32(block + 4U * t);
    }
    for (std::size_t t = 16; t < 64; ++t) {
        const std::uint32_t s0 = rotr(w[t - 15], 7U) ^ rotr(w[t - 15], 18U) ^ (w[t - 15] >> 3U);
        const std::uint32_t s1 = rotr(w[t - 2], 17U) ^ rotr(w[t - 2], 19U) ^ (w[t - 2] >> 10U);
        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    std::uint32_t a = h_[0];
    std::uint32_t b = h_[1];
    std::uint32_t c = h_[2];
    std::uint32_t d = h_[3];
    std::uint32_t e = h_[4];
    std::uint32_t f = h_[5];
    std::uint32_t g = h_[6];
    std::uint32_t h = h_[7];
    for (std::size_t t = 0; t < 64; ++t) {
        const std::uint32_t big_s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + big_s1 + ch + kRound[t] + w[t];
        const std::uint32_t big_s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = big_s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
    h_[5] += f;
    h_[6] += g;
    h_[7] += h;
}

void Sha256::update(std::span<const std::uint8_t> bytes) noexcept {
    total_len_ += static_cast<std::uint64_t>(bytes.size());
    std::size_t pos = 0;
    while (pos < bytes.size()) {
        if (buffer_len_ == 0 && bytes.size() - pos >= 64U) {
            compress_block(bytes.data() + pos);
            pos += 64U;
            continue;
        }
        buffer_[static_cast<std::size_t>(buffer_len_)] = bytes[pos];
        ++buffer_len_;
        ++pos;
        if (buffer_len_ == 64U) {
            compress_block(buffer_.data());
            buffer_len_ = 0;
        }
    }
}

void Sha256::update(std::string_view text) noexcept {
    // Reinterpret each char as a byte, without relying on char signedness.
    std::array<std::uint8_t, 256> chunk{};
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t n = text.size() - pos;
        if (n > chunk.size()) {
            n = chunk.size();
        }
        for (std::size_t i = 0; i < n; ++i) {
            chunk[i] = static_cast<std::uint8_t>(text[pos + i]);
        }
        update(std::span<const std::uint8_t>(chunk.data(), n));
        pos += n;
    }
}

Sha256Digest Sha256::final() noexcept {
    const std::uint64_t bit_len = total_len_ * 8U;  // mod 2^64, per FIPS 180-4
    std::array<std::uint8_t, 72> pad{};
    pad[0] = 0x80U;
    // Pad to 56 mod 64, then the 8-byte big-endian bit length.
    const std::uint64_t rem = total_len_ % 64U;
    const std::uint64_t zeros = rem < 56U ? 55U - rem : 119U - rem;
    const auto pad_len = static_cast<std::size_t>(1U + zeros);
    for (std::size_t i = 0; i < 8U; ++i) {
        pad[pad_len + i] = static_cast<std::uint8_t>((bit_len >> (56U - 8U * i)) & 0xFFU);
    }
    // Padding goes through update(); total_len_ is not used after this.
    update(std::span<const std::uint8_t>(pad.data(), pad_len + 8U));

    Sha256Digest out{};
    for (std::size_t i = 0; i < 8U; ++i) {
        out[4U * i + 0U] = static_cast<std::uint8_t>((h_[i] >> 24U) & 0xFFU);
        out[4U * i + 1U] = static_cast<std::uint8_t>((h_[i] >> 16U) & 0xFFU);
        out[4U * i + 2U] = static_cast<std::uint8_t>((h_[i] >> 8U) & 0xFFU);
        out[4U * i + 3U] = static_cast<std::uint8_t>(h_[i] & 0xFFU);
    }
    reset();
    return out;
}

Sha256Digest sha256(std::span<const std::uint8_t> bytes) noexcept {
    Sha256 h;
    h.update(bytes);
    return h.final();
}

Sha256Digest sha256(std::string_view text) noexcept {
    Sha256 h;
    h.update(text);
    return h.final();
}

std::string to_hex(const Sha256Digest& digest) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const std::uint8_t b : digest) {
        out.push_back(kHex[b >> 4U]);
        out.push_back(kHex[b & 0x0FU]);
    }
    return out;
}

std::string sha256_hex(std::span<const std::uint8_t> bytes) {
    return to_hex(sha256(bytes));
}

std::string sha256_hex(std::string_view text) {
    return to_hex(sha256(text));
}

}  // namespace worldgen
