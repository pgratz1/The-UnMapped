#pragma once
// Kind schemas and the kind registry of the TOML loader (spec P0-03 sections
// 4, 7.1 and 7.5).
//
// A kind (for example `biome`) is registered in code with a name, a data
// directory, a required flag and a schema: a list of typed fields, each
// required or with a default, plus optional named checks. The production
// registry (builtin_kinds()) holds the fixed-id kind `worldgen` of P1-00
// 6.1; tests may build their own KindRegistry.
//
// Registration mistakes (bad names, a default that does not satisfy its own
// type, min > max, duplicate names) are programming errors and raise
// worldgen::FatalError (P0-01 4.1) when the type, field or kind is built.
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/value.hpp"

namespace worldgen::data {

class Schema;

// --- Grammars (P0-03 sections 7.1, 7.3) -------------------------------------

// [a-z][a-z0-9_]*, at least one byte: kind names, directories, field names
// (P0-03 7.1).
[[nodiscard]] bool is_valid_name(std::string_view text) noexcept;
// [a-z][a-z0-9_]*, 1 to 64 bytes: definition ids, and the values of ref
// fields (P0-03 7.3, section 4).
[[nodiscard]] bool is_valid_id(std::string_view text) noexcept;

// Parses decimal text with parse_decimal (P0-01 5.6), for writing fx bounds
// and defaults in schema code, e.g. decimal_literal("0.5"). Fatal if the
// text is not a valid decimal.
[[nodiscard]] Fx decimal_literal(std::string_view text);

// An optional extra rule for string fields, `string(max_bytes, grammar)`
// (P0-03 section 4). `matches` is applied to the string after the newline
// rule; `description` names the grammar in error messages ("expected ...").
struct StringGrammar {
    const char* description = nullptr;
    bool (*matches)(std::string_view text) noexcept = nullptr;
};

// The id grammar as a StringGrammar ([a-z][a-z0-9_]*, 1 to 64 bytes).
[[nodiscard]] StringGrammar id_grammar() noexcept;

// --- Field types (P0-03 section 4) ------------------------------------------

// The type constructor of a field (P0-03 section 4).
enum class TypeKind : std::uint8_t {
    integer,      // int(min, max)
    fixed,        // fx(min, max)
    boolean,      // bool
    string,       // string(max_bytes[, grammar])
    enumeration,  // enum(v1, v2, ...)
    reference,    // ref(kind)
    array,        // array(T, min_len, max_len)
    record,       // record(schema)
};

// A field type (P0-03 section 4). Built with the static functions; copies
// share nested element types and schemas.
class Type {
public:
    // Default string(4096) value cap for `string(...)` (P0-03 section 4).
    static constexpr std::uint64_t DEFAULT_MAX_BYTES = 4096;

    // int(min, max): any TOML integer in [min, max]. Fatal if min > max.
    [[nodiscard]] static Type integer(std::int64_t min, std::int64_t max);
    // fx(min, max): a TOML integer n (from_int(n)) or a decimal string,
    // in [min, max] by raw value. Fatal if min > max.
    [[nodiscard]] static Type fixed(Fx min, Fx max);
    // fx(min, max) with the bounds as decimal text, e.g. fixed("0", "1").
    [[nodiscard]] static Type fixed(std::string_view min, std::string_view max);
    // bool.
    [[nodiscard]] static Type boolean();
    // string(max_bytes): at most max_bytes bytes after the newline rule, no
    // forbidden control character. Fatal if max_bytes is 0.
    [[nodiscard]] static Type string(std::uint64_t max_bytes = DEFAULT_MAX_BYTES);
    // string(max_bytes, grammar). Fatal if max_bytes is 0 or grammar.matches
    // or grammar.description is null.
    [[nodiscard]] static Type string(std::uint64_t max_bytes, StringGrammar grammar);
    // enum(v1, v2, ...): a string equal to one of the values. Fatal if the
    // list is empty or has duplicates.
    [[nodiscard]] static Type enumeration(std::vector<std::string> values);
    // ref(kind): a string in the id grammar naming a definition of `kind`,
    // checked after all kinds are loaded (P0-03 7.5). Fatal if kind is not a
    // valid name. Loading fails with FatalError if `kind` is not registered.
    [[nodiscard]] static Type reference(std::string kind);
    // array(element, min_len, max_len): a TOML array of min_len to max_len
    // elements, each valid for element. Fatal if min_len > max_len.
    [[nodiscard]] static Type array(Type element, std::uint64_t min_len, std::uint64_t max_len);
    // record(schema): a TOML table (standard or inline), validated like a
    // definition (P0-03 7.5).
    [[nodiscard]] static Type record(Schema schema);

    [[nodiscard]] TypeKind kind() const noexcept { return kind_; }

    // Bounds of int (integer kind only; fatal otherwise).
    [[nodiscard]] std::int64_t int_min() const;
    [[nodiscard]] std::int64_t int_max() const;
    // Bounds of fx (fixed kind only).
    [[nodiscard]] Fx fx_min() const;
    [[nodiscard]] Fx fx_max() const;
    // Byte cap and grammar of string (string kind only; grammar.matches is
    // null when there is none).
    [[nodiscard]] std::uint64_t max_bytes() const;
    [[nodiscard]] const StringGrammar& grammar() const;
    // Values of enum (enumeration kind only), in the order given.
    [[nodiscard]] const std::vector<std::string>& enum_values() const;
    // Target kind of ref (reference kind only).
    [[nodiscard]] const std::string& ref_kind() const;
    // Element type and length range of array (array kind only).
    [[nodiscard]] const Type& element() const;
    [[nodiscard]] std::uint64_t min_len() const;
    [[nodiscard]] std::uint64_t max_len() const;
    // Schema of record (record kind only).
    [[nodiscard]] const Schema& schema() const;

    // True if v is a valid value of this type, as the loader would produce
    // it (ranges, lengths, enum membership, string rules, id grammar of refs,
    // record fields complete and in declaration order). Ref targets are not
    // checked here. Used to check defaults.
    [[nodiscard]] bool accepts(const Value& v) const;

private:
    Type() = default;
    TypeKind kind_ = TypeKind::boolean;
    std::int64_t int_min_ = 0;
    std::int64_t int_max_ = 0;
    Fx fx_min_;
    Fx fx_max_;
    std::uint64_t max_bytes_ = 0;
    StringGrammar grammar_;
    std::vector<std::string> enum_values_;
    std::string ref_kind_;
    std::shared_ptr<const Type> element_;
    std::uint64_t min_len_ = 0;
    std::uint64_t max_len_ = 0;
    std::shared_ptr<const Schema> schema_;
};

// --- Fields, checks, schemas (P0-03 sections 7.1, 7.5) -----------------------

// One schema field (P0-03 7.1): a name in the name grammar, a type, and
// either required or a default value.
struct Field {
    std::string name;
    Type type;
    std::optional<Value> default_value;  // empty for a required field

    // A required field. Fatal if name is not a valid name.
    [[nodiscard]] static Field required(std::string name, Type type);
    // An optional field with a default, applied when the field is absent
    // (P0-03 7.5 step 4). Fatal if name is not a valid name or
    // !type.accepts(default_value).
    [[nodiscard]] static Field optional(std::string name, Type type, Value default_value);

    [[nodiscard]] bool is_required() const noexcept { return !default_value.has_value(); }
};

// A named kind-specific rule (P0-03 7.5), run on a record after its type
// validation succeeded (all fields valid, defaults applied). Returns
// std::nullopt when the record passes, or a message saying what is wrong.
// Must be a pure function of the record.
struct Check {
    std::string name;
    std::function<std::optional<std::string>(const Record&)> rule;
};

// A list of fields in declaration order, plus named checks (P0-03 7.1).
// Field names must be unique; the declaration order is the order of the
// canonical encoding (section 8).
class Schema {
public:
    Schema() = default;
    // Fatal if two fields share a name or a check has an empty name or no rule.
    explicit Schema(std::vector<Field> fields, std::vector<Check> checks = {});

    [[nodiscard]] const std::vector<Field>& fields() const noexcept { return fields_; }
    [[nodiscard]] const std::vector<Check>& checks() const noexcept { return checks_; }
    // The field of that name, or nullptr.
    [[nodiscard]] const Field* find(std::string_view name) const noexcept;

private:
    std::vector<Field> fields_;
    std::vector<Check> checks_;
};

// --- Kinds and the registry (P0-03 section 7.1) -----------------------------

// One id of a fixed-id kind and the schema of its definition (P0-03
// amendment 15, P1-00 appendix A3 and section 6.1).
struct IdSchema {
    std::string id;  // id grammar [a-z][a-z0-9_]*, 1 to 64 bytes
    Schema schema;
};

// A kind of definition (P0-03 7.1).
//
// Two forms. An open kind (id_schemas empty) accepts any id, every
// definition validated against `schema`. A fixed-id kind (P0-03 amendment
// 15, P1-00 appendix A3) has a fixed, non-empty set of ids in id_schemas,
// each with its own schema, and `schema` must have no fields and no checks.
// For a fixed-id kind the loader reports:
//   - a definition whose id is not in the set, at its header:
//     "unknown id '<id>' for kind '<kind>'; expected one of '<a>', '<b>', ..."
//     (ids in ascending byte order); that definition is not validated;
//   - for each id of the set with no definition, when the kind's directory
//     exists, an error against the directory with no position:
//     "missing definition '<kind>.<id>'". When the directory is missing,
//     a required kind reports only the missing directory (as for an open
//     kind) and a kind that is not required loads with no definitions.
// Each definition is validated against its id's schema, named checks
// included. The canonical encoding (P0-03 8) is unchanged.
struct Kind {
    std::string name;       // grammar [a-z][a-z0-9_]*, unique
    std::string directory;  // one path segment under the data root, same grammar, unique
    bool required = false;  // missing directory or zero definitions is an error
    Schema schema;          // the schema of every definition of an open kind
    std::vector<IdSchema> id_schemas{};  // fixed-id kinds only: one entry per id ({} so that Kind{name, dir, req, schema} needs no fifth initializer)

    // A fixed-id kind (P0-03 amendment 15); `schema` stays empty.
    [[nodiscard]] static Kind with_fixed_ids(std::string name, std::string directory, bool required,
                                             std::vector<IdSchema> id_schemas);

    // True if id_schemas is not empty.
    [[nodiscard]] bool has_fixed_ids() const noexcept { return !id_schemas.empty(); }

    // The schema a definition with this id is validated against: `schema`
    // for an open kind; for a fixed-id kind the id's schema, or nullptr if
    // the id is not in the set.
    [[nodiscard]] const Schema* schema_for(std::string_view id) const noexcept;
};

// The set of registered kinds (P0-03 7.1), kept in ascending name order
// (by bytes), which is the processing and encoding order.
class KindRegistry {
public:
    // Registers a kind. Fatal if its name or directory is not a valid name,
    // or either is already used by a registered kind. For a fixed-id kind
    // (P0-03 amendment 15) also fatal if an id does not match the id
    // grammar, an id is listed twice, or `schema` has fields or checks. Each
    // id's schema was already validated when it was built (Schema and Field
    // constructors). id_schemas is stored in ascending id order.
    void add(Kind kind);

    // All kinds, ascending name.
    [[nodiscard]] const std::vector<Kind>& kinds() const noexcept { return kinds_; }
    // The kind of that name, or nullptr.
    [[nodiscard]] const Kind* find(std::string_view name) const noexcept;

private:
    std::vector<Kind> kinds_;
};

// The production registry: the fixed list of every kind the generator uses
// (P0-03 7.1, defined in one source file, src/data/kinds.cpp). It holds the
// fixed-id kind `worldgen` (P1-00 6.1, P0-03 amendment 16: directory
// `worldgen`, required) with the ids of the first pass, `elevation`
// (P1-01 4.2) and `hydrology` (P1-02 4). Because a required kind is
// registered, every world needs a data root and its content_hash is the
// hash of the loaded data (P0-03 amendment 17).
[[nodiscard]] KindRegistry builtin_kinds();

// --- The `worldgen` kind (P1-00 section 6.1) ---------------------------------

// Kind name, directory and ids (P1-00 6.1).
inline constexpr std::string_view WORLDGEN_KIND = "worldgen";
inline constexpr std::string_view WORLDGEN_DIRECTORY = "worldgen";
inline constexpr std::string_view WORLDGEN_ELEVATION_ID = "elevation";
inline constexpr std::string_view WORLDGEN_HYDROLOGY_ID = "hydrology";

// The schema of definition worldgen.elevation (P1-01 4.2): every field of
// the table of 4.2 in its order, all required, with the shared noise record
// types of P1-01 4.1 (worldgen/geo/noise_schema.hpp). Named checks of P1-01
// 4.3, each a load error at the definition's header (P0-03 7.5):
//   edge_radial_span   edge_radial_end - edge_radial_start >= 0.01;
//   mountain_ranks     mountain_cover_rank <= mountain_peak_rank;
//   lake_areas         lake_min_area_m2 <= lake_max_area_m2.
[[nodiscard]] Schema worldgen_elevation_schema();

// The schema of definition worldgen.hydrology (P1-02 4): river_min_area_m2
// int(256, 10^9), required.
[[nodiscard]] Schema worldgen_hydrology_schema();

}  // namespace worldgen::data
