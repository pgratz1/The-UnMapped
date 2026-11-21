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
//
// From generator 0.17.0 (P3-08, P3-02 r5 11.2 and 11.4): the naming blocks
// hold god_patterns and the elven House form, kind name_filter is required,
// and the tests of 11.7 items 1 to 3 are here. They fail until the loader
// knows the kind and the field.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
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
    if (n.god_patterns.has_value()) o << "god_patterns = " << toml_array(*n.god_patterns) << "\n";  // r5 (11.4)
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

// --- r5 (P3-02 11.2, 11.4, 11.7 items 1-3; generator 0.17.0) ---------------------------------------

namespace {

// The shipped root in memory with name_filters/ replaced by `files` (name -> text).
wd::LoadResult<wd::ContentSet> load_with_filters(const std::vector<std::pair<std::string, std::string>>& files) {
    wd::MemoryDataRoot root = shipped_in_memory();
    bool replaced = false;
    for (wd::MemoryDirectory& d : root.directories) {
        if (d.name != "name_filters") continue;
        d.entries.clear();
        for (const auto& [name, text] : files) d.entries.push_back(wd::MemoryEntry{name, text, false});
        replaced = true;
    }
    REQUIRE(replaced);
    return wd::load_content_from_memory(wd::builtin_kinds(), root);
}

std::string filter_toml(std::string_view id, const std::vector<std::string>& anywhere,
                        const std::vector<std::string>& at_start, const std::vector<std::string>& whole) {
    return "[name_filter." + std::string(id) + "]\nanywhere = " + toml_array(anywhere) + "\nat_start = " +
           toml_array(at_start) + "\nwhole = " + toml_array(whole) + "\n";
}

std::string errors_of(const wd::LoadResult<wd::ContentSet>& r) {
    std::string out;
    for (const wd::LoadError& e : r.errors) out += e.to_string() + "\n";
    return out;
}

void filters_fail(const std::vector<std::pair<std::string, std::string>>& files, std::string_view mention) {
    const wd::LoadResult<wd::ContentSet> r = load_with_filters(files);
    const std::string e = errors_of(r);
    INFO(e);
    CHECK_FALSE(r.ok());
    CHECK(r.errors.size() == 1U);
    CHECK(e.find(std::string(mention)) != std::string::npos);
}

}  // namespace

TEST_SUITE("history-names-data") {

TEST_CASE("P3-02 11.7 item 1: the shipped name_filters/ loads: four definitions, the effective lists of 11.2") {
    const wd::LoadResult<wd::ContentSet> r = wd::load_content(wd::builtin_kinds(), data_root());
    INFO(errors_of(r));
    REQUIRE(r.ok());
    const wd::KindContent* kind = r.value->find_kind("name_filter");
    REQUIRE(kind != nullptr);
    REQUIRE(kind->definitions.size() == 4U);
    const std::vector<std::string_view> ids = {"crude", "famous", "slurs", "words"};
    for (std::size_t i = 0; i < 4; ++i) CHECK(kind->definitions[i].id == ids[i]);
    // Sizes of 11.2 per definition: anywhere, at_start, whole.
    // words: 389 / 5 / 1 and crude 70 anywhere from P3-02 r10 12.1 (0.19.0; 393 whole from r8 11.11, 647 at 0.17.0)
    const std::vector<std::array<std::size_t, 3>> sizes = {{70, 16, 14}, {0, 0, 101}, {33, 19, 6}, {5, 1, 389}};
    for (std::size_t i = 0; i < 4; ++i) {
        CAPTURE(ids[i]);
        const wd::Record& rec = kind->definitions[i].record;
        CHECK(rec.get_array("anywhere").size() == sizes[i][0]);
        CHECK(rec.get_array("at_start").size() == sizes[i][1]);
        CHECK(rec.get_array("whole").size() == sizes[i][2]);
    }
    // The library's effective filter (name_filter_from) is the union of the files, read by the test.
    const names_api::Filter lib = names_api::filter_from_content(*r.value);
    const names_api::Filter& mine = names_test::shipped_filter();
    using Set = std::set<std::string>;
    CHECK(Set(lib.anywhere.begin(), lib.anywhere.end()) == Set(mine.anywhere.begin(), mine.anywhere.end()));
    CHECK(Set(lib.at_start.begin(), lib.at_start.end()) == Set(mine.at_start.begin(), mine.at_start.end()));
    CHECK(Set(lib.whole.begin(), lib.whole.end()) == Set(mine.whole.begin(), mine.whole.end()));
}

TEST_CASE("P3-02 r8 11.11.5 item 5: the anchors of the short word list with the shipped filter") {
    // 11.11.1: ordinary words that fit the setting pass as bare root words; the listed slips and comic or
    // modern words are rejected. The words are written plainly; none is offensive.
    const wd::LoadResult<wd::ContentSet> r = wd::load_content(wd::builtin_kinds(), data_root());
    INFO(errors_of(r));
    REQUIRE(r.ok());
    const names_api::Filter lib = names_api::filter_from_content(*r.value);
    for (const char* w : {"gold", "thorn", "moss", "iron", "ash", "grub", "sleet", "horn"}) {
        CAPTURE(std::string_view(w));
        CHECK_FALSE(names_api::name_filtered(lib, w));
    }
    for (const char* w : {"also", "went", "movie", "dork", "zit", "old", "mold"}) {
        CAPTURE(std::string_view(w));
        CHECK(names_api::name_filtered(lib, w));
    }
}

TEST_CASE("P3-02 11.7 item 1: every shipped name_filter list is sorted; the fixture copies are byte-identical") {
    const std::filesystem::path dir = data_root() / "name_filters";
    std::vector<std::filesystem::path> files;
    for (const auto& e : std::filesystem::directory_iterator(dir)) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 4U);
    for (const auto& f : files) {
        CAPTURE(f.filename().string());
        const names_api::Filter one = names_test::read_filter_dir_file(f);
        for (const std::vector<std::string>* l : {&one.anywhere, &one.at_start, &one.whole}) {
            CHECK(std::is_sorted(l->begin(), l->end()));
        }
        for (const char* fixture : {"worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
            CAPTURE(fixture);
            const std::filesystem::path copy =
                std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / fixture / "name_filters" / f.filename();
            CHECK(std::filesystem::is_regular_file(copy));
            CHECK(read_file(copy) == read_file(f));
        }
    }
}

TEST_CASE("P3-02 11.7 item 1: a data root without name_filters/ is a required-kind error") {
    wd::MemoryDataRoot root = shipped_in_memory();
    root.directories.erase(std::remove_if(root.directories.begin(), root.directories.end(),
                                          [](const wd::MemoryDirectory& d) { return d.name == "name_filters"; }),
                           root.directories.end());
    const wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(wd::builtin_kinds(), root);
    INFO(errors_of(r));
    REQUIRE_FALSE(r.ok());
    bool found = false;
    for (const wd::LoadError& e : r.errors) {
        found = found || (e.path == "name_filters" && e.line == 0U &&
                          e.message.find("required kind 'name_filter'") != std::string::npos);
    }
    CHECK(found);
}

TEST_CASE("P3-02 11.7 item 2: the grammar filter_word and the three checks of 11.2, each once with its message") {
    // The kind checks of 11.2 are reported at the offending definition (the first entry's, by 11.2's
    // order), as P3-01's race_order_unique is: 11.2 gives the messages, not the position (integration
    // reading, 2026-11-20).
    const std::string desc = "lower-case letters a-z, 2 to 16 of them";
    for (const std::string bad : {"Ni", "a", "ab1"}) {
        CAPTURE(bad);
        filters_fail({{"x.toml", filter_toml("x", {bad}, {}, {})}}, desc);
    }
    // 17 letters: outside string(16, filter_word).
    {
        const wd::LoadResult<wd::ContentSet> r = load_with_filters({{"x.toml", filter_toml("x", {std::string(17, 'q')}, {}, {})}});
        INFO(errors_of(r));
        CHECK_FALSE(r.ok());
        CHECK(errors_of(r).find("name_filter.x.anywhere[0]") != std::string::npos);
    }
    // Two-letter and sixteen-letter entries load.
    {
        const wd::LoadResult<wd::ContentSet> r =
            load_with_filters({{"x.toml", filter_toml("x", {"qq", std::string(16, 'q')}, {}, {})}});
        INFO(errors_of(r));
        CHECK(r.ok());
    }
    filters_fail({{"x.toml", filter_toml("x", {"qa", "qb", "qa"}, {}, {})}},
                 "error: check 'name_filter_unique' failed for 'name_filter.x': anywhere[2] 'qa' repeats anywhere[0]");
    filters_fail({{"a.toml", filter_toml("a", {"qx"}, {}, {})}, {"b.toml", filter_toml("b", {"qx"}, {}, {})}},
                 "error: check 'name_filter_distinct' failed for 'name_filter.b': anywhere 'qx' is in 'a' and 'b'");
    filters_fail({{"a.toml", filter_toml("a", {"qb"}, {"zqb"}, {})}},
                 "error: check 'name_filter_not_redundant' failed for 'name_filter.a': "
                 "'a' at_start 'zqb' is already rejected by anywhere 'qb' of 'a'");
    filters_fail({{"a.toml", filter_toml("a", {}, {}, {"qdx"})}, {"b.toml", filter_toml("b", {}, {"qd"}, {})}},
                 "error: check 'name_filter_not_redundant' failed for 'name_filter.a': "
                 "'a' whole 'qdx' is already rejected by at_start 'qd' of 'b'");
    // All three lists empty: loads, and the filter rejects nothing.
    const wd::LoadResult<wd::ContentSet> r = load_with_filters({{"x.toml", filter_toml("x", {}, {}, {})}});
    INFO(errors_of(r));
    CHECK(r.ok());
}

TEST_CASE("P3-02 11.7 item 3: naming_realm_forms and god_patterns' grammar, bounds and presence") {
    Naming n = shipped_naming("elven");
    n.civilization_forms = {"{root}"};
    check_fails(n, check_text("naming_realm_forms", "naming.civilization_forms[0] must hold a word besides {root}"));
    n = shipped_naming("elven");
    n.civilization_forms = {"The Court of {root}", "{root}ia"};
    check_fails(n, check_text("naming_realm_forms", "naming.civilization_forms[1] must hold a word besides {root}"));
    n = shipped_naming("elven");
    n.god_patterns.reset();  // the field missing
    {
        const Loaded l = load_with_elven(n);
        INFO(l.errors);
        CHECK_FALSE(l.ok);
        CHECK(l.errors.find("god_patterns") != std::string::npos);
    }
    n = shipped_naming("elven");
    n.god_patterns = std::vector<std::string>{};  // array(..., 1, 16)
    CHECK_FALSE(load_with_elven(n).ok);
    n.god_patterns = std::vector<std::string>(17, "ON");
    CHECK_FALSE(load_with_elven(n).ok);
    n.god_patterns = std::vector<std::string>(16, "ON");
    CHECK(load_with_elven(n).ok);
    for (const std::string bad : {"OC", "'ON", "ONX"}) {
        CAPTURE(bad);
        n = shipped_naming("elven");
        (*n.god_patterns)[0] = bad;
        check_fails(n, PATTERN);
    }
}

}  // TEST_SUITE
