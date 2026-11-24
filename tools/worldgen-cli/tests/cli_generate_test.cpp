// worldgen-cli generate, spec P0-04 sections 5, 10 and 12.2, with the
// amendments of section 14.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/reader.hpp"

namespace {

using namespace clitest;
using worldgen::db::Codec;
using worldgen::db::ElementType;
using worldgen::db::WorldReader;

std::uint64_t phrase_to_seed(std::string_view phrase) {
    const worldgen::PhraseSeed ps = worldgen::seed_from_phrase(phrase);
    REQUIRE(ps.ok());
    return ps.seed;
}

WorldReader open_world(const fs::path& path) {
    auto r = WorldReader::open(u8(path));
    INFO("open: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return std::move(r).value();
}

}  // namespace

TEST_CASE("cli 12.2: default output path") {
    Sandbox sb;
    SUBCASE("--seed 42") {
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "42"});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_OK);
        const fs::path expected = sb.out_dir() / "world-42.db";
        CHECK(r.out == printed_path(expected));
        CHECK(r.err.empty());
        CHECK(file_exists(expected));
        // Nothing else is written to the output directory.
        std::vector<std::string> names;
        for (const auto& e : fs::directory_iterator(sb.out_dir())) {
            names.push_back(u8(e.path().filename()));
        }
        CHECK(names == std::vector<std::string>{"world-42.db"});
    }
    SUBCASE("--seed 18446744073709551615 and hex seeds use the canonical decimal name") {
        CHECK(sb.cli({"generate", "--until", "none", "--seed", "18446744073709551615"}).out ==
              printed_path(sb.out_dir() / "world-18446744073709551615.db"));
        CHECK(sb.cli({"generate", "--until", "none", "--seed", "0xFFFF"}).out == printed_path(sb.out_dir() / "world-65535.db"));
        CHECK(sb.cli({"generate", "--until", "none", "--seed", "0x0"}).out == printed_path(sb.out_dir() / "world-0.db"));
    }
    SUBCASE("--seed-phrase \"The Unmapped\": the name uses the scrubbed phrase's seed (amendment 15)") {
        const std::uint64_t seed = worldgen::phrase_seed("the unmapped");
        CHECK(phrase_to_seed("The Unmapped") == seed);
        CHECK(seed != worldgen::phrase_seed("The Unmapped"));
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed-phrase", "The Unmapped"});
        CHECK(r.exit_code == EXIT_OK);
        const fs::path expected = sb.out_dir() / ("world-" + std::to_string(seed) + ".db");
        CHECK(r.out == printed_path(expected));
        const WorldReader w = open_world(expected);
        CHECK(w.seed() == seed);
        CHECK(w.seed_phrase() == "the unmapped");
    }
    SUBCASE("--seed-phrase \"Ashen Marches!\" and its other spellings share one default name") {
        const std::uint64_t seed = worldgen::phrase_seed("ashen marches");
        const fs::path expected = sb.out_dir() / ("world-" + std::to_string(seed) + ".db");
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed-phrase", "Ashen Marches!"});
        CHECK(r.exit_code == EXIT_OK);
        CHECK(r.out == printed_path(expected));
        // Same default path, so a second spelling meets the existing file.
        CHECK(sb.cli({"generate", "--until", "none", "--seed-phrase", "ASHEN  MARCHES"}).exit_code == EXIT_OUTPUT);
        const ProcessResult f = sb.cli({"generate", "--until", "none", "--seed-phrase", "ashen-marches", "--force"});
        CHECK(f.exit_code == EXIT_OK);
        CHECK(f.out == printed_path(expected));
    }
    SUBCASE("the default output directory is created with its parents") {
        RunOptions o;
        const fs::path deep_root = sb.file("not/yet/there");
        o.env = {{"UNMAPPED_BUILD_ROOT", u8(deep_root)}};
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "3"}, o);
        CHECK(r.exit_code == EXIT_OK);
        CHECK(r.out == printed_path(deep_root / "out" / "world-3.db"));
    }
}

TEST_CASE("cli 12.2: --force replaces an existing file, without it the old file is untouched") {
    Sandbox sb;
    const fs::path out = sb.file("w.db");
    const fs::path seed2 = sb.file("seed2.db");
    REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(out)}).exit_code == EXIT_OK);
    REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "2", "--out", u8(seed2)}).exit_code == EXIT_OK);
    const Bytes first = read_bytes(out);
    REQUIRE(read_bytes(seed2) != first);

    const ProcessResult refused = sb.cli({"generate", "--until", "none", "--seed", "2", "--out", u8(out)});
    CHECK(refused.exit_code == EXIT_OUTPUT);
    CHECK(read_bytes(out) == first);

    const ProcessResult forced = sb.cli({"generate", "--until", "none", "--seed", "2", "--out", u8(out), "--force"});
    CHECK(forced.exit_code == EXIT_OK);
    CHECK(forced.out == printed_path(out));
    CHECK(read_bytes(out) == read_bytes(seed2));

    // Default path too.
    REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "1"}).exit_code == EXIT_OK);
    CHECK(sb.cli({"generate", "--until", "none", "--seed", "1"}).exit_code == EXIT_OUTPUT);
    CHECK(sb.cli({"generate", "--until", "none", "--seed", "1", "--force"}).exit_code == EXIT_OK);
}

TEST_CASE("cli 12.2: equivalent ways of giving seed and map size give identical files") {
    Sandbox sb;
    auto gen = [&](const std::string& name, std::vector<std::string> args) {
        const fs::path out = sb.file(name);
        args.insert(args.begin(), {"generate", "--until", "none", "--out", u8(out)});
        const ProcessResult r = sb.cli(args);
        INFO(name, " stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        return read_bytes(out);
    };
    auto settings = [&](const std::string& name, std::string_view text) {
        const fs::path p = sb.file(name);
        write_text(p, text);
        return u8(p);
    };

    SUBCASE("seed") {
        const Bytes reference = gen("a.db", {"--seed", "42"});
        CHECK(gen("b.db", {"--seed", "0x2A"}) == reference);
        CHECK(gen("c.db", {"--seed", "0x2a"}) == reference);
        CHECK(gen("d.db", {"--seed", "0x000000000000002a"}) == reference);
        CHECK(gen("e.db", {"--settings", settings("s1.toml", "[world]\nseed = 42\n")}) == reference);
        CHECK(gen("f.db", {"--settings", settings("s2.toml", "[world]\nseed = \"42\"\n")}) == reference);
        CHECK(gen("g.db", {"--settings", settings("s3.toml", "[world]\nseed = \"0x2A\"\n")}) == reference);
        // A seed flag replaces both seed keys of the file (P0-03 6).
        CHECK(gen("h.db", {"--seed", "42", "--settings", settings("s4.toml", "[world]\nseed = 7\n")}) == reference);
        CHECK(gen("i.db", {"--seed", "42", "--settings", settings("s5.toml", "[world]\nseed_phrase = \"x\"\n")}) ==
              reference);
        CHECK(gen("j.db", {"--seed", "43"}) != reference);
        // Explicit default map size.
        CHECK(gen("k.db", {"--seed", "42", "--map-size", "1024"}) == reference);
    }
    SUBCASE("map size") {
        const Bytes reference = gen("a.db", {"--seed", "0", "--map-size", "512"});
        CHECK(gen("b.db", {"--settings", settings("s1.toml", "[world]\nseed = 0\nmap_size = 512\n")}) == reference);
        CHECK(gen("c.db", {"--seed", "0", "--settings", settings("s2.toml", "[world]\nmap_size = 512\n")}) ==
              reference);
        // Flags override the file key by key.
        CHECK(gen("d.db", {"--seed", "0", "--map-size", "512", "--settings",
                           settings("s3.toml", "[world]\nseed = 5\nmap_size = 2048\n")}) == reference);
        CHECK(gen("e.db", {"--seed", "0"}) != reference);
        CHECK(open_world(sb.file("a.db")).map_size() == 512);
        CHECK(open_world(sb.file("e.db")).map_size() == 1024);
        CHECK(gen("f.db", {"--seed", "0", "--map-size", "2048"}) != reference);
    }
    SUBCASE("phrase") {
        const Bytes reference = gen("a.db", {"--seed-phrase", "The Unmapped"});
        CHECK(gen("b.db", {"--settings", settings("s1.toml", "[world]\nseed_phrase = \"The Unmapped\"\n")}) ==
              reference);
        // The scrubbed phrase is what is stored and hashed (P0-01 7.3).
        CHECK(gen("c.db", {"--seed-phrase", "  The Unmapped\t\n"}) == reference);
        CHECK(gen("c2.db", {"--seed-phrase", "the unmapped"}) == reference);
        CHECK(gen("d.db", {"--seed-phrase", "The Unmapped", "--settings",
                           settings("s2.toml", "[world]\nseed = 9\n")}) == reference);
        CHECK(open_world(sb.file("a.db")).seed_phrase() == "the unmapped");
    }
    SUBCASE("three spellings of \"ashen marches\", by flag and by settings file (amendment 15)") {
        const Bytes reference = gen("ref.db", {"--seed-phrase", "ashen marches"});
        const char* spellings[] = {"Ashen Marches!", "ashen-marches", "ASHEN  MARCHES"};
        int i = 0;
        for (const char* spelling : spellings) {
            CAPTURE(std::string_view(spelling));
            const std::string n = std::to_string(i++);
            CHECK(gen("flag" + n + ".db", {"--seed-phrase", spelling}) == reference);
            CHECK(gen("file" + n + ".db", {"--settings", settings("p" + n + ".toml", std::string("[world]\nseed_phrase = \"") +
                                                                                       spelling + "\"\n")}) == reference);
        }
        const WorldReader w = open_world(sb.file("flag0.db"));
        CHECK(w.seed_phrase() == "ashen marches");
        CHECK(w.meta("seed_phrase") == std::optional<std::string>("ashen marches"));
        CHECK(w.seed() == worldgen::phrase_seed("ashen marches"));
        CHECK(gen("other.db", {"--seed-phrase", "ashen marchez"}) != reference);
    }
    SUBCASE("--seed-phrase 42 is a phrase, a different world from --seed 42 (amendment 15)") {
        const Bytes by_phrase = gen("p.db", {"--seed-phrase", "42"});
        const Bytes by_seed = gen("s.db", {"--seed", "42"});
        CHECK(by_phrase != by_seed);
        const WorldReader wp = open_world(sb.file("p.db"));
        CHECK(wp.seed_phrase() == "42");
        CHECK(wp.seed() == worldgen::phrase_seed("42"));
        CHECK(wp.seed() != 42);
        const WorldReader ws = open_world(sb.file("s.db"));
        CHECK(ws.seed_phrase().empty());
        CHECK(ws.seed() == 42);
        CHECK(gen("f.db", {"--settings", settings("p42.toml", "[world]\nseed_phrase = \"42\"\n")}) == by_phrase);
    }
    SUBCASE("an empty data directory is an input error: the worldgen kind is required (P0-03 amendment 17)") {
        fs::create_directories(sb.file("data"));
        const ProcessResult r =
            sb.cli({"generate", "--until", "none", "--seed", "5", "--data", u8(sb.file("data")), "--out", u8(sb.file("a.db"))});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK_FALSE(file_exists(sb.file("a.db")));
    }
}

TEST_CASE("cli 12.2: output path and current directory do not reach the file") {
    Sandbox sb;
    fs::create_directories(sb.file("a"));
    fs::create_directories(sb.file("b"));
    fs::create_directories(sb.file("work"));
    for (const bool pattern : {false, true}) {
        CAPTURE(pattern);
        std::vector<std::string> extra = {"--seed", "42"};
        if (pattern) {
            extra.emplace_back("--test-pattern");
        } else {
            extra.insert(extra.end(), {"--until", "none"});
        }
        const std::string tag = pattern ? "p" : "e";
        auto args = [&](const std::string& out) {
            std::vector<std::string> v = {"generate", "--out", out};
            v.insert(v.end(), extra.begin(), extra.end());
            return v;
        };
        // Relative --out resolves against the current directory (4.3); the
        // printed path is absolute.
        RunOptions in_files;
        in_files.cwd = sb.files();
        const ProcessResult r1 = sb.cli(args("a/w" + tag + ".db"), in_files);
        REQUIRE(r1.exit_code == EXIT_OK);
        CHECK(r1.out == printed_path(sb.file("a/w" + tag + ".db")));
        RunOptions in_work;
        in_work.cwd = sb.file("work");
        const ProcessResult r2 = sb.cli(args("../b/other-name" + tag + ".db"), in_work);
        REQUIRE(r2.exit_code == EXIT_OK);
        CHECK(r2.out == printed_path(sb.file("b/other-name" + tag + ".db")));
        const ProcessResult r3 = sb.cli(args(u8(sb.file("b/third" + tag + ".db"))));
        REQUIRE(r3.exit_code == EXIT_OK);
        const ProcessResult r4 = sb.cli(pattern ? std::vector<std::string>{"generate", "--seed", "42", "--test-pattern"}
                                                : std::vector<std::string>{"generate", "--until", "none", "--seed", "42"});
        REQUIRE(r4.exit_code == EXIT_OK);
        const Bytes reference = read_bytes(sb.file("a/w" + tag + ".db"));
        REQUIRE(!reference.empty());
        CHECK(read_bytes(sb.file("b/other-name" + tag + ".db")) == reference);
        CHECK(read_bytes(sb.file("b/third" + tag + ".db")) == reference);
        CHECK(read_bytes(sb.out_dir() / "world-42.db") == reference);
        fs::remove(sb.out_dir() / "world-42.db");
    }
    CHECK(fs::is_empty(sb.file("work")));
}

TEST_CASE("cli 12.2: environment and locale variables do not reach the file") {
    Sandbox sb;
    const std::vector<std::vector<EnvChange>> variants = {
        {{"TZ", "UTC"}},
        {{"TZ", "Asia/Kolkata"}},
        {{"LANG", "C"}, {"LC_ALL", std::nullopt}},
        {{"LANG", "de_DE.UTF-8"}, {"LC_ALL", "de_DE.UTF-8"}, {"LC_NUMERIC", "de_DE.UTF-8"}},
        {{"TZ", "Asia/Kolkata"}, {"LANG", "tr_TR.UTF-8"}, {"LC_ALL", "tr_TR.UTF-8"}},
    };
    // A setting that names an unavailable locale is harmless: the CLI never
    // calls setlocale (4.4), so no variant needs to be skipped.
    for (const bool pattern : {false, true}) {
        CAPTURE(pattern);
        Bytes reference;
        int index = 0;
        for (const auto& env : variants) {
            CAPTURE(index);
            const fs::path out = sb.file("env-" + std::string(pattern ? "p" : "e") + "-" + std::to_string(index++) + ".db");
            RunOptions o;
            o.env = env;
            std::vector<std::string> args = {"generate", "--seed-phrase", std::string(PHRASE_NON_ASCII), "--out", u8(out)};
            if (pattern) {
                args.emplace_back("--test-pattern");
            } else {
                args.insert(args.end(), {"--until", "none"});
            }
            const ProcessResult r = sb.cli(args, o);
            INFO("stderr: ", r.err);
            REQUIRE(r.exit_code == EXIT_OK);
            const Bytes bytes = read_bytes(out);
            if (reference.empty()) {
                reference = bytes;
            }
            CHECK(bytes == reference);
        }
    }
}

TEST_CASE("cli 12.2: a non-ASCII seed phrase and output path survive on every platform") {
    Sandbox sb;
    // "Ünmapped Wörld" scrubs to "nmapped w rld" (amendment 15).
    const std::uint64_t seed = phrase_to_seed(PHRASE_NON_ASCII);
    CHECK(seed == worldgen::phrase_seed("nmapped w rld"));
    MESSAGE("seed of \"\xC3\x9Cnmapped W\xC3\xB6rld\" = ", seed, " (golden value for the validation specialist)");

    const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed-phrase", std::string(PHRASE_NON_ASCII)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    const fs::path expected = sb.out_dir() / ("world-" + std::to_string(seed) + ".db");
    CHECK(r.out == printed_path(expected));
    const WorldReader w = open_world(expected);
    CHECK(w.seed() == seed);
    CHECK(w.meta("seed_phrase") == std::optional<std::string>("nmapped w rld"));
    // The same world as the scrubbed phrase typed directly.
    const fs::path plain = sb.file("plain.db");
    REQUIRE(sb.cli({"generate", "--until", "none", "--seed-phrase", "nmapped w rld", "--out", u8(plain)}).exit_code == EXIT_OK);
    CHECK(read_bytes(plain) == read_bytes(expected));

    // A UTF-8 output path ("wörld Ü.db") in a UTF-8 directory.
    const std::string dir_name = "d\xC3\xAFr";
    fs::create_directories(sb.file(dir_name));
    const fs::path out = sb.file(dir_name + "/w\xC3\xB6rld \xC3\x9C.db");
    const ProcessResult r2 = sb.cli({"generate", "--until", "none", "--seed-phrase", std::string(PHRASE_NON_ASCII), "--out", u8(out)});
    INFO("stderr: ", r2.err);
    REQUIRE(r2.exit_code == EXIT_OK);
    CHECK(r2.out == printed_path(out));
    CHECK(file_exists(out));
    CHECK(read_bytes(out) == read_bytes(expected));
    // The other commands accept the UTF-8 path too.
    CHECK(sb.cli({"hash", "--db", u8(out)}).out == file_sha256_hex(out) + "\n");
    CHECK(sb.cli({"report", "--db", u8(out)}).exit_code == EXIT_OK);
}

TEST_CASE("cli 12.2: empty world contents (P0-02 8)") {
    Sandbox sb;
    const fs::path out = sb.generate({"--until", "none", "--seed", "0"});
    const WorldReader w = open_world(out);
    const std::vector<worldgen::db::MetaEntry> meta = w.meta_entries();
    const p004::MetaRows expected = cli_meta_rows(0);
    REQUIRE(meta.size() == expected.size());
    for (std::size_t i = 0; i < meta.size(); ++i) {
        CHECK(meta[i].key == expected[i].first);
        CHECK(meta[i].value == expected[i].second);
    }
    const auto tables = w.tables();
    REQUIRE(tables.ok());
    // Schema 9 (P3-04 9.1, P5-12 7.2, P5-02 11.1): thirty-two tables, all empty but meta (17 rows).
    const std::vector<std::string> names = {"age", "beast", "beast_population", "biome", "civilization", "climate", "culture", "event", "event_cause",
                                            "event_kind", "event_kind_role", "event_kind_value", "event_participant",
                                            "event_value", "god", "layer", "meta", "monster", "monster_kind", "pass", "phase", "race",
                                            "race_biome", "region", "region_biome", "region_border",
                                            "region_control", "region_name", "relation", "settlement",
                                            "settlement_census", "site"};
    REQUIRE(tables.value().size() == names.size());
    for (std::size_t k = 0; k < names.size(); ++k) {
        CHECK(tables.value()[k].name == names[k]);
        CHECK(tables.value()[k].row_count == (names[k] == "meta" ? 17U : 0U));
    }
    const auto layers = w.layers();
    REQUIRE(layers.ok());
    CHECK(layers.value().empty());
    CHECK(w.stages().empty());
    // Expected size, P0-02 8 and amendment 9; schema version 2 adds the root
    // page of table pass (P1-01 9.1); schema version 3 the root pages of
    // tables biome and climate and of the automatic index of biome.key;
    // schema version 5 the root pages of the three region tables (P0-02
    // amendment 17); schema version 6 the roots of the five chronology tables
    // and two key indexes, 18 pages (P3-03 5.1); schema version 7 the
    // thirteen history tables, 34 pages (P3-04 9.6); schema version 8 the
    // root of region_name, 35 pages (P5-12 12).
    CHECK(fs::file_size(out) == 184320U);   // P5-02 11.1 (r1): 43 pages
}

TEST_CASE("cli 12.2: test pattern") {
    Sandbox sb;
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        CAPTURE(seed);
        // The map size does not change the pattern (5.2: 256 x 256
        // regardless of --map-size).
        const fs::path out =
            sb.generate({"--seed", std::to_string(seed), "--test-pattern", "--map-size", seed == 0 ? "512" : "2048"});
        const WorldReader w = open_world(out);
        CHECK(w.meta("stages") == std::optional<std::string>("test_pattern"));
        CHECK(w.stages() == std::vector<std::string>{"test_pattern"});
        const auto list = w.layers();
        REQUIRE(list.ok());
        REQUIRE(list.value().size() == 3);
        const char* names[] = {"test.ramp", "test.hash", "test.noise"};
        const ElementType types[] = {ElementType::u8, ElementType::u8, ElementType::i16};
        for (std::size_t i = 0; i < 3; ++i) {
            CAPTURE(i);
            const worldgen::db::LayerInfo& info = list.value()[i];
            CHECK(info.id == static_cast<std::int64_t>(i + 1));
            CHECK(info.name == names[i]);
            CHECK(info.width == 256U);
            CHECK(info.height == 256U);
            CHECK(info.type == types[i]);
            CHECK(info.codec == Codec::delta_varint);
        }

        const auto ramp = w.read_layer("test.ramp");
        REQUIRE(ramp.ok());
        REQUIRE(ramp.value().grid.get_if<std::uint8_t>() != nullptr);
        CHECK(*ramp.value().grid.get_if<std::uint8_t>() == p004::ramp_cells());

        const auto hash = w.read_layer("test.hash");
        REQUIRE(hash.ok());
        REQUIRE(hash.value().grid.get_if<std::uint8_t>() != nullptr);
        CHECK(*hash.value().grid.get_if<std::uint8_t>() == p004::hash_cells(seed));

        const auto noise = w.read_layer("test.noise");
        REQUIRE(noise.ok());
        const std::vector<std::int16_t>* cells = noise.value().grid.get_if<std::int16_t>();
        REQUIRE(cells != nullptr);
        CHECK(*cells == p004::noise_cells(seed));
        for (const std::int16_t v : *cells) {
            if (v < -32767) {
                FAIL("test.noise value below -32767: ", v);
            }
        }
    }
}

TEST_CASE("cli 12.2: a fatal error inside a stage gives exit 1 and leaves nothing") {
    Sandbox sb;
    SUBCASE("default path") {
        const ProcessResult r = sb.hooks({"generate", "--seed", "0", "--test-pattern", "--test-fatal-in-stage"});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_INTERNAL);
        CHECK(!r.signalled);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
        CHECK(!file_exists(sb.out_dir() / "world-0.db"));
        // Sandbox: no .partial anywhere.
    }
    SUBCASE("explicit --out") {
        const fs::path out = sb.file("w.db");
        const ProcessResult r =
            sb.hooks({"generate", "--seed", "0", "--test-pattern", "--test-fatal-in-stage", "--out", u8(out)});
        CHECK(r.exit_code == EXIT_INTERNAL);
        CHECK(!file_exists(out));
    }
    SUBCASE("an existing output survives with --force") {
        const fs::path out = sb.file("w.db");
        REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(out)}).exit_code == EXIT_OK);
        const Bytes before = read_bytes(out);
        const ProcessResult r = sb.hooks(
            {"generate", "--seed", "0", "--test-pattern", "--test-fatal-in-stage", "--out", u8(out), "--force"});
        CHECK(r.exit_code == EXIT_INTERNAL);
        CHECK(read_bytes(out) == before);
    }
    SUBCASE("the test-hook build otherwise behaves as the production binary") {
        const ProcessResult h = sb.hooks({"generate", "--seed", "0", "--test-pattern"});
        CHECK(h.exit_code == EXIT_OK);
        const ProcessResult p = sb.cli({"generate", "--seed", "0", "--test-pattern", "--out", u8(sb.file("p.db"))});
        CHECK(p.exit_code == EXIT_OK);
        CHECK(read_bytes(sb.out_dir() / "world-0.db") == read_bytes(sb.file("p.db")));
    }
    SUBCASE("the production binary rejects the hook option (amendment 11)") {
        const ProcessResult r = sb.cli({"generate", "--seed", "0", "--test-pattern", "--test-fatal-in-stage"});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(!file_exists(sb.out_dir() / "world-0.db"));
    }
}

TEST_CASE("cli 10: generate twice gives identical bytes") {
    Sandbox sb;
    const ProcessResult a = sb.cli({"generate", "--seed", "7", "--test-pattern", "--out", u8(sb.file("a.db"))});
    const ProcessResult b = sb.cli({"generate", "--seed", "7", "--test-pattern", "--out", u8(sb.file("b.db"))});
    REQUIRE(a.exit_code == EXIT_OK);
    REQUIRE(b.exit_code == EXIT_OK);
    CHECK(read_bytes(sb.file("a.db")) == read_bytes(sb.file("b.db")));
}
