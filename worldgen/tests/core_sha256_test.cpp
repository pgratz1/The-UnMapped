// Spec P0-01 section 13.7: SHA-256 (FIPS 180-4) vectors and incremental use.

#include <string>
#include <vector>

#include "core_test_util.hpp"

using namespace worldgen;

TEST_CASE("P0-01 13.7: FIPS 180-4 vectors") {
    CHECK(sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(to_hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::vector<std::uint8_t> abc = {0x61, 0x62, 0x63};
    CHECK(sha256_hex(std::span<const std::uint8_t>(abc)) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256(std::span<const std::uint8_t>(abc)) == sha256("abc"));
    // Digest bytes are in standard (big-endian) order.
    const Sha256Digest d = sha256("abc");
    CHECK(d[0] == 0xba);
    CHECK(d[31] == 0xad);
}

TEST_CASE("P0-01 13.7: one million 'a'") {
    const std::string expected = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    // Incremental, in 1000 pieces of 1000 bytes.
    const std::string chunk(1000, 'a');
    Sha256 h;
    for (int i = 0; i < 1000; ++i) {
        h.update(chunk);
    }
    CHECK(to_hex(h.final()) == expected);
    // One-shot.
    CHECK(sha256_hex(std::string(1000000, 'a')) == expected);
}

TEST_CASE("P0-01 13.7: incremental split at every offset 0..64 of a 130-byte message") {
    std::string msg;
    for (int i = 0; i < 130; ++i) {
        msg.push_back(static_cast<char>(static_cast<unsigned char>((i * 37 + 11) & 0xFF)));
    }
    const Sha256Digest whole = sha256(msg);
    const std::string_view view(msg);
    for (std::size_t split = 0; split <= 64; ++split) {
        Sha256 h;
        h.update(view.substr(0, split));
        h.update(view.substr(split));
        INFO("split at " << split);
        CHECK(h.final() == whole);
        // Three pieces: split, then the rest in two at a second offset.
        Sha256 h3;
        h3.update(view.substr(0, split));
        h3.update(view.substr(split, 65 - split));
        h3.update(view.substr(65));
        CHECK(h3.final() == whole);
    }
    // Byte by byte.
    Sha256 b;
    for (const char c : msg) {
        b.update(std::string_view(&c, 1));
    }
    CHECK(b.final() == whole);
}

TEST_CASE("P0-01 10: final() resets the object for reuse") {
    Sha256 h;
    h.update("abc");
    CHECK(to_hex(h.final()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(to_hex(h.final()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    h.update("ab");
    h.update("c");
    CHECK(to_hex(h.final()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
