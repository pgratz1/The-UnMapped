// P0-03 TOML loader: order independence, canonical encoding and content
// hash, determinism and golden fixture hashes (spec P0-03 sections 7.4, 8,
// 11, 13 tests 4 and 8 and the determinism tests).
//
// The expected byte streams are written out here by hand from section 8,
// independently of the loader's canonical_encoding, and compared both as
// bytes and through SHA-256 (P0-01 section 10).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "data_loader_support.hpp"
#include "worldgen/core/sha256.hpp"

using namespace loader_test;

namespace {

// The byte stream of section 8, built by hand.
class Stream {
public:
    Stream& u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            bytes_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
        }
        return *this;
    }
    Stream& i64(std::int64_t v) { return u64(static_cast<std::uint64_t>(v)); }
    Stream& byte(std::uint8_t b) {
        bytes_.push_back(b);
        return *this;
    }
    Stream& str(std::string_view s) {
        u64(static_cast<std::uint64_t>(s.size()));
        for (char c : s) {
            bytes_.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
        }
        return *this;
    }
    // Field helpers: str(name) value(...).
    Stream& f_int(std::string_view name, std::int64_t v) { return str(name).byte(0x01).i64(v); }
    Stream& f_fx(std::string_view name, std::int64_t raw) { return str(name).byte(0x02).i64(raw); }
    Stream& f_bool(std::string_view name, bool v) {
        return str(name).byte(0x03).byte(v ? std::uint8_t{0x01} : std::uint8_t{0x00});
    }
    Stream& f_str(std::string_view name, std::string_view v) { return str(name).byte(0x04).str(v); }
    Stream& f_str_array(std::string_view name, const std::vector<std::string>& v) {
        str(name).byte(0x05).u64(static_cast<std::uint64_t>(v.size()));
        for (const auto& s : v) {
            byte(0x04).str(s);
        }
        return *this;
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] worldgen::Sha256Digest sha() const { return worldgen::sha256(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

constexpr std::int64_t HALF_RAW = 2147483648;     // "0.5"
constexpr std::int64_t QUARTER_RAW = 1073741824;  // "0.25"

// An alpha record in declaration order.
void alpha_record(Stream& s, std::int64_t count, std::int64_t ratio_raw, bool enabled, std::string_view label,
                  std::string_view mode, const std::vector<std::string>& tags, std::string_view partner) {
    s.f_int("count", count)
        .f_fx("ratio", ratio_raw)
        .f_bool("enabled", enabled)
        .f_str("label", label)
        .f_str("mode", mode)
        .f_str_array("tags", tags)
        .f_str("partner", partner);
}

// Section 13 test 8: alpha a1 {count = 3, partner = "b1", rest defaulted},
// beta b1 {weight = 7}.
Stream canonical_stream() {
    Stream s;
    s.str("alpha").u64(1).str("a1");
    alpha_record(s, 3, HALF_RAW, true, "", "fast", {}, "b1");
    s.str("beta").u64(1).str("b1").f_int("weight", 7);
    return s;
}

// The split_xy / split_yx fixtures.
Stream split_stream() {
    Stream s;
    s.str("alpha").u64(2);
    s.str("x");
    alpha_record(s, 10, HALF_RAW, true, "the x", "fast", {"one", "two"}, "b1");
    s.str("y");
    alpha_record(s, 20, QUARTER_RAW, false, "", "slow", {}, "b2");
    s.str("beta").u64(2);
    s.str("b1").f_int("weight", 7);
    s.str("b2").f_int("weight", 1000);
    return s;
}

wd::ContentSet load_fixture(std::string_view name) {
    const auto r = wd::load_content(alpha_beta_registry(), fixture(name));
    check_content_result(r);
    INFO(std::string(name) << "\n" << error_dump(r));
    REQUIRE(r.ok());
    return *r.value;
}

wd::ContentSet load_ok(const wd::KindRegistry& reg, const wd::MemoryDataRoot& root) {
    const auto r = wd::load_content_from_memory(reg, root);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    return *r.value;
}

// Checks the loader's encoding and hash against a hand-built stream.
void check_against(const wd::ContentSet& content, const Stream& expected) {
    CHECK(wd::canonical_encoding(content) == expected.bytes());
    CHECK(wd::content_hash(content) == expected.sha());
    CHECK(wd::content_hash_hex(content) == worldgen::to_hex(expected.sha()));
}

constexpr std::string_view EMPTY_SHA256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

}  // namespace

// --- Test 4: order independence (7.4) -----------------------------------------

TEST_CASE("loader order independence: splitting and reordering never changes the content") {
    // Fixtures: {x in a.toml, y in b.toml} and {y in a.toml, x in b.toml},
    // keys in another order, betas reordered, 10 written as 0xA, "0.25" as
    // "0.250".
    const auto xy = load_fixture("split_xy");
    const auto yx = load_fixture("split_yx");
    CHECK(xy == yx);
    CHECK(wd::content_hash(xy) == wd::content_hash(yx));
    check_against(xy, split_stream());
    check_against(yx, split_stream());

    // Definitions come out in id order regardless of file order.
    REQUIRE(xy.kinds.size() == 2);
    REQUIRE(xy.kinds[0].definitions.size() == 2);
    CHECK(xy.kinds[0].definitions[0].id == "x");
    CHECK(xy.kinds[0].definitions[1].id == "y");
    REQUIRE(yx.kinds[0].definitions.size() == 2);
    CHECK(yx.kinds[0].definitions[0].id == "x");
    CHECK(yx.kinds[1].definitions[0].id == "b1");
}

TEST_CASE("loader order independence: in memory, ids out of order within and across files") {
    const auto reg = alpha_beta_registry();
    const std::string zeta = "[alpha.zeta]\ncount = 1\npartner = \"b1\"\n";
    const std::string able = "[alpha.able]\ncount = 2\npartner = \"b1\"\n";
    const std::string mid = "[alpha.mid]\ncount = 3\npartner = \"b1\"\nlabel = \"m\"\n";
    const std::string mid_reordered = "[alpha.mid]\nlabel = \"m\"\npartner = \"b1\"\ncount = 3\n";

    const auto one_file = load_ok(reg, alphas_root({file("a.toml", zeta + able + mid)}));
    const auto three_files = load_ok(reg, alphas_root({file("c.toml", able), file("a.toml", mid_reordered),
                                                       file("b.toml", zeta)}));
    const auto swapped = load_ok(reg, alphas_root({file("z.toml", zeta + mid), file("y.toml", able)}));
    CHECK(one_file == three_files);
    CHECK(one_file == swapped);
    CHECK(wd::content_hash(one_file) == wd::content_hash(three_files));
    CHECK(wd::content_hash(one_file) == wd::content_hash(swapped));

    const auto* k = one_file.find_kind("alpha");
    REQUIRE(k != nullptr);
    REQUIRE(k->definitions.size() == 3);
    CHECK(k->definitions[0].id == "able");
    CHECK(k->definitions[1].id == "mid");
    CHECK(k->definitions[2].id == "zeta");
    CHECK(k->find("mid") != nullptr);
    CHECK(k->find("nope") == nullptr);
    CHECK(one_file.find("alpha", "zeta")->record.get_int("count") == 1);

    // Ids sort by bytes: "a_b" (0x5F) after "a9" (0x39) and before "ab".
    const auto bytes = load_ok(reg, alphas_root({file("a.toml", "[alpha.ab]\ncount = 1\npartner = \"b1\"\n"
                                                                "[alpha.a_b]\ncount = 1\npartner = \"b1\"\n"
                                                                "[alpha.a9]\ncount = 1\npartner = \"b1\"\n"
                                                                "[alpha.a]\ncount = 1\npartner = \"b1\"\n")}));
    const auto& defs = bytes.find_kind("alpha")->definitions;
    REQUIRE(defs.size() == 4);
    CHECK(defs[0].id == "a");
    CHECK(defs[1].id == "a9");
    CHECK(defs[2].id == "a_b");
    CHECK(defs[3].id == "ab");
}

// --- Test 8: canonical encoding (section 8) -----------------------------------

TEST_CASE("loader canonical encoding: hand-built stream for the section 13 example") {
    const Stream expected = canonical_stream();
    // Fixture on disk, and the same from memory.
    const auto disk = load_fixture("canonical");
    check_against(disk, expected);
    const auto mem = load_ok(alpha_beta_registry(), alphas_root({file("a.toml", "[alpha.a1]\ncount = 3\npartner = \"b1\"\n")}));
    check_against(mem, expected);
    CHECK(disk == mem);
    // Hand-computed from the stream above with an independent SHA-256
    // (Python hashlib), recorded when the test was written.
    CHECK(worldgen::to_hex(expected.sha()) == "a6db197309cab5080de27c8ef8a446ea4d7aff1c63b36a50afe2dff066ac8586");
}

TEST_CASE("loader canonical encoding: the empty registry gives the empty stream") {
    const wd::KindRegistry empty;
    const auto r = wd::load_content_from_memory(empty, wd::MemoryDataRoot{});
    REQUIRE(r.ok());
    CHECK(wd::canonical_encoding(*r.value).empty());
    CHECK(wd::content_hash_hex(*r.value) == EMPTY_SHA256);
    CHECK(wd::content_hash_hex(wd::ContentSet{}) == EMPTY_SHA256);
    const auto b = wd::load_content_without_data(wd::builtin_kinds());
    REQUIRE(b.ok());
    CHECK(wd::content_hash_hex(*b.value) == EMPTY_SHA256);
    CHECK(worldgen::to_hex(wd::content_hash(*b.value)) == EMPTY_SHA256);
}

TEST_CASE("loader canonical encoding: comments and formatting do not change the hash; values do") {
    const auto canonical = load_fixture("canonical");
    const auto reformatted = load_fixture("canonical_reformatted");
    const auto count4 = load_fixture("canonical_count4");
    CHECK(canonical == reformatted);
    CHECK(wd::content_hash(canonical) == wd::content_hash(reformatted));
    CHECK_FALSE(canonical == count4);
    CHECK(wd::content_hash(canonical) != wd::content_hash(count4));

    Stream s4;
    s4.str("alpha").u64(1).str("a1");
    alpha_record(s4, 4, HALF_RAW, true, "", "fast", {}, "b1");
    s4.str("beta").u64(1).str("b1").f_int("weight", 7);
    check_against(count4, s4);
}

TEST_CASE("loader canonical encoding: equal validated values hash equal") {
    const auto reg = alpha_beta_registry();
    auto load_x = [&reg](std::string_view extra) {
        return load_ok(reg, alphas_root({file("a.toml", "[alpha.x]\ncount = 1\npartner = \"b1\"\n" +
                                                            std::string(extra))}));
    };
    // fx written as 1, "1" and "1.0".
    const auto one_int = load_x("ratio = 1\n");
    CHECK(one_int == load_x("ratio = \"1\"\n"));
    CHECK(wd::content_hash(one_int) == wd::content_hash(load_x("ratio = \"1.0\"\n")));
    // Defaults written out equal defaults applied.
    const auto defaulted = load_x("");
    const auto explicit_defaults =
        load_x("ratio = \"0.5\"\nenabled = true\nlabel = \"\"\nmode = \"fast\"\ntags = []\n");
    CHECK(defaulted == explicit_defaults);
    CHECK(wd::content_hash(defaulted) == wd::content_hash(explicit_defaults));
    // Any changed value changes the hash.
    const auto base = wd::content_hash(defaulted);
    for (const char* change : {"ratio = \"0.500000001\"\n"  /* 1e-9 is 4 raw units */, "enabled = false\n", "label = \" \"\n",
                               "mode = \"slow\"\n", "tags = [\"\"]\n"}) {
        INFO(std::string(change));
        CHECK(wd::content_hash(load_x(change)) != base);
    }
}

TEST_CASE("loader canonical encoding: arrays, booleans, records and negative values by hand") {
    // A kind with every value tag: int (negative), fx (negative), bool,
    // string, enum, array of arrays, nested record.
    std::vector<wd::Field> inner;
    inner.push_back(wd::Field::required("n", wd::Type::integer(-5, 5)));
    inner.push_back(wd::Field::optional("s", wd::Type::string(8), wd::Value::string("d")));
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("i", wd::Type::integer(-1000, 1000)));
    fields.push_back(wd::Field::required("f", wd::Type::fixed("-2", "2")));
    fields.push_back(wd::Field::required("b", wd::Type::boolean()));
    fields.push_back(wd::Field::required("e", wd::Type::enumeration({"up", "down"})));
    fields.push_back(wd::Field::required("aa", wd::Type::array(wd::Type::array(wd::Type::integer(0, 9), 0, 3), 0, 3)));
    fields.push_back(wd::Field::required("r", wd::Type::record(wd::Schema(std::move(inner)))));
    wd::KindRegistry reg;
    reg.add(wd::Kind{"gamma", "gammas", true, wd::Schema(std::move(fields))});

    wd::MemoryDataRoot root;
    root.directories.push_back(
        dir("gammas", {file("g.toml", "[gamma.g]\nr = { n = -5 }\naa = [[1, 2], []]\ne = \"down\"\nb = false\n"
                                      "f = \"-1.5\"\ni = -1000\n")}));
    const auto content = load_ok(reg, root);

    Stream s;
    s.str("gamma").u64(1).str("g");
    s.f_int("i", -1000);
    s.f_fx("f", -6442450944);  // -1.5 * 2^32
    s.f_bool("b", false);
    s.f_str("e", "down");
    s.str("aa").byte(0x05).u64(2);
    s.byte(0x05).u64(2).byte(0x01).i64(1).byte(0x01).i64(2);
    s.byte(0x05).u64(0);
    s.str("r").byte(0x06).f_int("n", -5).f_str("s", "d");
    check_against(content, s);
}

TEST_CASE("loader canonical encoding: a registered kind with no files adds its name and a zero count") {
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("n", wd::Type::integer(0, 1)));
    wd::KindRegistry with_solo = alpha_beta_registry();
    with_solo.add(wd::Kind{"solo", "solos", false, wd::Schema(std::move(fields))});

    const auto root = alphas_root({file("a.toml", "[alpha.a1]\ncount = 3\npartner = \"b1\"\n")});
    const auto plain = load_ok(alpha_beta_registry(), root);
    const auto solo = load_ok(with_solo, root);
    CHECK(wd::content_hash(plain) != wd::content_hash(solo));

    Stream s = canonical_stream();
    s.str("solo").u64(0);
    check_against(solo, s);

    // With no data root at all.
    wd::KindRegistry only_solo;
    std::vector<wd::Field> f2;
    f2.push_back(wd::Field::required("n", wd::Type::integer(0, 1)));
    only_solo.add(wd::Kind{"solo", "solos", false, wd::Schema(std::move(f2))});
    const auto none = wd::load_content_without_data(only_solo);
    REQUIRE(none.ok());
    Stream s2;
    s2.str("solo").u64(0);
    check_against(*none.value, s2);
}

// --- Determinism (section 13, section 11) -------------------------------------

TEST_CASE("loader determinism: repeated loads, disk and memory give identical bytes") {
    for (const char* name : {"canonical", "canonical_reformatted", "split_xy", "split_yx", "discovery"}) {
        INFO(std::string(name));
        const auto a = load_fixture(name);
        const auto b = load_fixture(name);
        CHECK(a == b);
        CHECK(wd::canonical_encoding(a) == wd::canonical_encoding(b));
        CHECK(wd::content_hash_hex(a) == wd::content_hash_hex(b));
        const auto hex = wd::content_hash_hex(a);
        CHECK(hex.size() == 64);
        CHECK(hex.find_first_not_of("0123456789abcdef") == std::string::npos);
    }
}

// --- Golden fixture hashes (section 13 determinism tests) -------------------
//
// Recorded once by the validation specialist and compared on every
// platform. Until then each value is the placeholder below, and the test
// case is skipped (doctest reports it as skipped, it does not pass). Run
// worldgen-tests with --no-skip to see the observed values as failures.

namespace {

constexpr std::string_view GOLDEN_PLACEHOLDER = "PENDING: to be recorded by the validation specialist";

struct GoldenHash {
    const char* fixture;
    std::string_view content_hash;
};

constexpr GoldenHash GOLDEN_HASHES[] = {
    {"canonical", "a6db197309cab5080de27c8ef8a446ea4d7aff1c63b36a50afe2dff066ac8586"},
    {"canonical_reformatted", "a6db197309cab5080de27c8ef8a446ea4d7aff1c63b36a50afe2dff066ac8586"},
    {"canonical_count4", "7195a8d9c5f78fbdb80c814f2e3a745b0be33a0bbabe0dd8c788454ecc6c80b0"},
    {"split_xy", "bb0bf3c1e772b1cd9a52f03147e8eb94307e012f7b2e5b05f40d3f30b253a6ea"},
    {"split_yx", "bb0bf3c1e772b1cd9a52f03147e8eb94307e012f7b2e5b05f40d3f30b253a6ea"},
    {"discovery", "b1598fb16a884cec6000ab97bcece90d686b7f2e3d8020cea7abfcd750061ea6"},
};

constexpr bool golden_pending() {
    for (const auto& g : GOLDEN_HASHES) {
        if (g.content_hash == GOLDEN_PLACEHOLDER) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("loader golden: fixture content hashes match the recorded values" * doctest::skip(golden_pending())) {
    for (const auto& g : GOLDEN_HASHES) {
        INFO(std::string(g.fixture));
        CHECK(wd::content_hash_hex(load_fixture(g.fixture)) == g.content_hash);
    }
}
