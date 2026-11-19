// The naming data (spec P3-02 sections 3 and 5; 9.1 item 7): the shipped
// naming blocks equal section 5 (transcribed in p3_02_names_support.hpp,
// not read from the files), culture_naming_from reads them, and each string
// grammar and named check of section 3 fails once with its message through
// P3-01's registration of kind `culture`.
//
// The data root is the checkout's game/data (P3-01 ships races/ and
// cultures/ there). For a failure case the root is read into memory and one
// culture file is replaced by a variant written by this test in the file
// form of P3-01 4. These tests need P3-01 merged first (P3-02 2: merge
// order P3-01, then P3-02).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "p3_02_names_api.hpp"
#include "p3_02_names_support.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"

namespace wd = worldgen::data;
using names_api::Naming;
using names_test::shipped_naming;

namespace {

const std::filesystem::path& data_root() {
    static const std::filesystem::path p = std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / "game" / "data";
    return p;
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The shipped data root, in memory: every kind directory with its entries.
wd::MemoryDataRoot shipped_in_memory() {
    wd::MemoryDataRoot root;
    std::vector<std::filesystem::path> dirs;
    for (const auto& d : std::filesystem::directory_iterator(data_root())) {
        if (d.is_directory()) dirs.push_back(d.path());
    }
    std::sort(dirs.begin(), dirs.end());
    for (const auto& d : dirs) {
        wd::MemoryDirectory md{d.filename().string(), {}};
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::directory_iterator(d)) files.push_back(f.path());
        std::sort(files.begin(), files.end());
        for (const auto& f : files) {
            const bool is_dir = std::filesystem::is_directory(f);
            md.entries.push_back(wd::MemoryEntry{f.filename().string(), is_dir ? std::string() : read_file(f), is_dir});
        }
        root.directories.push_back(std::move(md));
    }
    return root;
}

std::string toml_array(const std::vector<std::string>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) out += ", ";
        out += "\"" + v[i] + "\"";
    }
    return out + "]";
}

// A culture file in the form of P3-01 4.
std::string culture_toml(std::string_view id, std::string_view race, std::string_view name, const Naming& n) {
    std::ostringstream o;
    o << "[culture." << id << "]\nrace = \"" << race << "\"\nname = \"" << name << "\"\n\n[culture." << id
      << ".naming]\n";
    o << "onsets = " << toml_array(n.onsets) << "\n";
    o << "nuclei = " << toml_array(n.nuclei) << "\n";
    o << "codas = " << toml_array(n.codas) << "\n";
    o << "patterns = " << toml_array(n.patterns) << "\n";
    o << "min_letters = " << n.min_letters << "\n";
    o << "max_letters = " << n.max_letters << "\n";
    o << "forbidden = " << toml_array(n.forbidden) << "\n";
    o << "god_forms = " << toml_array(n.god_forms) << "\n";
    o << "civilization_forms = " << toml_array(n.civilization_forms) << "\n";
    o << "settlement_forms = " << toml_array(n.settlement_forms) << "\n";
    return o.str();
}

struct Loaded {
    bool ok = false;
    std::string errors;  // every error's to_string(), one per line
    std::size_t count = 0;
};

Loaded load_with_elven(const Naming& n) {
    wd::MemoryDataRoot root = shipped_in_memory();
    bool replaced = false;
    for (wd::MemoryDirectory& d : root.directories) {
        if (d.name != "cultures") continue;
        for (wd::MemoryEntry& e : d.entries) {
            if (e.name == "elven.toml") {
                e.bytes = culture_toml("elven", "elf", "Elven", n);
                replaced = true;
            }
        }
    }
    REQUIRE(replaced);
    const wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(wd::builtin_kinds(), root);
    Loaded out;
    out.ok = r.ok();
    out.count = r.errors.size();
    for (const wd::LoadError& e : r.errors) out.errors += e.to_string() + "\n";
    return out;
}

void check_fails(const Naming& n, std::string_view mention) {
    const Loaded l = load_with_elven(n);
    INFO(l.errors);
    CHECK_FALSE(l.ok);
    CHECK(l.count == 1);
    CHECK(l.errors.find("cultures/elven.toml") != std::string::npos);
    CHECK(l.errors.find(std::string(mention)) != std::string::npos);
}

std::string check_text(std::string_view check, std::string_view message) {
    return "error: check '" + std::string(check) + "' failed for 'culture.elven': " + std::string(message);
}

constexpr std::string_view SYLLABLE = "a syllable (lower-case letters a-z, possibly empty)";
constexpr std::string_view PATTERN = "a name pattern (O, N, C, ' and -, with at least one N)";
constexpr std::string_view FORM = "a name form (letters, spaces, ' and -, with exactly one {root})";

}  // namespace

TEST_SUITE("history-names-data") {

TEST_CASE("P3-02 5: the shipped naming blocks equal section 5, read through culture_naming_from") {
    const wd::LoadResult<wd::ContentSet> r = wd::load_content(wd::builtin_kinds(), data_root());
    std::string errs;
    for (const wd::LoadError& e : r.errors) errs += e.to_string() + "\n";
    INFO(errs);
    REQUIRE(r.ok());
    const wd::KindContent* kind = r.value->find_kind("culture");
    REQUIRE(kind != nullptr);
    REQUIRE(kind->definitions.size() == 6);
    for (std::size_t i = 0; i < 6; ++i) {
        const std::string_view id = names_test::CULTURE_IDS[i];
        CAPTURE(id);
        // Canonical id order is the culture number order of 4.6.
        CHECK(kind->definitions[i].id == id);
        const wd::Definition* def = r.value->find("culture", id);
        REQUIRE(def != nullptr);
        const Naming n = names_api::naming_from_record(def->record.get_record("naming"));
        CHECK(n == shipped_naming(id));
    }
}

TEST_CASE("P3-02 3: the in-memory copy of the shipped root and the test's file form load cleanly") {
    // The baseline of the failure cases below: replacing elven.toml by the
    // test's own rendering of section 5's elven block changes nothing.
    const Loaded l = load_with_elven(shipped_naming("elven"));
    INFO(l.errors);
    CHECK(l.ok);
}

TEST_CASE("P3-02 9.1 test 7: the named checks of section 3 fail once each with their messages") {
    Naming n = shipped_naming("elven");
    n.nuclei[1] = "";
    check_fails(n, check_text("naming_nuclei_nonempty", "naming.nuclei[1] must not be empty"));

    n = shipped_naming("elven");
    n.min_letters = 12;
    n.max_letters = 11;
    check_fails(n, check_text("naming_letters_ordered", "naming.min_letters must not be above naming.max_letters"));
    // Equal bounds are allowed.
    n.min_letters = 11;
    CHECK(load_with_elven(n).ok);

    n = shipped_naming("elven");
    n.forbidden.push_back("");
    check_fails(n, check_text("naming_forbidden_nonempty",
                              "naming.forbidden[" + std::to_string(n.forbidden.size() - 1) + "] must not be empty"));
    // An empty forbidden list is allowed (array(..., 0, 32)).
    n.forbidden.clear();
    CHECK(load_with_elven(n).ok);
}

TEST_CASE("P3-02 9.1 test 7: the grammar syllable refuses upper case, digits and more than 8 bytes; allows \"\"") {
    Naming n = shipped_naming("elven");
    n.onsets[1] = "L";
    check_fails(n, SYLLABLE);
    n = shipped_naming("elven");
    n.codas[2] = "l1";
    check_fails(n, SYLLABLE);
    n = shipped_naming("elven");
    n.forbidden[0] = "a'e";
    check_fails(n, SYLLABLE);
    n = shipped_naming("elven");
    n.onsets[1] = "abcdefghi";  // 9 bytes
    check_fails(n, SYLLABLE);
    n.onsets[1] = "abcdefgh";  // 8 bytes
    CHECK(load_with_elven(n).ok);
    n = shipped_naming("elven");
    n.codas = {""};  // only the empty coda
    CHECK(load_with_elven(n).ok);
}

TEST_CASE("P3-02 9.1 test 7: the grammar name_pattern") {
    for (const std::string bad : {"OC", "'ON", "ON-", "ON'-NC", "ONX", "on", "ONC NC", "ON--N"}) {
        CAPTURE(bad);
        Naming n = shipped_naming("elven");
        n.patterns[0] = bad;
        check_fails(n, PATTERN);
    }
    for (const std::string good : {"N", "N'N", "ONC-ONC", "NNNNNNNNNNNNNNNN"}) {
        CAPTURE(good);
        Naming n = shipped_naming("elven");
        n.patterns[0] = good;
        CHECK(load_with_elven(n).ok);
    }
    // 17 symbols exceed string(16, ...).
    Naming n = shipped_naming("elven");
    n.patterns[0] = std::string(17, 'N');
    const Loaded l = load_with_elven(n);
    CHECK_FALSE(l.ok);
}

TEST_CASE("P3-02 9.1 test 7: the grammar name_form") {
    for (const std::string bad : {"The Court", "{root} {root}", " {root}", "{root} ", "The  {root}", "{root}1",
                                  "{root}, of", "{Root}", "{root"}) {
        CAPTURE(bad);
        Naming n = shipped_naming("elven");
        n.civilization_forms[0] = bad;
        check_fails(n, FORM);
    }
    for (const std::string good : {"{root}", "The {root}'s Reach", "Old-{root}", "{root}dell"}) {
        CAPTURE(good);
        Naming n = shipped_naming("elven");
        n.settlement_forms[0] = good;
        CHECK(load_with_elven(n).ok);
    }
    // At most 48 bytes.
    Naming n = shipped_naming("elven");
    n.god_forms[0] = "{root}" + std::string(43, 'x');  // 49 bytes
    CHECK_FALSE(load_with_elven(n).ok);
    n.god_forms[0] = "{root}" + std::string(42, 'x');  // 48 bytes
    CHECK(load_with_elven(n).ok);
}

TEST_CASE("P3-02 3: array bounds of the naming fields") {
    Naming n = shipped_naming("elven");
    n.nuclei.clear();
    CHECK_FALSE(load_with_elven(n).ok);
    n = shipped_naming("elven");
    n.patterns.assign(17, "ON");
    CHECK_FALSE(load_with_elven(n).ok);
    n.patterns.assign(16, "ON");
    CHECK(load_with_elven(n).ok);
    n = shipped_naming("elven");
    n.god_forms.assign(9, "{root}");
    CHECK_FALSE(load_with_elven(n).ok);
    n.god_forms.assign(8, "{root}");
    CHECK(load_with_elven(n).ok);
    n = shipped_naming("elven");
    n.forbidden.assign(33, "x");
    CHECK_FALSE(load_with_elven(n).ok);
    n = shipped_naming("elven");
    n.min_letters = 0;
    CHECK_FALSE(load_with_elven(n).ok);
    n = shipped_naming("elven");
    n.max_letters = 33;
    CHECK_FALSE(load_with_elven(n).ok);
}

}  // TEST_SUITE
