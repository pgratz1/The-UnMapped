// World database determinism tests, spec P0-02 sections 10, 11 and 12.3,
// and the golden-hash cases of 12.4.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/db/db.hpp"
#include "worldgen/db/testing.hpp"

namespace {

namespace fs = std::filesystem;
using worldgen::db::Codec;
using worldgen::db::LayerGrid;
using worldgen::db::Overwrite;
using worldgen::db::WorldWriter;
using worldgen::db::testing::MetaOrder;
using worldgen::db::testing::WriterAccess;
using Bytes = std::vector<std::uint8_t>;

// --- A fixed set of layers ---------------------------------------------------

struct LayerSpec {
    std::int64_t id;
    const char* name;
    LayerGrid grid;
    Codec codec;
};

std::vector<LayerSpec> fixed_layers(std::uint64_t seed) {
    std::vector<LayerSpec> layers;
    {
        LayerGrid g;
        g.width = 64;
        g.height = 48;
        std::vector<std::uint8_t> v(64U * 48U);
        for (std::size_t i = 0; i < v.size(); ++i) {
            v[i] = static_cast<std::uint8_t>((i % 64U + 2U * (i / 64U)) % 256U);
        }
        g.values = std::move(v);
        layers.push_back({1, "test.ramp", std::move(g), Codec::delta_varint});
    }
    {
        // Large enough to need overflow pages.
        worldgen::Rng rng(seed ^ 0xA5A5A5A5ULL);
        LayerGrid g;
        g.width = 128;
        g.height = 96;
        std::vector<std::int32_t> v(128U * 96U);
        for (auto& e : v) {
            const std::uint64_t r = rng.next();
            e = static_cast<std::int32_t>(static_cast<std::uint32_t>(r >> 32U));
        }
        g.values = std::move(v);
        layers.push_back({2, "test.noise", std::move(g), Codec::delta_varint});
    }
    {
        LayerGrid g;
        g.width = 7;
        g.height = 3;
        g.values = std::vector<std::uint16_t>(21, 0x1234);
        layers.push_back({3, "biome", std::move(g), Codec::raw});
    }
    return layers;
}

// How the rows reach the working database. The file must not depend on it
// (P0-02 6, 11, 12.3).
enum class Layout {
    normal,          // write_layer in id order, meta ascending
    reverse,         // layers in reverse id order, meta descending
    insert_delete,   // extra rows inserted and deleted, rows updated and restored
    rolled_back,     // an abandoned stage before the real one
};

void write_world(const std::string& out, std::uint64_t seed, bool with_layers, Layout layout = Layout::normal) {
    WorldWriter writer;
    dbtest::set_standard_meta(writer, seed);
    const auto layers = fixed_layers(seed);
    if (layout == Layout::rolled_back) {
        // Rows written by raw SQL inside a stage that is then abandoned. (A
        // rolled-back write_layer is covered by the writer test of 14.6.)
        auto abandoned = writer.begin_stage("test_pattern");
        REQUIRE(WriterAccess::execute(writer, "INSERT INTO layer VALUES (1, 'junk', 1, 1, 'u8', zeroblob(40000))")
                    .ok());
        REQUIRE(WriterAccess::execute(writer, "INSERT INTO meta VALUES ('zz.junk', 'temporary')").ok());
    }
    if (with_layers) {
        auto stage = writer.begin_stage("test_pattern");
        switch (layout) {
            case Layout::normal:
            case Layout::rolled_back:
                for (const auto& l : layers) {
                    stage.write_layer(l.id, l.name, l.grid, l.codec);
                }
                break;
            case Layout::reverse:
                for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
                    WriterAccess::write_layer_any_id(stage, it->id, it->name, it->grid, it->codec);
                }
                break;
            case Layout::insert_delete: {
                REQUIRE(WriterAccess::execute(writer, "INSERT INTO layer VALUES (100, 'junk.a', 1, 1, 'u8', "
                                                      "zeroblob(30000))")
                            .ok());
                REQUIRE(WriterAccess::execute(writer, "INSERT INTO meta VALUES ('zz.junk', 'temporary')").ok());
                for (const auto& l : layers) {
                    stage.write_layer(l.id, l.name, l.grid, l.codec);
                }
                REQUIRE(WriterAccess::execute(writer, "INSERT INTO layer VALUES (101, 'junk.b', 1, 1, 'u8', "
                                                      "randomblob(20000))")
                            .ok());
                REQUIRE(WriterAccess::execute(writer, "UPDATE layer SET name = 'renamed' WHERE id = 1").ok());
                REQUIRE(WriterAccess::execute(writer, "UPDATE layer SET name = 'test.ramp' WHERE id = 1").ok());
                REQUIRE(WriterAccess::execute(writer, "DELETE FROM layer WHERE id >= 100").ok());
                REQUIRE(WriterAccess::execute(writer, "DELETE FROM meta WHERE key = 'zz.junk'").ok());
                break;
            }
        }
        stage.commit();
    } else if (layout == Layout::insert_delete) {
        REQUIRE(WriterAccess::execute(writer, "INSERT INTO layer VALUES (100, 'junk.a', 1, 1, 'u8', zeroblob(30000))")
                    .ok());
        REQUIRE(WriterAccess::execute(writer, "DELETE FROM layer WHERE id = 100").ok());
    }
    if (layout == Layout::reverse) {
        WriterAccess::set_meta_order(writer, MetaOrder::descending);
    }
    const auto status = writer.finalize(out, Overwrite::refuse);
    REQUIRE_MESSAGE(status.ok(), (status.ok() ? std::string() : status.error().message));
}

// --- Environment variables, portably ------------------------------------------

std::optional<std::string> get_env(const char* name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

// Sets (or, for nullopt, removes) an environment variable of this process.
// POSIX setenv/unsetenv; MSVC _putenv_s, where an empty value removes the
// variable.
void set_env(const char* name, const std::optional<std::string>& value) {
#if defined(_WIN32)
    (void)_putenv_s(name, value ? value->c_str() : "");
#else
    if (value) {
        (void)setenv(name, value->c_str(), 1);
    } else {
        (void)unsetenv(name);
    }
#endif
}

class EnvOverride {
public:
    EnvOverride(const char* name, const char* value) : name_(name), saved_(get_env(name)) {
        set_env(name, std::string(value));
    }
    ~EnvOverride() { set_env(name_, saved_); }
    EnvOverride(const EnvOverride&) = delete;
    EnvOverride& operator=(const EnvOverride&) = delete;

private:
    const char* name_;
    std::optional<std::string> saved_;
};

}  // namespace

TEST_SUITE("db.determinism") {

TEST_CASE("same inputs twice give identical bytes (12.3)") {
    dbtest::TempDir dir("det-twice");
    for (const bool with_layers : {false, true}) {
        CAPTURE(with_layers);
        const std::string a = dir.file(with_layers ? "a-layers.db" : "a.db");
        const std::string b = dir.file(with_layers ? "b-layers.db" : "b.db");
        write_world(a, 42, with_layers);
        write_world(b, 42, with_layers);
        const Bytes ba = dbtest::read_file(a);
        REQUIRE_FALSE(ba.empty());
        CHECK(ba == dbtest::read_file(b));
    }
    // Different inputs give different files (the comparison is not vacuous).
    write_world(dir.file("seed43.db"), 43, false);
    CHECK(dbtest::read_file(dir.file("seed43.db")) != dbtest::read_file(dir.file("a.db")));
}

TEST_CASE("different output paths and current directories give identical bytes (10, 12.3)") {
    dbtest::TempDir dir("det-paths");
    fs::create_directories(dir.path() / "one");
    fs::create_directories(dir.path() / "two" / "deeper");
    const std::string reference = dir.file("reference.db");
    write_world(reference, 7, true);
    const Bytes expected = dbtest::read_file(reference);
    REQUIRE_FALSE(expected.empty());

    // Absolute paths with different directories and names, one non-ASCII.
    const std::string other = dir.file("one/other-name.db");
    write_world(other, 7, true);
    CHECK(dbtest::read_file(other) == expected);
    const std::string unicode = dir.file("two/\xC3\xA9tendue-\xD0\xBC\xD0\xB8\xD1\x80.db");  // "étendue-мир.db"
    write_world(unicode, 7, true);
    CHECK(dbtest::read_file(unicode) == expected);

    // Relative paths resolved against two different current directories.
    {
        dbtest::CurrentDir cd(dir.path() / "one");
        write_world("relative.db", 7, true);
    }
    CHECK(dbtest::read_file(dir.file("one/relative.db")) == expected);
    {
        dbtest::CurrentDir cd(dir.path() / "two" / "deeper");
        write_world("../relative-up.db", 7, true);
    }
    CHECK(dbtest::read_file(dir.file("two/relative-up.db")) == expected);

    // Nothing else was left anywhere.
    CHECK(dbtest::dir_entries(dir.path() / "one") == std::set<std::string>{"other-name.db", "relative.db"});
    CHECK(dbtest::dir_entries(dir.path() / "two") ==
          std::set<std::string>{"deeper", "relative-up.db", "\xC3\xA9tendue-\xD0\xBC\xD0\xB8\xD1\x80.db"});
    CHECK(dbtest::dir_entries(dir.path() / "two" / "deeper").empty());
}

TEST_CASE("changing TZ, LANG and LC_ALL does not change the bytes (10, 12.3)") {
    // The variables are changed in this process with setenv (POSIX) or
    // _putenv_s (Windows) and restored afterwards. No setlocale call is made:
    // the generator must not consult the locale or the time zone at all.
    dbtest::TempDir dir("det-env");
    const std::string reference = dir.file("reference.db");
    write_world(reference, 1234, true);
    const Bytes expected = dbtest::read_file(reference);
    REQUIRE_FALSE(expected.empty());

    struct Env {
        const char* tz;
        const char* lang;
    };
    const Env envs[] = {
        {"UTC", "C"},
        {"Asia/Kolkata", "de_DE.UTF-8"},
        {"America/St_Johns", "tr_TR.UTF-8"},
    };
    int index = 0;
    for (const Env& e : envs) {
        CAPTURE(e.tz);
        CAPTURE(e.lang);
        const std::string out = dir.file("env" + std::to_string(index++) + ".db");
        {
            EnvOverride tz("TZ", e.tz);
            EnvOverride lang("LANG", e.lang);
            EnvOverride lc_all("LC_ALL", e.lang);
            CHECK(get_env("TZ") == std::optional<std::string>(e.tz));
            write_world(out, 1234, true);
        }
        CHECK(dbtest::read_file(out) == expected);
    }
}

TEST_CASE("layout independence: reverse order, insert-then-delete, rolled-back stage (6, 11, 12.3)") {
    dbtest::TempDir dir("det-layout");
    for (const bool with_layers : {false, true}) {
        CAPTURE(with_layers);
        const std::string prefix = with_layers ? "layers-" : "empty-";
        const std::string normal = dir.file(prefix + "normal.db");
        write_world(normal, 99, with_layers, Layout::normal);
        const Bytes expected = dbtest::read_file(normal);
        REQUIRE_FALSE(expected.empty());

        SUBCASE("reverse id order and descending meta order") {
            const std::string out = dir.file(prefix + "reverse.db");
            write_world(out, 99, with_layers, Layout::reverse);
            CHECK(dbtest::read_file(out) == expected);
        }
        SUBCASE("extra rows inserted and deleted, rows updated and restored") {
            const std::string out = dir.file(prefix + "insdel.db");
            write_world(out, 99, with_layers, Layout::insert_delete);
            CHECK(dbtest::read_file(out) == expected);
        }
        SUBCASE("an abandoned stage before the real one") {
            const std::string out = dir.file(prefix + "rollback.db");
            write_world(out, 99, with_layers, Layout::rolled_back);
            CHECK(dbtest::read_file(out) == expected);
        }
    }
}

// --- Golden hashes (P0-02 12.4, P0-04 9) ------------------------------------
//
// SHA-256 of the file bytes of the empty world with the content hash of no
// data, as `worldgen-cli generate` wrote it until the `worldgen` kind was
// registered (re-recorded for generator 0.13.0, schema version 6; since then the CLI writes the content hash of
// the loaded data, P0-03 amendment 17, so the CLI goldens in p0-worlds.txt
// differ from these). The case names are those of P0-04 section 9. The values are
// recorded by the validation specialist: until then each is the placeholder
// below, and the comparison test is SKIPPED (doctest reports it as skipped),
// never silently passed. The test-pattern cases belong to P0-04.

namespace {

constexpr std::string_view GOLDEN_PLACEHOLDER = "PLACEHOLDER-TO-BE-RECORDED-BY-VALIDATION-SPECIALIST";

struct GoldenCase {
    const char* name;
    std::uint64_t seed;
    const char* phrase;  // the phrase as typed (P0-04 9); nullptr for a numeric seed
    std::string_view sha256;
};

constexpr std::array<GoldenCase, 5> GOLDEN_CASES = {{
    {"empty-phrase", 0, "The Unmapped", "d9af72602f6c3a0d843f137ad1f54408a6d7433d88b4f9faf14e7902a2a9d788"},
    {"empty-seed-0", 0, nullptr, "c5326b79c220b1e615ff334022a783113bf92ae45b7d052dcc49e0870074fed2"},
    {"empty-seed-1", 1, nullptr, "6bafbea54324e1160bc849fcced4f3a3812ac6f762977ab79d9b8c3b2d9887ae"},
    {"empty-seed-42", 42, nullptr, "e54a061a15bafdb461f2b2fcc175615d746c66984ed28626298946a94f4c7b89"},
    {"empty-seed-max", 18446744073709551615ULL, nullptr, "31a5de190f791fbef0dd8b1a757ad2c096da96793e8534ea81a4361910062a38"},
}};

constexpr bool golden_pending() {
    for (const auto& c : GOLDEN_CASES) {
        if (c.sha256 == GOLDEN_PLACEHOLDER) {
            return true;
        }
    }
    return false;
}

// The empty world as `worldgen-cli generate --until none` writes it (P0-02
// 8), but with the content hash of no data: map size 1024, no stages.
std::string golden_world_hash(const dbtest::TempDir& dir, const GoldenCase& c) {
    const std::string out = dir.file(std::string(c.name) + ".db");
    WorldWriter writer;
    if (c.phrase != nullptr) {
        // As the CLI does: the writer takes the scrubbed phrase (P0-02
        // amendment 12), so "The Unmapped" is stored as "the unmapped".
        const worldgen::PhraseSeed ps = worldgen::seed_from_phrase(c.phrase);
        REQUIRE(ps.ok());
        CHECK(ps.phrase == "the unmapped");
        writer.set_seed_phrase(ps.phrase);
    } else {
        writer.set_seed(c.seed);
    }
    writer.set_content_hash(dbtest::empty_content_hash());
    writer.set_param_integer("map_size", 1024);
    dbtest::set_history_defaults(writer);
    const auto status = writer.finalize(out, Overwrite::refuse);
    REQUIRE(status.ok());
    return dbtest::file_sha256(out);
}

}  // namespace

TEST_CASE("golden: empty-world hashes match the recorded values (12.4)" * doctest::skip(golden_pending())) {
    dbtest::TempDir dir("golden-compare");
    for (const auto& c : GOLDEN_CASES) {
        CAPTURE(c.name);
        REQUIRE_MESSAGE(c.sha256 != GOLDEN_PLACEHOLDER, "golden hash not recorded yet");
        CHECK(golden_world_hash(dir, c) == std::string(c.sha256));
    }
}

TEST_CASE("golden: observed empty-world hashes (informational; comparison pending while placeholders remain)") {
    dbtest::TempDir dir("golden-observe");
    for (const auto& c : GOLDEN_CASES) {
        const std::string hash = golden_world_hash(dir, c);
        CHECK(hash.size() == 64);
        if (golden_pending()) {
            const std::string line = "golden pending: " + std::string(c.name) + " " + hash;
            MESSAGE(line);
        }
    }
    if (golden_pending()) {
        WARN_MESSAGE(false, "P0-02 12.4 golden hashes are placeholders: the comparison test is skipped");
    }
}

}  // TEST_SUITE
