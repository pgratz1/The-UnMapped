// Output locations and file writing (spec P0-04 sections 4.3, 4.4).

#include "output.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>

#include "platform.hpp"
#include "worldgen/db/utf8_path.hpp"

namespace cli {

namespace fs = std::filesystem;
using worldgen::db::ErrorCode;
using worldgen::db::path_from_utf8;
using worldgen::db::path_to_utf8;

Exit exit_for(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::output_exists:
        case ErrorCode::output_parent_missing:
        case ErrorCode::output_write_failed:
        case ErrorCode::output_rename_failed:
            return Exit::output;
        case ErrorCode::out_of_memory:
        case ErrorCode::sql_rejected:
            return Exit::internal;
        case ErrorCode::file_unreadable:
        case ErrorCode::not_sqlite:
        case ErrorCode::not_world:
        case ErrorCode::unsupported_schema_version:
        case ErrorCode::meta_key_missing:
        case ErrorCode::meta_value_invalid:
        case ErrorCode::layer_not_found:
        case ErrorCode::corrupt_layer:
        case ErrorCode::sqlite_error:
        case ErrorCode::table_not_found:
        case ErrorCode::no_primary_key:
        case ErrorCode::real_value:
            return Exit::input;
    }
    return Exit::internal;
}

void fail_with(const worldgen::db::Error& error) {
    if (error.code == ErrorCode::output_exists) {
        // The writer's wording speaks of "allowing overwriting"; say how.
        throw Failure(Exit::output, error.message + " (pass --force to replace it)");
    }
    throw Failure(exit_for(error.code), error.message);
}

namespace {

constexpr std::string_view ROOT_VARIABLE = "UNMAPPED_BUILD_ROOT";

// The variable's value, or std::nullopt if it is unset or empty. A value
// that is not valid UTF-16 (Windows) is a usage error, never "unset", so a
// malformed UNMAPPED_BUILD_ROOT cannot silently fall back to another root.
std::optional<std::string> variable(const char* name) {
    platform::EnvironmentValue env = platform::environment_utf8(name);
    if (env.invalid) {
        throw Failure(Exit::usage, std::string(name) + " is not valid UTF-16; fix it or pass --out");
    }
    if (env.value && env.value->empty()) {
        return std::nullopt;
    }
    return std::move(env.value);
}

// The build root (P0-04 4.3), or a usage error.
fs::path build_root() {
    if (const std::optional<std::string> root = variable(ROOT_VARIABLE.data())) {
        const fs::path p = path_from_utf8(*root);
        if (!p.is_absolute()) {
            throw Failure(Exit::usage, "UNMAPPED_BUILD_ROOT is '" + *root +
                                           "', which is not an absolute path; set it to an absolute path or pass --out");
        }
        return p;
    }
#if defined(_WIN32)
    const char* const fallback = "LOCALAPPDATA";
    const std::optional<std::string> base = variable(fallback);
    const fs::path suffix = path_from_utf8("the-unmapped");
#else
    const char* const fallback = "HOME";
    const std::optional<std::string> base = variable(fallback);
    const fs::path suffix = path_from_utf8(".cache/the-unmapped");
#endif
    if (!base) {
        throw Failure(Exit::usage, std::string("no default output location: neither UNMAPPED_BUILD_ROOT nor ") +
                                       fallback + " is set; pass --out");
    }
    const fs::path p = path_from_utf8(*base);
    if (!p.is_absolute()) {
        throw Failure(Exit::usage, std::string("no default output location: ") + fallback + " is '" + *base +
                                       "', which is not an absolute path; pass --out");
    }
    return p / suffix;
}

std::string shown(const fs::path& p) {
    return path_to_utf8(p);
}

// Deletes <out>.partial on scope exit unless dismissed.
class PartialGuard {
public:
    explicit PartialGuard(fs::path partial) : partial_(std::move(partial)) {}
    PartialGuard(const PartialGuard&) = delete;
    PartialGuard& operator=(const PartialGuard&) = delete;
    ~PartialGuard() {
        if (armed_) {
            std::error_code ec;
            fs::remove(partial_, ec);  // nothing more can be done if this fails
        }
    }
    void dismiss() noexcept { armed_ = false; }

private:
    fs::path partial_;
    bool armed_ = true;
};

}  // namespace

std::string default_output_dir() {
    const fs::path dir = build_root() / path_from_utf8("out");
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec || !fs::is_directory(dir)) {
        throw Failure(Exit::output, "cannot create the default output directory '" + shown(dir) + "'" +
                                        (ec ? ": " + ec.message() : std::string()));
    }
    return path_to_utf8(dir);
}

std::string absolute_utf8(std::string_view path_utf8) {
    std::error_code ec;
    fs::path p = fs::absolute(path_from_utf8(path_utf8), ec);
    if (ec) {
        p = path_from_utf8(path_utf8);
    }
    return path_to_utf8(p.lexically_normal());
}

void check_output_location(std::string_view path_utf8, bool force) {
    if (path_utf8.empty()) {
        throw Failure(Exit::output, "output path is empty");
    }
    const fs::path out = path_from_utf8(path_utf8);
    const std::string name(path_utf8);
    std::error_code ec;
    // symlink_status: anything at the path, a dangling symbolic link
    // included, counts as existing; a link is never followed.
    const fs::file_status status = fs::symlink_status(out, ec);
    if (fs::exists(status)) {
        if (fs::is_directory(status)) {
            throw Failure(Exit::output, "output '" + name + "' is a directory");
        }
        if (!force) {
            throw Failure(Exit::output, "output '" + name + "' already exists (pass --force to replace it)");
        }
    }
    fs::path parent = out.parent_path();
    if (parent.empty()) {
        parent = fs::path(".");
    }
    if (!fs::is_directory(parent, ec)) {
        throw Failure(Exit::output, "cannot write '" + name + "': the directory '" + shown(parent) +
                                        "' does not exist");
    }
}

void write_file_atomically(std::string_view path_utf8, std::span<const std::uint8_t> bytes, bool force) {
    check_output_location(path_utf8, force);
    const std::string name(path_utf8);
    const std::string partial_name = name + ".partial";
    const fs::path out = path_from_utf8(path_utf8);
    const fs::path partial = path_from_utf8(partial_name);

    PartialGuard guard(partial);  // from here every exit removes <out>.partial unless dismissed
    std::error_code ec;
    fs::remove(partial, ec);  // a stale one from an earlier crash
    if (ec) {
        throw Failure(Exit::output, "cannot remove old '" + partial_name + "': " + ec.message());
    }
    {
        std::ofstream file(partial, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw Failure(Exit::output, "cannot create '" + partial_name + "'");
        }
        if (!bytes.empty()) {
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        file.close();
        if (!file) {
            throw Failure(Exit::output, "writing '" + partial_name + "' failed");
        }
    }
    // Checked again just before the rename; the window in between is
    // accepted, as for world files (P0-02 14.7).
    if (!force && fs::exists(fs::symlink_status(out, ec))) {
        throw Failure(Exit::output, "output '" + name + "' already exists (pass --force to replace it)");
    }
    fs::rename(partial, out, ec);
    std::error_code exists_ec;
    if (ec && force && fs::exists(fs::symlink_status(out, exists_ec))) {
        // Should std::filesystem::rename refuse to replace an existing file
        // on some platform, remove the target and retry once.
        std::error_code remove_ec;
        if (fs::remove(out, remove_ec) && !remove_ec) {
            ec.clear();
            fs::rename(partial, out, ec);
        }
    }
    if (ec) {
        throw Failure(Exit::output, "cannot rename '" + partial_name + "' to '" + name + "': " + ec.message());
    }
    guard.dismiss();
}

}  // namespace cli
