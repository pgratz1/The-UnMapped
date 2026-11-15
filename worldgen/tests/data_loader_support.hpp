#pragma once
// Shared helpers for the P0-03 TOML loader tests (spec P0-03 section 13).
//
// The test-only kinds `alpha` and `beta` exactly as section 13 defines them,
// builders for in-memory data roots, the result invariants of section 11,
// the committed fixture directory, and a scratch directory outside the
// source tree for the on-disk cases that must not be committed.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/data.hpp"

#ifndef UNMAPPED_LOADER_FIXTURE_DIR
#error "UNMAPPED_LOADER_FIXTURE_DIR must give the path of worldgen/tests/data/loader"
#endif

namespace loader_test {

namespace wd = worldgen::data;

// --- The test kinds of P0-03 section 13 ------------------------------------

// alpha (directory alphas, not required):
//   count int(0, 100) required; ratio fx(0, 1) default "0.5";
//   enabled bool default true; label string(32) default "";
//   mode enum(fast, slow) default fast; tags array(string(16), 0, 4) default [];
//   partner ref(beta) required.
inline wd::Kind alpha_kind() {
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("count", wd::Type::integer(0, 100)));
    fields.push_back(wd::Field::optional("ratio", wd::Type::fixed("0", "1"),
                                         wd::Value::fixed(wd::decimal_literal("0.5"))));
    fields.push_back(wd::Field::optional("enabled", wd::Type::boolean(), wd::Value::boolean(true)));
    fields.push_back(wd::Field::optional("label", wd::Type::string(32), wd::Value::string("")));
    fields.push_back(wd::Field::optional("mode", wd::Type::enumeration({"fast", "slow"}),
                                         wd::Value::string("fast")));
    fields.push_back(wd::Field::optional("tags", wd::Type::array(wd::Type::string(16), 0, 4),
                                         wd::Value::array({})));
    fields.push_back(wd::Field::required("partner", wd::Type::reference("beta")));
    wd::Kind k;
    k.name = "alpha";
    k.directory = "alphas";
    k.required = false;
    k.schema = wd::Schema(std::move(fields));
    return k;
}

// beta (directory betas, required): weight int(1, 1000) required.
inline wd::Kind beta_kind() {
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("weight", wd::Type::integer(1, 1000)));
    wd::Kind k;
    k.name = "beta";
    k.directory = "betas";
    k.required = true;
    k.schema = wd::Schema(std::move(fields));
    return k;
}

// Registry with alpha and beta (added in reverse order on purpose: the
// registry keeps ascending name order itself).
inline wd::KindRegistry alpha_beta_registry() {
    wd::KindRegistry r;
    r.add(beta_kind());
    r.add(alpha_kind());
    return r;
}

// --- In-memory data roots ---------------------------------------------------

inline wd::MemoryEntry file(std::string name, std::string bytes) {
    return wd::MemoryEntry{std::move(name), std::move(bytes), false};
}

inline wd::MemoryEntry subdir(std::string name) {
    return wd::MemoryEntry{std::move(name), std::string{}, true};
}

inline wd::MemoryDirectory dir(std::string name, std::vector<wd::MemoryEntry> entries) {
    return wd::MemoryDirectory{std::move(name), std::move(entries)};
}

// The beta file most tests need: one definition b1 with weight 7.
inline constexpr std::string_view BETA_B1 = "[beta.b1]\nweight = 7\n";

// A data root with the given alphas files and betas/b.toml holding BETA_B1.
inline wd::MemoryDataRoot alphas_root(std::vector<wd::MemoryEntry> alpha_files) {
    wd::MemoryDataRoot root;
    root.directories.push_back(dir("alphas", std::move(alpha_files)));
    root.directories.push_back(dir("betas", {file("b.toml", std::string(BETA_B1))}));
    return root;
}

// Loads a single alphas/a.toml with the given text (plus betas/b.toml).
inline wd::LoadResult<wd::ContentSet> load_alpha_text(std::string text) {
    return wd::load_content_from_memory(alpha_beta_registry(),
                                        alphas_root({file("a.toml", std::move(text))}));
}

// --- Result invariants (P0-03 sections 3, 9 and 11) ------------------------

// Strictly ascending ids within each kind, kinds in ascending name order.
inline void check_content_order(const wd::ContentSet& content) {
    for (std::size_t k = 0; k < content.kinds.size(); ++k) {
        if (k > 0) {
            CHECK(content.kinds[k - 1].kind < content.kinds[k].kind);
        }
        const auto& defs = content.kinds[k].definitions;
        for (std::size_t i = 1; i < defs.size(); ++i) {
            CHECK(defs[i - 1].id < defs[i].id);  // std::string compares bytes
        }
    }
}

// Either a complete value and no errors, or no value and a non-empty error
// list that is sorted by (path, line, column, message) and has no duplicates.
template <typename T>
void check_result_shape(const wd::LoadResult<T>& r) {
    CHECK(r.ok() == r.value.has_value());
    if (r.value.has_value()) {
        CHECK(r.errors.empty());
    } else {
        CHECK_FALSE(r.errors.empty());
    }
    for (std::size_t i = 1; i < r.errors.size(); ++i) {
        const auto& a = r.errors[i - 1];
        const auto& b = r.errors[i];
        CHECK(wd::error_less(a, b));
        CHECK_FALSE(a == b);
    }
}

// The same plus the content order, for content loads.
inline void check_content_result(const wd::LoadResult<wd::ContentSet>& r) {
    check_result_shape(r);
    if (r.value.has_value()) {
        check_content_order(*r.value);
    }
}

// All errors, one per line, for failure messages.
template <typename T>
std::string error_dump(const wd::LoadResult<T>& r) {
    std::string out;
    for (const auto& e : r.errors) {
        out += e.to_string();
        out += '\n';
    }
    return out;
}

inline bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// The single alpha definition `id` of a successful load.
inline const wd::Definition& alpha_def(const wd::LoadResult<wd::ContentSet>& r, std::string_view id) {
    REQUIRE(r.ok());
    const wd::Definition* d = r.value->find("alpha", id);
    REQUIRE(d != nullptr);
    return *d;
}

// --- Paths ------------------------------------------------------------------

// A filesystem path from UTF-8 text (portable: no ANSI code page on Windows).
inline std::filesystem::path utf8_path(std::string_view text) {
    std::u8string u;
    u.reserve(text.size());
    for (char c : text) {
        u.push_back(static_cast<char8_t>(static_cast<unsigned char>(c)));
    }
    return std::filesystem::path(u);
}

// The committed fixtures, worldgen/tests/data/loader/ in the source tree.
// Read only: tests never write here.
inline std::filesystem::path fixture_dir() {
    return utf8_path(UNMAPPED_LOADER_FIXTURE_DIR);
}

inline std::filesystem::path fixture(std::string_view relative) {
    return fixture_dir() / utf8_path(relative);
}

// A fresh empty directory under the system temporary directory (never the
// source tree), removed with everything in it on destruction. Names are
// picked by trying a fixed sequence, so no clock or process id is involved
// and concurrent runs simply take different numbers.
class ScratchDir {
public:
    ScratchDir() {
        std::error_code ec;
        const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
        REQUIRE_FALSE(ec);
        for (int i = 0; i < 10000; ++i) {
            std::filesystem::path candidate =
                base / utf8_path("unmapped-loader-test-" + std::to_string(i));
            if (std::filesystem::create_directory(candidate, ec) && !ec) {
                path_ = std::move(candidate);
                return;
            }
        }
        FAIL("could not create a scratch directory under " << base.string());
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;
    ~ScratchDir() {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    // Creates directory `relative` (and parents).
    std::filesystem::path make_dir(std::string_view relative) const {
        const std::filesystem::path p = path_ / utf8_path(relative);
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        REQUIRE_FALSE(ec);
        return p;
    }

    // Writes `bytes` exactly (binary) to `relative`.
    std::filesystem::path write(std::string_view relative, std::string_view bytes) const {
        const std::filesystem::path p = path_ / utf8_path(relative);
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        REQUIRE(out.good());
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        REQUIRE(out.good());
        return p;
    }

private:
    std::filesystem::path path_;
};

// Replaces every LF in text with CR LF.
inline std::string to_crlf(std::string_view text) {
    std::string out;
    out.reserve(text.size() + text.size() / 8);
    for (char c : text) {
        if (c == '\n') {
            out += '\r';
        }
        out += c;
    }
    return out;
}

}  // namespace loader_test
