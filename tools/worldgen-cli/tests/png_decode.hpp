#pragma once

// A small PNG reader for the render tests: 8-bit RGB (colour type 2), no
// interlace, as worldgen-cli writes (P0-04 6). Inflate (RFC 1951: stored,
// fixed and dynamic Huffman blocks) and the five scanline filters (PNG 9).
// Written for the tests; returns an empty result on any malformed input.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace pngtest {

struct Image {
    bool ok = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgb;  // width * height * 3
};

namespace detail {

struct Bits {
    const std::vector<std::uint8_t>& in;
    std::size_t pos = 0;  // byte
    std::uint32_t bit = 0;
    bool bad = false;

    std::uint32_t get(std::uint32_t n) {
        std::uint32_t v = 0;
        for (std::uint32_t i = 0; i < n; ++i) {
            if (pos >= in.size()) {
                bad = true;
                return 0;
            }
            v |= static_cast<std::uint32_t>((in[pos] >> bit) & 1U) << i;
            if (++bit == 8) {
                bit = 0;
                ++pos;
            }
        }
        return v;
    }
    void align() {
        if (bit != 0) {
            bit = 0;
            ++pos;
        }
    }
};

// Canonical Huffman decoding table: counts per length and symbols by code.
struct Huffman {
    std::array<std::uint16_t, 16> count{};
    std::vector<std::uint16_t> symbol;

    bool build(const std::vector<std::uint8_t>& lengths) {
        count.fill(0);
        for (const std::uint8_t l : lengths) {
            ++count[l];
        }
        count[0] = 0;
        std::array<std::uint16_t, 16> offs{};
        for (std::size_t l = 1; l < 16; ++l) {
            offs[l] = static_cast<std::uint16_t>(offs[l - 1] + count[l - 1]);
        }
        symbol.assign(lengths.size(), 0);
        for (std::size_t s = 0; s < lengths.size(); ++s) {
            if (lengths[s] != 0) {
                symbol[offs[lengths[s]]++] = static_cast<std::uint16_t>(s);
            }
        }
        return true;
    }

    int decode(Bits& b) const {
        int code = 0, first = 0, index = 0;
        for (std::size_t len = 1; len < 16; ++len) {
            code |= static_cast<int>(b.get(1));
            const int c = count[len];
            if (code - c < first) {
                return symbol[static_cast<std::size_t>(index + (code - first))];
            }
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
            if (b.bad) {
                return -1;
            }
        }
        return -1;
    }
};

inline constexpr std::array<std::uint16_t, 29> LEN_BASE = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                                            15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                                            67, 83, 99, 115, 131, 163, 195, 227, 258};
inline constexpr std::array<std::uint8_t, 29> LEN_EXTRA = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
inline constexpr std::array<std::uint16_t, 30> DIST_BASE = {1,    2,    3,    4,    5,    7,     9,     13,
                                                             17,   25,   33,   49,   65,   97,    129,   193,
                                                             257,  385,  513,  769,  1025, 1537,  2049,  3073,
                                                             4097, 6145, 8193, 12289, 16385, 24577};
inline constexpr std::array<std::uint8_t, 30> DIST_EXTRA = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

inline bool codes(Bits& b, std::vector<std::uint8_t>& out, const Huffman& lit, const Huffman& dist) {
    for (;;) {
        const int s = lit.decode(b);
        if (s < 0) {
            return false;
        }
        if (s < 256) {
            out.push_back(static_cast<std::uint8_t>(s));
        } else if (s == 256) {
            return true;
        } else {
            const std::size_t li = static_cast<std::size_t>(s - 257);
            if (li >= 29) {
                return false;
            }
            const std::size_t len = LEN_BASE[li] + b.get(LEN_EXTRA[li]);
            const int ds = dist.decode(b);
            if (ds < 0 || ds >= 30) {
                return false;
            }
            const std::size_t d = DIST_BASE[static_cast<std::size_t>(ds)] + b.get(DIST_EXTRA[static_cast<std::size_t>(ds)]);
            if (d > out.size()) {
                return false;
            }
            for (std::size_t k = 0; k < len; ++k) {
                out.push_back(out[out.size() - d]);
            }
        }
        if (b.bad) {
            return false;
        }
    }
}

inline bool inflate(const std::vector<std::uint8_t>& z, std::vector<std::uint8_t>& out) {
    if (z.size() < 2 || (z[0] & 0x0FU) != 8U) {
        return false;
    }
    std::vector<std::uint8_t> body(z.begin() + 2, z.end());
    Bits b{body};
    for (;;) {
        const std::uint32_t last = b.get(1);
        const std::uint32_t type = b.get(2);
        if (type == 0) {
            b.align();
            if (b.pos + 4 > body.size()) {
                return false;
            }
            const std::size_t len = body[b.pos] | (static_cast<std::size_t>(body[b.pos + 1]) << 8);
            b.pos += 4;
            if (b.pos + len > body.size()) {
                return false;
            }
            out.insert(out.end(), body.begin() + static_cast<std::ptrdiff_t>(b.pos),
                       body.begin() + static_cast<std::ptrdiff_t>(b.pos + len));
            b.pos += len;
        } else if (type == 1) {
            std::vector<std::uint8_t> l(288);
            for (std::size_t i = 0; i < 288; ++i) {
                l[i] = static_cast<std::uint8_t>(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
            }
            Huffman lit, dist;
            lit.build(l);
            dist.build(std::vector<std::uint8_t>(30, 5));
            if (!codes(b, out, lit, dist)) {
                return false;
            }
        } else if (type == 2) {
            const std::uint32_t hlit = b.get(5) + 257, hdist = b.get(5) + 1, hclen = b.get(4) + 4;
            static constexpr std::array<std::uint8_t, 19> ORDER = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                                   11, 4,  12, 3, 13, 2, 14, 1, 15};
            std::vector<std::uint8_t> cl(19, 0);
            for (std::uint32_t i = 0; i < hclen; ++i) {
                cl[ORDER[i]] = static_cast<std::uint8_t>(b.get(3));
            }
            Huffman h;
            h.build(cl);
            std::vector<std::uint8_t> lengths;
            while (lengths.size() < hlit + hdist) {
                const int s = h.decode(b);
                if (s < 0) {
                    return false;
                }
                if (s < 16) {
                    lengths.push_back(static_cast<std::uint8_t>(s));
                } else {
                    std::uint8_t v = 0;
                    std::uint32_t rep = 0;
                    if (s == 16) {
                        if (lengths.empty()) {
                            return false;
                        }
                        v = lengths.back();
                        rep = 3 + b.get(2);
                    } else if (s == 17) {
                        rep = 3 + b.get(3);
                    } else {
                        rep = 11 + b.get(7);
                    }
                    lengths.insert(lengths.end(), rep, v);
                }
            }
            Huffman lit, dist;
            lit.build(std::vector<std::uint8_t>(lengths.begin(), lengths.begin() + hlit));
            dist.build(std::vector<std::uint8_t>(lengths.begin() + hlit, lengths.begin() + hlit + hdist));
            if (!codes(b, out, lit, dist)) {
                return false;
            }
        } else {
            return false;
        }
        if (b.bad) {
            return false;
        }
        if (last != 0U) {
            return true;
        }
    }
}

inline std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t at) {
    return (std::uint32_t{b[at]} << 24) | (std::uint32_t{b[at + 1]} << 16) | (std::uint32_t{b[at + 2]} << 8) |
           std::uint32_t{b[at + 3]};
}

inline int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

}  // namespace detail

inline Image decode(const std::vector<std::uint8_t>& png) {
    Image img;
    if (png.size() < 8 + 25) {
        return img;
    }
    std::size_t at = 8;
    std::vector<std::uint8_t> z;
    while (at + 8 <= png.size()) {
        const std::uint32_t len = detail::be32(png, at);
        const std::string type(png.begin() + static_cast<std::ptrdiff_t>(at + 4),
                               png.begin() + static_cast<std::ptrdiff_t>(at + 8));
        if (at + 12 + len > png.size()) {
            return img;
        }
        const std::size_t data = at + 8;
        if (type == "IHDR") {
            img.width = detail::be32(png, data);
            img.height = detail::be32(png, data + 4);
            if (png[data + 8] != 8 || png[data + 9] != 2 || png[data + 12] != 0) {
                return img;
            }
        } else if (type == "IDAT") {
            z.insert(z.end(), png.begin() + static_cast<std::ptrdiff_t>(data),
                     png.begin() + static_cast<std::ptrdiff_t>(data + len));
        } else if (type == "IEND") {
            break;
        }
        at += 12 + len;
    }
    std::vector<std::uint8_t> raw;
    if (!detail::inflate(z, raw)) {
        return img;
    }
    const std::size_t stride = static_cast<std::size_t>(img.width) * 3U;
    if (raw.size() != (stride + 1U) * img.height) {
        return img;
    }
    img.rgb.assign(stride * img.height, 0);
    for (std::size_t y = 0; y < img.height; ++y) {
        const std::uint8_t f = raw[y * (stride + 1)];
        for (std::size_t x = 0; x < stride; ++x) {
            const int v = raw[y * (stride + 1) + 1 + x];
            const int a = x >= 3 ? img.rgb[y * stride + x - 3] : 0;
            const int b = y > 0 ? img.rgb[(y - 1) * stride + x] : 0;
            const int c = (x >= 3 && y > 0) ? img.rgb[(y - 1) * stride + x - 3] : 0;
            int out = v;
            switch (f) {
                case 0: break;
                case 1: out = v + a; break;
                case 2: out = v + b; break;
                case 3: out = v + (a + b) / 2; break;
                case 4: out = v + detail::paeth(a, b, c); break;
                default: return Image{};
            }
            img.rgb[y * stride + x] = static_cast<std::uint8_t>(out & 0xFF);
        }
    }
    img.ok = true;
    return img;
}

}  // namespace pngtest
