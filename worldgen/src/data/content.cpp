// Loading the data directory: discovery, file structure, definition order,
// validation, defaults and references (P0-03 sections 7.1 to 7.5, 9).

#include "worldgen/data/content.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "data/common.hpp"
#include "data/text.hpp"
#include "worldgen/core/decimal.hpp"
#include "worldgen/core/error.hpp"

namespace worldgen::data {

// --- ContentSet lookups ---------------------------------------------------------

const Definition* KindContent::find(std::string_view id) const noexcept {
    const auto it = std::lower_bound(definitions.begin(), definitions.end(), id,
                                     [](const Definition& d, std::string_view x) { return detail::bytes_less(d.id, x); });
    return it != definitions.end() && it->id == id ? &*it : nullptr;
}

const KindContent* ContentSet::find_kind(std::string_view kind) const noexcept {
    for (const KindContent& k : kinds) {
        if (k.kind == kind) {
            return &k;
        }
    }
    return nullptr;
}

const Definition* ContentSet::find(std::string_view kind, std::string_view id) const noexcept {
    const KindContent* k = find_kind(kind);
    return k == nullptr ? nullptr : k->find(id);
}

namespace {

using detail::ErrorList;
using detail::Node;
using detail::NodeKind;
using detail::Position;
using detail::in_quotes;

// --- Discovery (P0-03 7.2) --------------------------------------------------------

enum class EntryType : std::uint8_t { regular, directory, symlink, other };

// One directory entry as listed, before the rules of 7.2 are applied.
struct RawEntry {
    std::string name;  // UTF-8 bytes
    EntryType type = EntryType::other;
    std::filesystem::path fs_path;          // filesystem source
    const std::string* memory_bytes = nullptr;  // in-memory source
};

// A data file accepted by discovery, with its bytes.
struct SourceFile {
    std::string path;  // "<directory>/<name>"
    std::string bytes;
};

// The files of one kind directory, in byte order of their names.
struct Listing {
    bool present = false;  // the directory exists
    bool failed = false;   // an error about the directory itself was reported
    std::vector<SourceFile> files;
};

// Applies steps 2 to 4 of P0-03 7.2 to the listed entries of `directory`,
// and reads the accepted files in name order.
void accept_entries(std::string_view directory, std::vector<RawEntry> entries, ErrorList& errors,
                    Listing& listing) {
    std::vector<RawEntry> accepted;
    for (RawEntry& e : entries) {
        if (!e.name.empty() && e.name[0] == '.') {
            continue;  // hidden
        }
        if (!detail::ends_with(e.name, ".toml")) {
            continue;  // README.md, notes.txt, races.toml~
        }
        const std::string display = std::string(directory) + "/" + e.name;
        if (e.type != EntryType::regular) {
            const char* what = e.type == EntryType::directory ? "a directory"
                               : e.type == EntryType::symlink ? "a symbolic link"
                                                              : "not a regular file";
            errors.add(display, Position{},
                       in_quotes(e.name) + " is " + what + "; data files must be regular files");
            continue;
        }
        if (!detail::is_valid_data_file_name(e.name)) {
            errors.add(display, Position{},
                       "invalid data file name " + in_quotes(e.name) +
                           "; names must match [a-z0-9][a-z0-9_-]*\\.toml (lower case only)");
            continue;
        }
        accepted.push_back(std::move(e));
    }
    std::stable_sort(accepted.begin(), accepted.end(),
              [](const RawEntry& a, const RawEntry& b) { return detail::bytes_less(a.name, b.name); });
    for (const RawEntry& e : accepted) {
        std::string display = std::string(directory) + "/" + e.name;
        if (e.memory_bytes != nullptr) {
            listing.files.push_back(SourceFile{std::move(display), *e.memory_bytes});
            continue;
        }
        std::optional<std::string> bytes = detail::read_file(e.fs_path, display, errors);
        if (bytes) {
            listing.files.push_back(SourceFile{std::move(display), std::move(*bytes)});
        }
    }
}

EntryType entry_type(const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    const std::filesystem::file_status st = entry.symlink_status(ec);
    if (ec) {
        return EntryType::other;
    }
    switch (st.type()) {
        case std::filesystem::file_type::regular: return EntryType::regular;
        case std::filesystem::file_type::directory: return EntryType::directory;
        case std::filesystem::file_type::symlink: return EntryType::symlink;
        default: return EntryType::other;  // includes MSVC's junction
    }
}

// Lists a directory. Returns false if it cannot be listed or a name cannot
// be converted to UTF-8.
bool list_directory(const std::filesystem::path& dir, std::vector<RawEntry>& out) {
    std::error_code ec;
    // determinism-lint: allow(directory-iteration) -- listing order is never used: accept_entries stable_sorts the accepted names by bytes_less (unique in one directory), load errors are sorted by error_less, and the root lookup is an exact match on a unique name
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) {
        return false;
    }
    try {
        // determinism-lint: allow(directory-iteration) -- the end sentinel of the listing above, whose order is discarded (see the suppression above)
        for (const std::filesystem::directory_iterator end; it != end; it.increment(ec)) {
            if (ec) {
                return false;
            }
            RawEntry e;
            e.name = detail::path_utf8(it->path().filename());
            e.type = entry_type(*it);
            e.fs_path = it->path();
            out.push_back(std::move(e));
        }
    } catch (const std::system_error&) {
        return false;  // a name that is not valid Unicode (Windows)
    }
    return !ec;
}

// The data root on disk: listed once, kind directories matched by exact name.
class FsSource {
public:
    explicit FsSource(std::filesystem::path root) : root_(std::move(root)) {}

    Listing list(const Kind& kind, ErrorList& errors) {
        Listing listing;
        if (!root_listed_) {
            root_listed_ = true;
            std::error_code ec;
            const bool is_dir = std::filesystem::is_directory(root_, ec);
            root_ok_ = !ec && is_dir && list_directory(root_, root_entries_);
            if (!root_ok_) {
                errors.add(detail::path_utf8(root_), Position{},
                           "the data directory does not exist, is not a directory or cannot be listed");
            }
        }
        if (!root_ok_) {
            listing.failed = true;
            return listing;
        }
        const RawEntry* dir = nullptr;
        for (const RawEntry& e : root_entries_) {
            if (e.name == kind.directory) {
                dir = &e;
            }
        }
        if (dir == nullptr) {
            return listing;  // missing
        }
        if (dir->type != EntryType::directory) {
            const char* what = dir->type == EntryType::symlink ? "a symbolic link" : "not a directory";
            errors.add(kind.directory, Position{},
                       "the directory of kind " + in_quotes(kind.name) + " is " + what + "; it must be a real directory");
            listing.failed = true;
            return listing;
        }
        listing.present = true;
        std::vector<RawEntry> entries;
        if (!list_directory(dir->fs_path, entries)) {
            errors.add(kind.directory, Position{}, "cannot list the directory");
            listing.failed = true;
            return listing;
        }
        accept_entries(kind.directory, std::move(entries), errors, listing);
        return listing;
    }

private:
    std::filesystem::path root_;
    bool root_listed_ = false;
    bool root_ok_ = false;
    std::vector<RawEntry> root_entries_;
};

class MemorySource {
public:
    explicit MemorySource(const MemoryDataRoot& root) : root_(root) {}

    Listing list(const Kind& kind, ErrorList& errors) const {
        Listing listing;
        const MemoryDirectory* dir = nullptr;
        for (const MemoryDirectory& d : root_.directories) {
            if (d.name == kind.directory) {
                dir = &d;
            }
        }
        if (dir == nullptr) {
            return listing;
        }
        listing.present = true;
        std::vector<RawEntry> entries;
        for (const MemoryEntry& m : dir->entries) {
            RawEntry e;
            e.name = m.name;
            e.type = m.is_directory ? EntryType::directory : EntryType::regular;
            e.memory_bytes = &m.bytes;
            entries.push_back(std::move(e));
        }
        accept_entries(kind.directory, std::move(entries), errors, listing);
        return listing;
    }

private:
    const MemoryDataRoot& root_;
};

class NoSource {
public:
    static Listing list(const Kind&, ErrorList&) { return Listing{}; }
};

// --- Validation (P0-03 section 4, 7.5) ----------------------------------------------

std::string position_text(std::string_view path, Position p) {
    return std::string(path) + ":" + detail::uint_text(p.line) + ":" + detail::uint_text(p.column);
}

// "expected ..." text of a type, for messages.
std::string expected_text(const Type& type) {
    switch (type.kind()) {
        case TypeKind::integer:
            return "an integer from " + detail::int_text(type.int_min()) + " to " + detail::int_text(type.int_max());
        case TypeKind::fixed:
            return "a number from " + detail::fx_text(type.fx_min()) + " to " + detail::fx_text(type.fx_max()) +
                   " (an integer, or a decimal string such as \"0.5\")";
        case TypeKind::boolean:
            return "true or false";
        case TypeKind::string:
            return "a string of at most " + detail::uint_text(type.max_bytes()) + " bytes" +
                   (type.grammar().matches != nullptr ? std::string(", ") + type.grammar().description : std::string());
        case TypeKind::enumeration: {
            std::string out = "one of";
            for (std::size_t i = 0; i < type.enum_values().size(); ++i) {
                out += i == 0 ? " \"" : ", \"";
                out += type.enum_values()[i];
                out += '"';
            }
            return out;
        }
        case TypeKind::reference:
            return "the id of a " + type.ref_kind() + " definition";
        case TypeKind::array:
            return "an array of " + detail::uint_text(type.min_len()) + " to " + detail::uint_text(type.max_len()) +
                   " elements";
        case TypeKind::record:
            return "a table";
    }
    return "a value";
}

struct PendingRef {
    std::string target;  // kind
    std::string id;
    std::string path;
    Position position;
    std::string key;
};

class Validator {
public:
    Validator(ErrorList& errors, std::vector<PendingRef>& refs) : errors_(errors), refs_(refs) {}

    // Validates a table against a schema (P0-03 7.5 steps 1 to 4, then the
    // named checks). Returns false if any error was reported.
    bool record(const Schema& schema, const Node& table, const std::string& key, const std::string& path,
                Record& out, bool* types_valid = nullptr) {
        bool ok = true;
        for (std::size_t i = 0; i < table.keys.size(); ++i) {
            if (schema.find(table.keys[i]) == nullptr) {
                std::string msg = "unknown key " + in_quotes(detail::child_key(key, table.keys[i])) + "; expected ";
                if (schema.fields().empty()) {
                    msg += "no keys";
                } else {
                    msg += "one of";
                    for (std::size_t f = 0; f < schema.fields().size(); ++f) {
                        msg += f == 0 ? " " : ", ";
                        msg += schema.fields()[f].name;
                    }
                }
                errors_.add(path, table.children[i].position, std::move(msg));
                ok = false;
            }
        }
        for (const Field& field : schema.fields()) {
            const std::string field_key = detail::child_key(key, field.name);
            const Node* node = child(table, field.name);
            if (node != nullptr) {
                Value v;
                if (value(field.type, *node, field_key, path, v)) {
                    out.add(field.name, std::move(v));
                } else {
                    ok = false;
                }
            } else if (field.is_required()) {
                errors_.add(path, table.position, "missing required field " + in_quotes(field_key));
                ok = false;
            } else {
                default_refs(field.type, *field.default_value, field_key, path, table.position);
                out.add(field.name, *field.default_value);
            }
        }
        if (types_valid != nullptr) {
            *types_valid = ok;
        }
        if (ok) {
            for (const Check& check : schema.checks()) {
                const std::optional<std::string> failure = check.rule(out);
                if (failure) {
                    errors_.add(path, table.position,
                                "check " + in_quotes(check.name) + " failed for " + in_quotes(key) + ": " + *failure);
                    ok = false;
                }
            }
        }
        return ok;
    }

private:
    static const Node* child(const Node& table, std::string_view name) {
        const auto it = std::lower_bound(table.keys.begin(), table.keys.end(), name, detail::BytesLess{});
        if (it == table.keys.end() || *it != name) {
            return nullptr;
        }
        return &table.children[static_cast<std::size_t>(it - table.keys.begin())];
    }

    void mismatch(const Type& type, const Node& node, const std::string& key, const std::string& path) {
        errors_.add(path, node.position,
                    in_quotes(key) + " is " + std::string(detail::kind_phrase(node)) + "; expected " + expected_text(type));
    }

    bool value(const Type& type, const Node& node, const std::string& key, const std::string& path, Value& out) {
        if (detail::is_rejected_scalar(node)) {
            return false;  // float, date/time or control character: already reported
        }
        switch (type.kind()) {
            case TypeKind::integer: return integer(type, node, key, path, out);
            case TypeKind::fixed: return fixed(type, node, key, path, out);
            case TypeKind::boolean:
                if (node.kind != NodeKind::boolean) {
                    mismatch(type, node, key, path);
                    return false;
                }
                out = Value::boolean(node.boolean);
                return true;
            case TypeKind::string: return string(type, node, key, path, out);
            case TypeKind::enumeration: {
                if (node.kind != NodeKind::string) {
                    mismatch(type, node, key, path);
                    return false;
                }
                const auto& values = type.enum_values();
                if (std::find(values.begin(), values.end(), node.text) == values.end()) {
                    errors_.add(path, node.position,
                                in_quotes(key) + " is \"" + node.text + "\"; expected " + expected_text(type));
                    return false;
                }
                out = Value::string(node.text);
                return true;
            }
            case TypeKind::reference: {
                if (node.kind != NodeKind::string) {
                    mismatch(type, node, key, path);
                    return false;
                }
                if (!is_valid_id(node.text)) {
                    errors_.add(path, node.position,
                                in_quotes(key) + " is \"" + node.text + "\", which is not an id; expected " +
                                    expected_text(type));
                    return false;
                }
                refs_.push_back(PendingRef{type.ref_kind(), node.text, path, node.position, key});
                out = Value::string(node.text);
                return true;
            }
            case TypeKind::array: return array(type, node, key, path, out);
            case TypeKind::record: {
                if (node.kind != NodeKind::table) {
                    mismatch(type, node, key, path);
                    return false;
                }
                Record r;
                if (!record(type.schema(), node, key, path, r)) {
                    return false;
                }
                out = Value::record(std::move(r));
                return true;
            }
        }
        return false;
    }

    bool integer(const Type& type, const Node& node, const std::string& key, const std::string& path, Value& out) {
        if (node.kind != NodeKind::integer) {
            mismatch(type, node, key, path);
            return false;
        }
        if (node.integer < type.int_min() || node.integer > type.int_max()) {
            errors_.add(path, node.position,
                        in_quotes(key) + " is " + detail::int_text(node.integer) + "; expected " + expected_text(type));
            return false;
        }
        out = Value::integer(node.integer);
        return true;
    }

    bool fixed(const Type& type, const Node& node, const std::string& key, const std::string& path, Value& out) {
        Fx v;
        std::string shown;
        if (node.kind == NodeKind::integer) {
            // from_int accepts |n| <= 2^31 - 1 (P0-01 5.2); check first so that
            // an out-of-range integer is a load error, not a fatal error.
            constexpr std::int64_t limit = 2147483647;
            if (node.integer < -limit || node.integer > limit) {
                errors_.add(path, node.position,
                            in_quotes(key) + " is " + detail::int_text(node.integer) +
                                ", outside the fixed-point range; expected " + expected_text(type));
                return false;
            }
            v = Fx::from_int(node.integer);
            shown = detail::int_text(node.integer);
        } else if (node.kind == NodeKind::string) {
            const DecimalParse p = parse_decimal(node.text);
            if (!p.ok()) {
                const char* why = "is not a decimal number";
                switch (p.error) {
                    case DecimalError::int_digits: why = "has more than 10 integer digits"; break;
                    case DecimalError::frac_digits: why = "has more than 18 fractional digits"; break;
                    case DecimalError::out_of_range: why = "is outside the fixed-point range"; break;
                    case DecimalError::syntax:
                    case DecimalError::none: break;
                }
                errors_.add(path, node.position,
                            in_quotes(key) + " is \"" + node.text + "\", which " + why + "; expected " +
                                expected_text(type));
                return false;
            }
            v = p.value;
            shown = "\"" + node.text + "\"";
        } else {
            mismatch(type, node, key, path);
            return false;
        }
        if (v < type.fx_min() || v > type.fx_max()) {
            errors_.add(path, node.position, in_quotes(key) + " is " + shown + "; expected " + expected_text(type));
            return false;
        }
        out = Value::fixed(v);
        return true;
    }

    bool string(const Type& type, const Node& node, const std::string& key, const std::string& path, Value& out) {
        if (node.kind != NodeKind::string) {
            mismatch(type, node, key, path);
            return false;
        }
        const auto size = static_cast<std::uint64_t>(node.text.size());
        if (size > type.max_bytes()) {
            errors_.add(path, node.position,
                        in_quotes(key) + " is " + detail::uint_text(size) + " bytes long; expected " +
                            expected_text(type));
            return false;
        }
        if (type.grammar().matches != nullptr && !type.grammar().matches(node.text)) {
            errors_.add(path, node.position, in_quotes(key) + " is \"" + node.text + "\"; expected " + expected_text(type));
            return false;
        }
        out = Value::string(node.text);
        return true;
    }

    bool array(const Type& type, const Node& node, const std::string& key, const std::string& path, Value& out) {
        if (node.kind != NodeKind::array) {
            mismatch(type, node, key, path);
            return false;
        }
        const auto n = static_cast<std::uint64_t>(node.children.size());
        bool ok = true;
        if (n < type.min_len() || n > type.max_len()) {
            errors_.add(path, node.position,
                        in_quotes(key) + " has " + detail::uint_text(n) + " elements; expected " + expected_text(type));
            ok = false;
        }
        std::vector<Value> items;
        items.reserve(node.children.size());
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            Value v;
            if (value(type.element(), node.children[i], detail::element_key(key, static_cast<std::uint64_t>(i)), path,
                      v)) {
                items.push_back(std::move(v));
            } else {
                ok = false;
            }
        }
        if (ok) {
            out = Value::array(std::move(items));
        }
        return ok;
    }

    // Queues the refs inside a default value, positioned at the definition's
    // table header.
    void default_refs(const Type& type, const Value& v, const std::string& key, const std::string& path,
                      Position at) {
        switch (type.kind()) {
            case TypeKind::reference:
                refs_.push_back(PendingRef{type.ref_kind(), v.as_string(), path, at, key});
                return;
            case TypeKind::array:
                for (std::size_t i = 0; i < v.as_array().size(); ++i) {
                    default_refs(type.element(), v.as_array()[i],
                                 detail::element_key(key, static_cast<std::uint64_t>(i)), path, at);
                }
                return;
            case TypeKind::record: {
                const Record& r = v.as_record();
                for (std::size_t i = 0; i < r.size(); ++i) {
                    default_refs(type.schema().fields()[i].type, r.value_at(i), detail::child_key(key, r.name_at(i)),
                                 path, at);
                }
                return;
            }
            case TypeKind::integer:
            case TypeKind::fixed:
            case TypeKind::boolean:
            case TypeKind::string:
            case TypeKind::enumeration:
                return;
        }
    }

    ErrorList& errors_;
    std::vector<PendingRef>& refs_;
};

// --- Registry check ---------------------------------------------------------------

void check_ref_targets(const KindRegistry& registry, const Type& type) {
    switch (type.kind()) {
        case TypeKind::reference:
            if (registry.find(type.ref_kind()) == nullptr) {
                fatal("load_content: a ref field targets a kind that is not registered");
            }
            return;
        case TypeKind::array:
            check_ref_targets(registry, type.element());
            return;
        case TypeKind::record:
            for (const Field& f : type.schema().fields()) {
                check_ref_targets(registry, f.type);
            }
            return;
        case TypeKind::integer:
        case TypeKind::fixed:
        case TypeKind::boolean:
        case TypeKind::string:
        case TypeKind::enumeration:
            return;
    }
}

// --- The load ---------------------------------------------------------------------

using IdSet = std::set<std::string, detail::BytesLess>;

struct Location {
    std::string path;
    Position position;
};

// Loads one kind from its listing (P0-03 7.3 to 7.5). Adds every id seen to
// ids (for ref resolution), whether or not its definition is valid.
KindContent load_kind(const Kind& kind, const Listing& listing, ErrorList& errors, std::vector<PendingRef>& refs,
                      IdSet& ids) {
    KindContent out{kind.name, {}};
    if (!listing.present) {
        if (kind.required && !listing.failed) {
            errors.add(kind.directory, Position{},
                       "the directory of required kind " + in_quotes(kind.name) + " is missing");
        }
        return out;
    }

    struct Candidate {
        std::string id;
        const std::string* path;
        const Node* node;
        bool duplicate;
    };
    std::vector<std::unique_ptr<Node>> documents;  // keep the parsed trees alive
    std::map<std::string, Location, detail::BytesLess> first_seen;
    std::vector<Candidate> candidates;

    for (const SourceFile& file : listing.files) {
        auto root = std::make_unique<Node>();
        if (!detail::parse_file(file.path, file.bytes, errors, *root)) {
            continue;  // a syntax error stops only its own file
        }
        for (std::size_t i = 0; i < root->keys.size(); ++i) {
            const std::string& top = root->keys[i];
            const Node& table = root->children[i];
            if (top != kind.name) {
                errors.add(file.path, table.position,
                           "unexpected top-level key " + in_quotes(top) + "; files in '" + kind.directory +
                               "/' hold only the table " + in_quotes(kind.name));
                continue;
            }
            if (table.kind != NodeKind::table) {
                errors.add(file.path, table.position,
                           in_quotes(top) + " is " + std::string(detail::kind_phrase(table)) +
                               "; expected a table of definitions keyed by id");
                continue;
            }
            for (std::size_t j = 0; j < table.keys.size(); ++j) {
                const std::string& id = table.keys[j];
                const Node& def = table.children[j];
                const std::string full = detail::child_key(kind.name, id);
                if (!is_valid_id(id)) {
                    errors.add(file.path, def.position,
                               "invalid id " + in_quotes(id) + " in " + in_quotes(kind.name) +
                                   "; ids match [a-z][a-z0-9_]* and are 1 to 64 bytes");
                    continue;
                }
                if (kind.schema_for(id) == nullptr) {
                    // A fixed-id kind and an id outside its set (P0-03
                    // amendment 15): an error at the definition's header.
                    std::string expected;
                    for (const IdSchema& s : kind.id_schemas) {  // ascending id
                        expected += expected.empty() ? "" : ", ";
                        expected += in_quotes(s.id);
                    }
                    errors.add(file.path, def.position,
                               "unknown id " + in_quotes(id) + " for kind " + in_quotes(kind.name) + "; expected one of " +
                                   expected);
                    continue;
                }
                if (def.kind != NodeKind::table) {
                    errors.add(file.path, def.position,
                               "definition " + in_quotes(full) + " is " + std::string(detail::kind_phrase(def)) +
                                   "; expected a table");
                    continue;
                }
                const auto [it, inserted] = first_seen.try_emplace(id, Location{file.path, def.position});
                if (!inserted) {
                    errors.add(file.path, def.position,
                               "duplicate id " + in_quotes(full) + "; also defined at " +
                                   position_text(it->second.path, it->second.position));
                }
                candidates.push_back(Candidate{id, &file.path, &def, !inserted});
            }
        }
        documents.push_back(std::move(root));
    }

    // Canonical order: ascending id by bytes (P0-03 7.4). Equal ids are
    // duplicates, already an error; stable_sort keeps them in file order.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return detail::bytes_less(a.id, b.id); });

    Validator validator(errors, refs);
    // The definitions that passed type validation (named checks aside), in
    // id order, with their header positions: what the kind-level checks see
    // (P1-04 4.1). Duplicate ids are left out (already an error).
    std::vector<Definition> typed;
    std::vector<Location> locations;
    for (const Candidate& c : candidates) {
        Record record;
        bool types_valid = false;
        const bool ok = validator.record(*kind.schema_for(c.id), *c.node, detail::child_key(kind.name, c.id), *c.path,
                                         record, &types_valid);
        if (types_valid && !c.duplicate && !kind.kind_checks.empty()) {
            typed.push_back(Definition{c.id, record});
            locations.push_back(Location{*c.path, c.node->position});
        }
        if (ok && !c.duplicate) {
            out.definitions.push_back(Definition{c.id, std::move(record)});
        }
    }
    // Kind-level checks (P1-04 4.1): over the type-valid definitions, and
    // only when there is at least one.
    if (!typed.empty()) {
        for (const KindCheck& check : kind.kind_checks) {
            for (const KindCheckFailure& f : check.rule(typed)) {
                if (f.definition && *f.definition < typed.size()) {
                    const Location& at = locations[*f.definition];
                    errors.add(at.path, at.position,
                               "check " + in_quotes(check.name) + " failed for " +
                                   in_quotes(detail::child_key(kind.name, typed[*f.definition].id)) + ": " +
                                   f.message);
                } else {
                    errors.add(kind.directory, Position{},
                               "check " + in_quotes(check.name) + " failed for kind " + in_quotes(kind.name) + ": " +
                                   f.message);
                }
            }
        }
    }
    for (const auto& [id, location] : first_seen) {
        ids.insert(id);
    }
    if (kind.has_fixed_ids()) {
        // Every id of the set must be defined (P0-03 amendment 15). This
        // replaces the "no definitions" error of a required kind, which it
        // implies.
        for (const IdSchema& s : kind.id_schemas) {
            if (first_seen.find(s.id) == first_seen.end()) {
                errors.add(kind.directory, Position{}, "missing definition " + in_quotes(detail::child_key(kind.name, s.id)));
            }
        }
    } else if (kind.required && first_seen.empty()) {
        errors.add(kind.directory, Position{},
                   "required kind " + in_quotes(kind.name) + " has no definitions in '" + kind.directory + "/'");
    }
    return out;
}

template <typename Source>
LoadResult<ContentSet> load_from(const KindRegistry& registry, Source& source) {
    for (const Kind& kind : registry.kinds()) {
        for (const Field& f : kind.schema.fields()) {
            check_ref_targets(registry, f.type);
        }
        for (const IdSchema& s : kind.id_schemas) {
            for (const Field& f : s.schema.fields()) {
                check_ref_targets(registry, f.type);
            }
        }
    }
    ErrorList errors;
    std::vector<PendingRef> refs;
    std::map<std::string, IdSet, detail::BytesLess> ids_by_kind;
    ContentSet content;
    for (const Kind& kind : registry.kinds()) {  // ascending name
        const Listing listing = source.list(kind, errors);
        content.kinds.push_back(load_kind(kind, listing, errors, refs, ids_by_kind[kind.name]));
    }
    // References, after all kinds are loaded (P0-03 7.5).
    for (const PendingRef& r : refs) {
        const IdSet& ids = ids_by_kind[r.target];
        if (ids.find(r.id) == ids.end()) {
            errors.add(r.path, r.position,
                       in_quotes(r.key) + " refers to " + in_quotes(r.id) + ", but there is no " + r.target +
                           " definition with that id");
        }
    }
    LoadResult<ContentSet> result;
    if (errors.empty()) {
        result.value = std::move(content);
    } else {
        result.errors = errors.take_sorted();
    }
    return result;
}

}  // namespace

LoadResult<ContentSet> load_content(const KindRegistry& registry, const std::filesystem::path& data_root) {
    FsSource source(data_root);
    return load_from(registry, source);
}

LoadResult<ContentSet> load_content_without_data(const KindRegistry& registry) {
    NoSource source;
    return load_from(registry, source);
}

LoadResult<ContentSet> load_content_from_memory(const KindRegistry& registry, const MemoryDataRoot& root) {
    MemorySource source(root);
    return load_from(registry, source);
}

}  // namespace worldgen::data
