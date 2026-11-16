// P0-03 TOML loader: file discovery and the data root (spec P0-03 sections
// 5, 7.2, 7.6, 9, 11, 13 test 3, amendments 7, 9 and 13).
//
// Every discovery case runs from an in-memory data root (amendment 13).
// The committed fixtures under worldgen/tests/data/loader/ cover the cases
// git can carry on every platform; the cases it cannot (upper-case names,
// directories named like files, symbolic links, CRLF bytes, a byte-order
// mark, invalid UTF-8, an oversized file) are built at run time in a fresh
// directory under the system temporary directory, removed afterwards.

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "data_loader_support.hpp"

using namespace loader_test;

namespace {

wd::LoadResult<wd::ContentSet> load_memory(const wd::MemoryDataRoot& root) {
    return wd::load_content_from_memory(alpha_beta_registry(), root);
}

wd::LoadResult<wd::ContentSet> load_disk(const std::filesystem::path& root) {
    return wd::load_content(alpha_beta_registry(), root);
}

std::string alpha_def_text(std::string_view id, int count) {
    return "[alpha." + std::string(id) + "]\ncount = " + std::to_string(count) + "\npartner = \"b1\"\n";
}

std::vector<std::string> alpha_ids(const wd::LoadResult<wd::ContentSet>& r) {
    std::vector<std::string> ids;
    REQUIRE(r.ok());
    const wd::KindContent* k = r.value->find_kind("alpha");
    REQUIRE(k != nullptr);
    for (const auto& d : k->definitions) {
        ids.push_back(d.id);
    }
    return ids;
}

// Exactly one error, at `path`; returns it.
wd::LoadError single_error_at(const wd::LoadResult<wd::ContentSet>& r, std::string_view path) {
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].path == path);
    return r.errors[0];
}

// The entries of the section 13 discovery case: b.toml, a.toml, .hidden.toml,
// notes.md, c.toml~ (the last three hold text that is not valid TOML).
std::vector<wd::MemoryEntry> discovery_entries() {
    return {file("b.toml", alpha_def_text("from_b", 2)), file("a.toml", alpha_def_text("from_a", 1)),
            file(".hidden.toml", "this is not TOML\n"), file("notes.md", "# not = = TOML\n"),
            file("c.toml~", "not = = TOML\n")};
}

// Creates a symbolic link; false where the platform refuses (Windows
// without the privilege). On other platforms a failure fails the test.
bool try_symlink(const std::filesystem::path& target, const std::filesystem::path& link, bool directory) {
    std::error_code ec;
    if (directory) {
        std::filesystem::create_directory_symlink(target, link, ec);
    } else {
        std::filesystem::create_symlink(target, link, ec);
    }
#ifdef _WIN32
    if (ec) {
        MESSAGE("skipped: cannot create a symbolic link here (" << ec.message()
                                                                 << "); the in-memory cases still run");
        return false;
    }
#else
    REQUIRE_MESSAGE(!ec, "create symlink: " << ec.message());
#endif
    return true;
}

}  // namespace

// --- Test 3 in memory ---------------------------------------------------------

TEST_CASE("loader discovery: only well-named .toml files are loaded, in byte order") {
    auto root = alphas_root(discovery_entries());
    const auto r = load_memory(root);
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    CHECK(alpha_ids(r) == std::vector<std::string>{"from_a", "from_b"});
    CHECK(r.value->find("beta", "b1") != nullptr);

    // a.toml is parsed before b.toml whatever the listing order: the same id
    // in both is reported at b.toml, the second in file-name order.
    auto dup = alphas_root({file("b.toml", alpha_def_text("dup", 2)), file("a.toml", alpha_def_text("dup", 1)),
                            file(".hidden.toml", alpha_def_text("dup", 3)), file("c.toml~", alpha_def_text("dup", 4))});
    const auto e = single_error_at(load_memory(dup), "alphas/b.toml");
    CHECK(e.line == 1);
    CHECK(contains(e.message, "alphas/a.toml"));
}

TEST_CASE("loader discovery: badly named .toml entries are errors") {
    for (const char* name : {"Upper.toml", "my races.toml", "-a.toml", "_a.toml", "a.b.toml", "aB.toml",
                             "\xC3\xA9.toml", "a+b.toml", ".toml.toml"}) {
        INFO(std::string(name));
        const std::string n = name;
        if (n.front() == '.') {
            // Starts with '.': ignored, not an error.
            CHECK(load_memory(alphas_root({file(n, "garbage = = \n")})).ok());
            continue;
        }
        single_error_at(load_memory(alphas_root({file(n, alpha_def_text("x", 1))})), "alphas/" + n);
    }
    for (const char* name : {"0.toml", "a-b_c.toml", "9lives.toml", "a.toml", "z--.toml"}) {
        INFO(std::string(name));
        const auto r = load_memory(alphas_root({file(name, alpha_def_text("x", 1))}));
        INFO(error_dump(r));
        CHECK(r.ok());
    }
}

TEST_CASE("loader discovery: names that do not end in .toml, or start with '.', are ignored") {
    for (const char* name : {"NAME.TOML", "a.TOML", "a.Toml", "a.toml~", "a.toml.bak", "a.tom", "README.md",
                             ".toml", ".a.toml", ".Upper.toml", "a", "toml"}) {
        INFO(std::string(name));
        const auto r = load_memory(alphas_root({file(name, "garbage = = \n")}));
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(alpha_ids(r).empty());
    }
    // Directories that would be ignored as files are ignored as well.
    for (const char* name : {".git", "old", "x.toml.d", ".x.toml"}) {
        INFO(std::string(name));
        CHECK(load_memory(alphas_root({subdir(name)})).ok());
    }
}

TEST_CASE("loader discovery: a directory named like a data file is an error") {
    const auto e = single_error_at(load_memory(alphas_root({subdir("x.toml")})), "alphas/x.toml");
    CHECK(e.line == 0);
    // Both not a regular file and badly named: one error, the "not a
    // regular file" one (amendment 7). The amendment names the error, not
    // its exact text, so the check is for the words "regular file", which
    // a naming error would not use.
    const auto e2 = single_error_at(load_memory(alphas_root({subdir("Old.toml")})), "alphas/Old.toml");
    INFO(e2.to_string());
    CHECK(contains(e2.message, "regular file"));
}

TEST_CASE("loader discovery: missing and empty kind directories") {
    SUBCASE("missing alphas (not required): zero definitions, no error") {
        wd::MemoryDataRoot root;
        root.directories.push_back(dir("betas", {file("b.toml", std::string(BETA_B1))}));
        const auto r = load_memory(root);
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(alpha_ids(r).empty());
        // The kind is still present in the content set.
        REQUIRE(r.value->kinds.size() == 2);
        CHECK(r.value->kinds[0].kind == "alpha");
        CHECK(r.value->kinds[1].kind == "beta");
    }
    SUBCASE("missing betas (required): error") {
        wd::MemoryDataRoot root;
        root.directories.push_back(dir("alphas", {}));
        const auto e = single_error_at(load_memory(root), "betas");
        CHECK(e.line == 0);
    }
    SUBCASE("betas present but with zero definitions: error") {
        for (const auto& entries : {std::vector<wd::MemoryEntry>{}, std::vector<wd::MemoryEntry>{file("b.toml", "")},
                                    std::vector<wd::MemoryEntry>{file("b.toml", "[beta]\n")},
                                    std::vector<wd::MemoryEntry>{file("notes.md", std::string(BETA_B1))}}) {
            wd::MemoryDataRoot root;
            root.directories.push_back(dir("betas", entries));
            const auto r = load_memory(root);
            check_content_result(r);
            CHECK_FALSE(r.ok());
        }
    }
    SUBCASE("an empty alphas directory is fine") {
        const auto r = load_memory(alphas_root({}));
        INFO(error_dump(r));
        CHECK(r.ok());
    }
}

TEST_CASE("loader discovery: kind directories match by exact bytes (amendment 7)") {
    // `Alphas` is not `alphas`: ignored, so alpha has zero definitions.
    wd::MemoryDataRoot root;
    root.directories.push_back(dir("Alphas", {file("a.toml", "garbage = = \n")}));
    root.directories.push_back(dir("betas", {file("b.toml", std::string(BETA_B1))}));
    const auto r = load_memory(root);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    CHECK(alpha_ids(r).empty());

    // `Betas` is not `betas`: the required kind is missing.
    wd::MemoryDataRoot root2;
    root2.directories.push_back(dir("Betas", {file("b.toml", std::string(BETA_B1))}));
    single_error_at(load_memory(root2), "betas");
}

TEST_CASE("loader discovery: directories of unregistered kinds are not read") {
    auto root = alphas_root({file("a.toml", alpha_def_text("x", 1))});
    root.directories.push_back(dir("gammas", {file("g.toml", "garbage = = \n"), file("Upper.toml", ""),
                                              subdir("x.toml")}));
    const auto r = load_memory(root);
    INFO(error_dump(r));
    CHECK(r.ok());
}

TEST_CASE("loader invariant: the listing order never matters (section 11)") {
    // Valid set and failing set, each in every rotation of its entries and
    // with the directories in both orders.
    std::vector<wd::MemoryEntry> good = discovery_entries();
    std::vector<wd::MemoryEntry> bad = {file("b.toml", "[alpha.q]\ncount = 0.5\npartner = \"b1\"\n"),
                                        file("a.toml", "[alpha.p]\npartner = \"zz\"\n"), file("Upper.toml", ""),
                                        subdir("d.toml"), file("c.toml", "x = = 1\n")};
    for (auto* entries : {&good, &bad}) {
        const auto first = load_memory(alphas_root(*entries));
        check_content_result(first);
        for (std::size_t rot = 0; rot < entries->size(); ++rot) {
            std::vector<wd::MemoryEntry> e = *entries;
            std::rotate(e.begin(), e.begin() + static_cast<std::ptrdiff_t>(rot), e.end());
            std::reverse(e.begin(), e.end());
            auto root = alphas_root(e);
            std::swap(root.directories[0], root.directories[1]);
            const auto again = load_memory(root);
            check_content_result(again);
            CHECK(again.ok() == first.ok());
            CHECK(again.errors == first.errors);
            if (first.ok() && again.ok()) {
                CHECK(*again.value == *first.value);
            }
        }
    }
    // The failing set reports one error per bad file, with relative paths.
    const auto r = load_memory(alphas_root(bad));
    INFO(error_dump(r));
    REQUIRE_FALSE(r.ok());
    for (const char* p : {"alphas/Upper.toml", "alphas/a.toml", "alphas/b.toml", "alphas/c.toml", "alphas/d.toml"}) {
        bool found = false;
        for (const auto& e : r.errors) {
            found = found || e.path == p;
        }
        CHECK_MESSAGE(found, std::string(p));
    }
}

// --- The data root (7.6, amendment 9) ----------------------------------------

TEST_CASE("loader data root: no kinds registered means nothing is read") {
    const wd::KindRegistry empty;
    const auto r = wd::load_content(empty, utf8_path("this/data/root/does/not/exist"));
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    CHECK(r.value->kinds.empty());

    // The production registry holds the required kind `worldgen` (P1-00 6.1,
    // P0-03 amendment 17), so a missing root or no root is an error.
    const auto rb = wd::load_content(wd::builtin_kinds(), utf8_path("this/data/root/does/not/exist"));
    CHECK_FALSE(rb.ok());

    const auto rn = wd::load_content_without_data(wd::builtin_kinds());
    CHECK_FALSE(rn.ok());
}

TEST_CASE("loader data root: without a data root, required kinds are errors") {
    const auto r = wd::load_content_without_data(alpha_beta_registry());
    check_content_result(r);
    INFO(error_dump(r));
    CHECK_FALSE(r.ok());

    wd::KindRegistry solo;
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("n", wd::Type::integer(0, 1)));
    solo.add(wd::Kind{"solo", "solos", false, wd::Schema(std::move(fields))});
    const auto rs = wd::load_content_without_data(solo);
    check_content_result(rs);
    REQUIRE(rs.ok());
    REQUIRE(rs.value->kinds.size() == 1);
    CHECK(rs.value->kinds[0].kind == "solo");
    CHECK(rs.value->kinds[0].definitions.empty());
}

TEST_CASE("loader data root: a missing data root is an error when kinds are registered") {
    ScratchDir scratch;
    const auto r = load_disk(scratch.path() / utf8_path("missing"));
    check_content_result(r);
    INFO(error_dump(r));
    CHECK_FALSE(r.ok());
}

// --- Committed fixtures on disk ------------------------------------------------

TEST_CASE("loader discovery on disk: the section 13 fixture directory") {
    const auto r = load_disk(fixture("discovery"));
    check_content_result(r);
    INFO(error_dump(r));
    REQUIRE(r.ok());
    CHECK(alpha_ids(r) == std::vector<std::string>{"from_a", "from_b"});

    // The same id in a.toml and b.toml: reported at b.toml.
    const auto e = single_error_at(load_disk(fixture("discovery_order")), "alphas/b.toml");
    CHECK(e.line == 2);
    CHECK(e.column == 1);
    CHECK(contains(e.message, "alphas/a.toml"));

    // A gammas directory of garbage is never read.
    const auto ru = load_disk(fixture("unregistered"));
    INFO(error_dump(ru));
    CHECK(ru.ok());
}

TEST_CASE("loader discovery on disk agrees with the in-memory root") {
    auto root = alphas_root(discovery_entries());
    const auto mem = load_memory(root);
    const auto disk = load_disk(fixture("discovery"));
    REQUIRE(mem.ok());
    REQUIRE(disk.ok());
    CHECK(*mem.value == *disk.value);
}

// --- Cases built at run time outside the source tree --------------------------

TEST_CASE("loader discovery on disk: case sensitivity (amendment 13)") {
    ScratchDir scratch;
    scratch.make_dir("betas");
    scratch.write("betas/b.toml", BETA_B1);
    scratch.make_dir("alphas");

    SUBCASE("Upper.toml is an error") {
        scratch.write("alphas/Upper.toml", alpha_def_text("x", 1));
        single_error_at(load_disk(scratch.path()), "alphas/Upper.toml");
    }
    SUBCASE("NAME.TOML does not end in .toml and is ignored") {
        scratch.write("alphas/NAME.TOML", "garbage = = \n");
        scratch.write("alphas/a.toml", alpha_def_text("x", 1));
        const auto r = load_disk(scratch.path());
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(alpha_ids(r) == std::vector<std::string>{"x"});
    }
    SUBCASE("a kind directory Betas does not stand in for betas") {
        ScratchDir other;
        other.make_dir("Betas");
        other.write("Betas/b.toml", BETA_B1);
        single_error_at(load_disk(other.path()), "betas");
    }
    SUBCASE("a kind directory Alphas is not read") {
        ScratchDir other;
        other.make_dir("betas");
        other.write("betas/b.toml", BETA_B1);
        other.make_dir("Alphas");
        other.write("Alphas/a.toml", "garbage = = \n");
        const auto r = load_disk(other.path());
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(alpha_ids(r).empty());
    }
}

TEST_CASE("loader discovery on disk: symbolic links are rejected (amendment 13)") {
    ScratchDir scratch;
    scratch.make_dir("betas");
    scratch.write("betas/b.toml", BETA_B1);
    scratch.make_dir("alphas");
    scratch.make_dir("elsewhere");
    const auto target = scratch.write("elsewhere/real.toml", alpha_def_text("x", 1));

    SUBCASE("a data file that is a symbolic link") {
        if (!try_symlink(target, scratch.path() / utf8_path("alphas/link.toml"), false)) {
            return;
        }
        single_error_at(load_disk(scratch.path()), "alphas/link.toml");
    }
    SUBCASE("a dangling symbolic link") {
        if (!try_symlink(scratch.path() / utf8_path("elsewhere/none.toml"),
                         scratch.path() / utf8_path("alphas/dangling.toml"), false)) {
            return;
        }
        single_error_at(load_disk(scratch.path()), "alphas/dangling.toml");
    }
    SUBCASE("a kind directory that is a symbolic link") {
        ScratchDir other;
        other.make_dir("real_betas");
        other.write("real_betas/b.toml", BETA_B1);
        if (!try_symlink(other.path() / utf8_path("real_betas"), other.path() / utf8_path("betas"), true)) {
            return;
        }
        const auto r = load_disk(other.path());
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE_FALSE(r.ok());
        CHECK(r.errors[0].path == "betas");
    }
    SUBCASE("a symbolic link whose name is ignored stays ignored") {
        if (!try_symlink(target, scratch.path() / utf8_path("alphas/.link.toml"), false)) {
            return;
        }
        const auto r = load_disk(scratch.path());
        INFO(error_dump(r));
        CHECK(r.ok());
    }
}

TEST_CASE("loader discovery on disk: entries that are not regular files") {
    ScratchDir scratch;
    scratch.make_dir("betas");
    scratch.write("betas/b.toml", BETA_B1);
    scratch.make_dir("alphas");

    SUBCASE("a directory x.toml") {
        scratch.make_dir("alphas/x.toml");
        single_error_at(load_disk(scratch.path()), "alphas/x.toml");
    }
    SUBCASE("a directory Old.toml: one error, not a regular file") {
        scratch.make_dir("alphas/Old.toml");
        const auto e = single_error_at(load_disk(scratch.path()), "alphas/Old.toml");
        INFO(e.to_string());
        CHECK(contains(e.message, "regular file"));
    }
    SUBCASE("a kind directory that is a regular file") {
        ScratchDir other;
        other.write("alphas", "not a directory\n");
        other.make_dir("betas");
        other.write("betas/b.toml", BETA_B1);
        single_error_at(load_disk(other.path()), "alphas");
    }
}

TEST_CASE("loader parsing on disk: CRLF, byte-order mark, invalid UTF-8 and size") {
    const std::string text = "[alpha.x]\ncount = 4\npartner = \"b1\"\nlabel = \"\"\"\nline one\nline two\"\"\"\n";
    const auto expected = load_memory(alphas_root({file("a.toml", text)}));
    REQUIRE(expected.ok());

    ScratchDir scratch;
    scratch.make_dir("betas");
    scratch.write("betas/b.toml", to_crlf(std::string(BETA_B1)));
    scratch.make_dir("alphas");

    SUBCASE("CRLF and a byte-order mark read the same as LF") {
        scratch.write("alphas/a.toml", "\xEF\xBB\xBF" + to_crlf(text));
        const auto r = load_disk(scratch.path());
        check_content_result(r);
        INFO(error_dump(r));
        REQUIRE(r.ok());
        CHECK(*r.value == *expected.value);
        CHECK(wd::content_hash(*r.value) == wd::content_hash(*expected.value));
    }
    SUBCASE("invalid UTF-8") {
        scratch.write("alphas/a.toml", "[alpha.x]\ncount = 4\nlabel = \"\xC3\x28\"\npartner = \"b1\"\n");
        const auto e = single_error_at(load_disk(scratch.path()), "alphas/a.toml");
        CHECK(e.line == 3);
        CHECK(e.column >= 1);
    }
    SUBCASE("1 MiB is accepted, 1 MiB + 1 byte is not") {
        const std::size_t mib = std::size_t{1} << 20;
        const std::string head = alpha_def_text("x", 1);
        scratch.write("alphas/a.toml", head + "#" + std::string(mib - head.size() - 2, 'x') + "\n");
        const auto r_ok = load_disk(scratch.path());
        INFO(error_dump(r_ok));
        CHECK(r_ok.ok());
        scratch.write("alphas/a.toml", head + "#" + std::string(mib - head.size() - 1, 'x') + "\n");
        single_error_at(load_disk(scratch.path()), "alphas/a.toml");
    }
}
