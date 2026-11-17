// Fixed-id kinds (spec P0-03 amendment 15, adopting P1-00 appendix A3
// amendment 16; P1-00 section 6.1) and the registration rules for them.
//
// Written from docs/specs/p0-03-toml-loader.md, docs/specs/p1-00-geography-
// overview.md and the public header worldgen/data/schema.hpp only. The
// expected canonical byte streams are written out by hand from P0-03 section 8.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "core_test_util.hpp"
#include "data_loader_support.hpp"
#include "worldgen/core/sha256.hpp"

using namespace loader_test;

namespace {

// --- The test kind `tune` (directory `tunes`) ---------------------------------
//
//   id alpha_stage: name string(16) required; ratio fx(0, 1) required
//   id beta_stage:  x int(0, 10) required; flag bool default true;
//                   named check x_not_7 ("x must not be 7")
//
// Ids are given out of order on purpose: the registry keeps them in
// ascending id order.

wd::Schema alpha_stage_schema() {
    std::vector<wd::Field> f;
    f.push_back(wd::Field::required("name", wd::Type::string(16)));
    f.push_back(wd::Field::required("ratio", wd::Type::fixed("0", "1")));
    return wd::Schema(std::move(f));
}

wd::Schema beta_stage_schema() {
    std::vector<wd::Field> f;
    f.push_back(wd::Field::required("x", wd::Type::integer(0, 10)));
    f.push_back(wd::Field::optional("flag", wd::Type::boolean(), wd::Value::boolean(true)));
    std::vector<wd::Check> checks;
    checks.push_back(wd::Check{"x_not_7", [](const wd::Record& r) -> std::optional<std::string> {
                                   if (r.get_int("x") == 7) {
                                       return std::string("x must not be 7");
                                   }
                                   return std::nullopt;
                               }});
    return wd::Schema(std::move(f), std::move(checks));
}

wd::Kind tune_kind(bool required) {
    std::vector<wd::IdSchema> ids;
    ids.push_back(wd::IdSchema{"beta_stage", beta_stage_schema()});
    ids.push_back(wd::IdSchema{"alpha_stage", alpha_stage_schema()});
    return wd::Kind::with_fixed_ids("tune", "tunes", required, std::move(ids));
}

// An open kind beside it: `plain` (directory plains, not required): v int(0, 9) required.
wd::Kind plain_kind() {
    std::vector<wd::Field> f;
    f.push_back(wd::Field::required("v", wd::Type::integer(0, 9)));
    wd::Kind k;
    k.name = "plain";
    k.directory = "plains";
    k.required = false;
    k.schema = wd::Schema(std::move(f));
    return k;
}

wd::KindRegistry tune_registry(bool required = true) {
    wd::KindRegistry r;
    r.add(tune_kind(required));
    r.add(plain_kind());
    return r;
}

constexpr std::string_view GOOD_ALPHA = "[tune.alpha_stage]\nname = \"hills\"\nratio = \"0.25\"\n";
constexpr std::string_view GOOD_BETA = "[tune.beta_stage]\nx = 3\n";

wd::LoadResult<wd::ContentSet> load_tunes(std::vector<wd::MemoryEntry> files, bool required = true) {
    wd::MemoryDataRoot root;
    root.directories.push_back(dir("tunes", std::move(files)));
    return wd::load_content_from_memory(tune_registry(required), root);
}

// The byte stream of P0-03 section 8, built by hand.
class Stream {
public:
    Stream& u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            bytes_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFU));
        }
        return *this;
    }
    Stream& byte(std::uint8_t b) {
        bytes_.push_back(b);
        return *this;
    }
    Stream& str(std::string_view s) {
        u64(static_cast<std::uint64_t>(s.size()));
        for (const char c : s) {
            bytes_.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
        }
        return *this;
    }
    Stream& f_int(std::string_view name, std::int64_t v) { return str(name).byte(0x01).u64(static_cast<std::uint64_t>(v)); }
    Stream& f_fx(std::string_view name, std::int64_t raw) {
        return str(name).byte(0x02).u64(static_cast<std::uint64_t>(raw));
    }
    Stream& f_bool(std::string_view name, bool v) { return str(name).byte(0x03).byte(v ? 0x01 : 0x00); }
    Stream& f_str(std::string_view name, std::string_view v) { return str(name).byte(0x04).str(v); }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

}  // namespace

// --- Registration ---------------------------------------------------------------------

TEST_CASE("fixed-id kinds: registration stores the ids in ascending order with their own schemas") {
    const wd::KindRegistry r = tune_registry();
    const auto& kinds = r.kinds();
    REQUIRE(kinds.size() == 2U);
    CHECK(kinds[0].name == "plain");
    CHECK(kinds[1].name == "tune");
    const wd::Kind& tune = kinds[1];
    CHECK(tune.has_fixed_ids());
    CHECK_FALSE(kinds[0].has_fixed_ids());
    CHECK(tune.directory == "tunes");
    CHECK(tune.required);
    CHECK(tune.schema.fields().empty());
    CHECK(tune.schema.checks().empty());
    REQUIRE(tune.id_schemas.size() == 2U);
    CHECK(tune.id_schemas[0].id == "alpha_stage");
    CHECK(tune.id_schemas[1].id == "beta_stage");
    const wd::Schema* a = tune.schema_for("alpha_stage");
    const wd::Schema* b = tune.schema_for("beta_stage");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a->find("name") != nullptr);
    CHECK(a->find("x") == nullptr);
    CHECK(b->find("x") != nullptr);
    CHECK(b->checks().size() == 1U);
    CHECK(tune.schema_for("gamma") == nullptr);
    CHECK(tune.schema_for("") == nullptr);
    // An open kind's schema serves every id.
    CHECK(kinds[0].schema_for("anything") == &kinds[0].schema);
}

TEST_CASE("fixed-id kinds: registration mistakes are fatal") {
    const auto with_ids = [](std::vector<std::string> ids) {
        std::vector<wd::IdSchema> v;
        for (std::string& id : ids) {
            v.push_back(wd::IdSchema{std::move(id), beta_stage_schema()});
        }
        return v;
    };
    const auto add = [](wd::Kind k) {
        wd::KindRegistry r;
        r.add(std::move(k));
    };
    // Ids that do not match [a-z][a-z0-9_]*, 1 to 64 bytes.
    for (const std::string& bad : {std::string(""), std::string("Upper"), std::string("1st"), std::string("_x"),
                                  std::string("a-b"), std::string("a.b"), std::string("a b"), std::string(65, 'a')}) {
        CAPTURE(bad);
        CHECK_FATAL(add(wd::Kind::with_fixed_ids("tune", "tunes", true, with_ids({"alpha", bad}))));
    }
    // 64 bytes is fine.
    add(wd::Kind::with_fixed_ids("tune", "tunes", true, with_ids({std::string(64, 'a')})));
    // An id listed twice.
    CHECK_FATAL(add(wd::Kind::with_fixed_ids("tune", "tunes", true, with_ids({"alpha", "beta", "alpha"}))));
    // An empty id set: the registry test requires non-empty id sets
    // (amendment 16); with_fixed_ids or add must refuse it.
    CHECK_FATAL(add(wd::Kind::with_fixed_ids("tune", "tunes", true, {})));
    // A fixed-id kind whose common schema has fields or checks.
    {
        wd::Kind k = tune_kind(true);
        std::vector<wd::Field> f;
        f.push_back(wd::Field::required("stray", wd::Type::boolean()));
        k.schema = wd::Schema(std::move(f));
        CHECK_FATAL(add(std::move(k)));
    }
    {
        wd::Kind k = tune_kind(true);
        std::vector<wd::Check> c;
        c.push_back(wd::Check{"stray", [](const wd::Record&) -> std::optional<std::string> { return std::nullopt; }});
        k.schema = wd::Schema({}, std::move(c));
        CHECK_FATAL(add(std::move(k)));
    }
    // The kind's own name and directory rules still apply, and so do the
    // uniqueness rules across kinds.
    CHECK_FATAL(add(wd::Kind::with_fixed_ids("Tune", "tunes", true, with_ids({"alpha"}))));
    CHECK_FATAL(add(wd::Kind::with_fixed_ids("tune", "tu/nes", true, with_ids({"alpha"}))));
    {
        wd::KindRegistry r;
        r.add(tune_kind(true));
        CHECK_FATAL(r.add(wd::Kind::with_fixed_ids("tune", "others", true, with_ids({"alpha"}))));
        CHECK_FATAL(r.add(wd::Kind::with_fixed_ids("other", "tunes", true, with_ids({"alpha"}))));
    }
}

// --- Loading -------------------------------------------------------------------------

TEST_CASE("fixed-id kinds: a complete set loads, each definition with its own schema's fields") {
    const auto r = load_tunes({file("a.toml", std::string(GOOD_BETA)), file("b.toml", std::string(GOOD_ALPHA))});
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    const wd::KindContent* tune = r.value->find_kind("tune");
    REQUIRE(tune != nullptr);
    REQUIRE(tune->definitions.size() == 2U);
    CHECK(tune->definitions[0].id == "alpha_stage");
    CHECK(tune->definitions[1].id == "beta_stage");
    const wd::Record& a = tune->definitions[0].record;
    REQUIRE(a.size() == 2U);
    CHECK(a.name_at(0) == "name");
    CHECK(a.get_string("name") == "hills");
    CHECK(a.get_fx("ratio").raw() == 1073741824);
    const wd::Record& b = tune->definitions[1].record;
    REQUIRE(b.size() == 2U);
    CHECK(b.get_int("x") == 3);
    CHECK(b.get_bool("flag"));  // default applied
    // The open kind beside it, with no directory, is empty.
    const wd::KindContent* plain = r.value->find_kind("plain");
    REQUIRE(plain != nullptr);
    CHECK(plain->definitions.empty());
}

TEST_CASE("fixed-id kinds: an unknown id is an error at its header, naming the kind and the expected ids") {
    const std::string text = std::string(GOOD_ALPHA) + std::string(GOOD_BETA) + "\n[tune.gamma]\nx = 1\n";
    const auto r = load_tunes({file("t.toml", text)});
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.errors.size() == 1U);
    const wd::LoadError& e = r.errors[0];
    CHECK(e.path == "tunes/t.toml");
    CHECK(e.line == 7U);  // the [tune.gamma] header
    CHECK(e.column == 1U);
    // Wording of amendment 16: "unknown id '<id>' for kind '<kind>'; expected one of ...".
    CHECK(contains(e.message, "unknown id 'gamma' for kind 'tune'"));
    CHECK(contains(e.message, "expected one of"));
    const std::size_t pa = e.message.find("alpha_stage");
    const std::size_t pb = e.message.find("beta_stage");
    CHECK(pa != std::string::npos);
    CHECK(pb != std::string::npos);
    CHECK(pa < pb);  // ascending id order
    // The header's exact form.
    CHECK(e.message == "unknown id 'gamma' for kind 'tune'; expected one of 'alpha_stage', 'beta_stage'");
    // The unknown definition is not validated against any schema: a key
    // that no schema has raises nothing beyond the unknown id.
    const auto r2 = load_tunes(
        {file("t.toml", std::string(GOOD_ALPHA) + std::string(GOOD_BETA) + "[tune.gamma]\nbogus = \"text\"\n")});
    INFO(error_dump(r2));
    REQUIRE(r2.errors.size() == 1U);
    CHECK(contains(r2.errors[0].message, "unknown id 'gamma'"));
}

TEST_CASE("fixed-id kinds: a missing id is an error against the directory, with no position") {
    const auto r = load_tunes({file("t.toml", std::string(GOOD_ALPHA))});
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.errors.size() == 1U);
    CHECK(r.errors[0].path == "tunes");
    CHECK(r.errors[0].line == 0U);
    CHECK(r.errors[0].column == 0U);
    CHECK(contains(r.errors[0].message, "missing definition 'tune.beta_stage'"));
    CHECK(r.errors[0].to_string() == "tunes: error: " + r.errors[0].message);

    // Both missing, and an unknown one: errors sorted (P0-03 9), the
    // position-less ones first.
    const auto r2 = load_tunes({file("t.toml", "[tune.zeta]\nx = 1\n")});
    check_content_result(r2);
    INFO(error_dump(r2));
    REQUIRE(r2.errors.size() == 3U);
    CHECK(r2.errors[0].path == "tunes");
    CHECK(contains(r2.errors[0].message, "missing definition 'tune.alpha_stage'"));
    CHECK(r2.errors[1].path == "tunes");
    CHECK(contains(r2.errors[1].message, "missing definition 'tune.beta_stage'"));
    CHECK(r2.errors[2].path == "tunes/t.toml");
    CHECK(r2.errors[2].line == 1U);
    CHECK(contains(r2.errors[2].message, "unknown id 'zeta' for kind 'tune'"));
}

TEST_CASE("fixed-id kinds: each definition is validated against its own id's schema, named checks included") {
    SUBCASE("a field of the other id's schema is an unknown key") {
        const auto r = load_tunes({file("t.toml", "[tune.alpha_stage]\nname = \"a\"\nratio = 0\nx = 3\n" +
                                                      std::string(GOOD_BETA))});
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.errors.size() == 1U);
        CHECK(r.errors[0].line == 4U);  // reported at the value (amendment 10)
        CHECK(contains(r.errors[0].message, "tune.alpha_stage.x"));
    }
    SUBCASE("a required field of its own schema is missing") {
        const auto r = load_tunes({file("t.toml", std::string(GOOD_ALPHA) + "[tune.beta_stage]\nflag = false\n")});
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.errors.size() == 1U);
        CHECK(r.errors[0].line == 4U);  // the definition's header
        CHECK(contains(r.errors[0].message, "tune.beta_stage.x"));
    }
    SUBCASE("types and ranges of its own schema") {
        const auto r = load_tunes({file("t.toml", "[tune.alpha_stage]\nname = \"seventeen-bytes-xx\"\nratio = \"1.5\"\n" +
                                                      std::string(GOOD_BETA))});
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.errors.size() == 2U);
        CHECK(contains(r.errors[0].message, "tune.alpha_stage.name"));
        CHECK(contains(r.errors[1].message, "tune.alpha_stage.ratio"));
    }
    SUBCASE("a named check of one id runs on that id only") {
        const auto bad = load_tunes({file("t.toml", std::string(GOOD_ALPHA) + "[tune.beta_stage]\nx = 7\n")});
        check_content_result(bad);
        INFO(error_dump(bad));
        REQUIRE(bad.errors.size() == 1U);
        CHECK(bad.errors[0].line == 4U);  // at the header (amendment 8)
        CHECK(contains(bad.errors[0].message, "x must not be 7"));
        // alpha_stage has no such check (and no field x); a valid set loads.
        const auto good = load_tunes({file("t.toml", std::string(GOOD_ALPHA) + "[tune.beta_stage]\nx = 10\nflag = false\n")});
        INFO(error_dump(good));
        REQUIRE(good.ok());
        CHECK_FALSE(good.value->find("tune", "beta_stage")->record.get_bool("flag"));
    }
    SUBCASE("a duplicate id across files is still an error") {
        const auto r = load_tunes({file("a.toml", std::string(GOOD_ALPHA) + std::string(GOOD_BETA)),
                                   file("b.toml", std::string(GOOD_BETA))});
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.errors.size() == 1U);
        CHECK(r.errors[0].path == "tunes/b.toml");
    }
}

TEST_CASE("fixed-id kinds: a missing directory, for a required and a non-required kind") {
    SUBCASE("required: only the missing directory is reported") {
        wd::MemoryDataRoot root;  // no tunes directory
        const auto r = wd::load_content_from_memory(tune_registry(true), root);
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        REQUIRE(r.errors.size() == 1U);
        CHECK(r.errors[0].path == "tunes");
        CHECK(r.errors[0].line == 0U);
        CHECK_FALSE(contains(r.errors[0].message, "missing definition"));
    }
    SUBCASE("not required: loads with no definitions") {
        wd::MemoryDataRoot root;
        const auto r = wd::load_content_from_memory(tune_registry(false), root);
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        const wd::KindContent* tune = r.value->find_kind("tune");
        REQUIRE(tune != nullptr);
        CHECK(tune->definitions.empty());
    }
    SUBCASE("without a data root: required is an error, not required is empty") {
        const auto req = wd::load_content_without_data(tune_registry(true));
        check_content_result(req);
        CHECK_FALSE(req.ok());
        const auto opt = wd::load_content_without_data(tune_registry(false));
        check_content_result(opt);
        INFO(error_dump(opt));
        REQUIRE(opt.ok());
        CHECK(opt.value->find_kind("tune")->definitions.empty());
    }
}

TEST_CASE("fixed-id kinds: an existing but empty directory reports every missing id") {
    for (const bool required : {true, false}) {
        CAPTURE(required);
        const auto r = load_tunes({file("notes.md", "not data")}, required);
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        bool alpha = false;
        bool beta = false;
        for (const wd::LoadError& e : r.errors) {
            CHECK(e.path == "tunes");
            alpha = alpha || contains(e.message, "missing definition 'tune.alpha_stage'");
            beta = beta || contains(e.message, "missing definition 'tune.beta_stage'");
        }
        CHECK(alpha);
        CHECK(beta);
        // For a required kind, P0-03 7.1 also calls zero definitions an
        // error; whether that is reported in addition is not specified.
        MESSAGE("errors for an empty directory (required = ", required, "):\n", error_dump(r));
    }
}

// --- Canonical encoding and content hash ---------------------------------------------

TEST_CASE("fixed-id kinds: canonical encoding and content_hash, built by hand") {
    const auto r = load_tunes({file("b.toml", "[tune.beta_stage]\nflag = false\nx = 9\n"),
                               file("a.toml", "[tune.alpha_stage]\nratio = 1\nname = \"ridge\"\n")});
    INFO(error_dump(r));
    REQUIRE(r.ok());
    Stream s;
    // Kinds in ascending name: plain (zero definitions), then tune.
    s.str("plain").u64(0);
    s.str("tune").u64(2);
    s.str("alpha_stage").f_str("name", "ridge").f_fx("ratio", worldgen_test::ONE_RAW);
    s.str("beta_stage").f_int("x", 9).f_bool("flag", false);
    CHECK(wd::canonical_encoding(*r.value) == s.bytes());
    CHECK(wd::content_hash(*r.value) == worldgen::sha256(s.bytes()));
    CHECK(wd::content_hash_hex(*r.value) == worldgen::to_hex(worldgen::sha256(s.bytes())));
    // Moving the definitions between files changes nothing.
    const auto moved =
        load_tunes({file("z.toml", "[tune.alpha_stage]\nname = \"ridge\"\nratio = \"1.0\"\n[tune.beta_stage]\nx = 9\nflag = false\n")});
    REQUIRE(moved.ok());
    CHECK(wd::canonical_encoding(*moved.value) == s.bytes());
    // A default is encoded after it is applied.
    const auto defaulted = load_tunes({file("t.toml", std::string(GOOD_ALPHA) + std::string(GOOD_BETA))});
    REQUIRE(defaulted.ok());
    Stream d;
    d.str("plain").u64(0);
    d.str("tune").u64(2);
    d.str("alpha_stage").f_str("name", "hills").f_fx("ratio", 1073741824);
    d.str("beta_stage").f_int("x", 3).f_bool("flag", true);
    CHECK(wd::canonical_encoding(*defaulted.value) == d.bytes());
}

TEST_CASE("fixed-id kinds: loading from disk") {
    ScratchDir scratch;
    scratch.make_dir("tunes");
    scratch.write("tunes/elevation.toml", std::string(GOOD_ALPHA));
    scratch.write("tunes/hydrology.toml", std::string(GOOD_BETA));
    const auto r = wd::load_content(tune_registry(true), scratch.path());
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    CHECK(r.value->find("tune", "alpha_stage") != nullptr);
    CHECK(r.value->find("tune", "beta_stage") != nullptr);
    scratch.write("tunes/extra.toml", "[tune.delta]\nx = 1\n");
    const auto bad = wd::load_content(tune_registry(true), scratch.path());
    check_content_result(bad);
    REQUIRE(bad.errors.size() == 1U);
    CHECK(bad.errors[0].path == "tunes/extra.toml");
    CHECK(bad.errors[0].line == 1U);
    CHECK(contains(bad.errors[0].message, "unknown id 'delta' for kind 'tune'"));
}

TEST_CASE("fixed-id kinds: the production registry holds worldgen with the implemented stages' ids") {
    // P1-00 6.1: `worldgen`, directory `worldgen`, required, fixed ids
    // `elevation` (P1-01 4.2), `hydrology` (P1-02 4), `climate` (P1-03 4)
    // and `biomes` (P1-04 4.2), kept in ascending id order. Since P1-04 the
    // registry also holds the open kind `biome` (P1-00 6.6), which sorts
    // first.
    const wd::KindRegistry builtin = wd::builtin_kinds();
    REQUIRE(builtin.kinds().size() == 2U);
    CHECK(builtin.kinds()[0].name == "biome");
    const wd::Kind& k = builtin.kinds()[1];
    CHECK(k.name == "worldgen");
    CHECK(k.directory == "worldgen");
    CHECK(k.required);
    REQUIRE(k.has_fixed_ids());
    REQUIRE(k.id_schemas.size() == 4U);
    CHECK(k.id_schemas[0].id == "biomes");
    CHECK(k.id_schemas[1].id == "climate");
    CHECK(k.id_schemas[2].id == "elevation");
    CHECK(k.id_schemas[3].id == "hydrology");
}
