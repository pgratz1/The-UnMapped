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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/value.hpp"

namespace worldgen::data {

class Schema;
struct Definition;  // worldgen/data/content.hpp

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

// The word grammar (P0-03 amendment 17, P1-06 4): one ASCII capital letter
// A-Z followed by zero or more ASCII lower-case letters a-z ([A-Z][a-z]*);
// the empty string does not match. No length limit of its own (the string
// type's byte cap applies first).
[[nodiscard]] bool is_valid_word(std::string_view text) noexcept;
// The word grammar as a StringGrammar, description "a word ([A-Z][a-z]*)".
[[nodiscard]] StringGrammar word_grammar() noexcept;

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

// --- Kind-level checks (P1-04 4.1) -------------------------------------------

// One failure of a kind-level check: the index of the definition it is
// reported at (in the span the check was given, ascending id), or
// std::nullopt to report it against the kind's directory with no position;
// and the message.
struct KindCheckFailure {
    std::optional<std::size_t> definition;
    std::string message;
};

// A named rule over all definitions of one kind (P1-04 4.1 named checks that
// compare definitions, e.g. biome_code_unique, or that report more than one
// error per definition). Run once, after every definition of the kind has
// been validated, on the definitions that passed type validation (whether
// or not they passed the per-definition checks; duplicate ids left out), in
// ascending id order, and only when there is at least one such definition.
// Each failure becomes one load error: at the definition's header,
// "check '<name>' failed for '<kind>.<id>': <message>", or against the
// directory, "check '<name>' failed for kind '<kind>': <message>". Must be a
// pure function of the definitions.
struct KindCheck {
    std::string name;
    std::function<std::vector<KindCheckFailure>(std::span<const Definition>)> rule;
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
    std::vector<KindCheck> kind_checks{};  // checks across definitions (P1-04 4.1), run in this order

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
// `worldgen`, required) with the ids `elevation` (P1-01 4.2),
// `hydrology` (P1-02 4), `climate` (P1-03 4) and `biomes` (P1-04 4.2),
// and the open kind `biome` (P1-00 6.6, P1-04 4.1: directory `biomes`,
// required, with its kind-level checks), and from P3-01 the open kinds
// `race` and `culture` and the fixed-id kind `history` (id `chronology`),
// all required (P3-01 3 to 5). Because a required kind is
// registered, every world needs a data root and its content_hash is the
// hash of the loaded data (P0-03 amendment 17).
[[nodiscard]] KindRegistry builtin_kinds();

// --- The `worldgen` kind (P1-00 section 6.1) ---------------------------------

// Kind name, directory and ids (P1-00 6.1).
inline constexpr std::string_view WORLDGEN_KIND = "worldgen";
inline constexpr std::string_view WORLDGEN_DIRECTORY = "worldgen";
inline constexpr std::string_view WORLDGEN_ELEVATION_ID = "elevation";
inline constexpr std::string_view WORLDGEN_HYDROLOGY_ID = "hydrology";
inline constexpr std::string_view WORLDGEN_CLIMATE_ID = "climate";
inline constexpr std::string_view WORLDGEN_BIOMES_ID = "biomes";
inline constexpr std::string_view WORLDGEN_RESOURCES_ID = "resources";
inline constexpr std::string_view WORLDGEN_REGIONS_ID = "regions";
// Upper bound of worldgen.climate.smooth_radius_m (P1-03 4); the climate
// stage's overflow argument for C4 depends on it (static_assert there).
inline constexpr std::int64_t CLIMATE_SMOOTH_RADIUS_MAX_M = 4000;

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

// The schema of definition worldgen.biomes (P1-04 4.2): jitter_noise
// (fbm_m), temp_jitter_c fx(0, 10), rain_jitter fx(0, 1), rain_scale_512,
// rain_scale_1024 and rain_scale_2048 fx(0.25, 4), drain_river_class_min
// int(1, 15), in this order, all required.
[[nodiscard]] Schema worldgen_biomes_schema();

// --- The `biome` kind (P1-00 6.6, P1-04 4.1) ---------------------------------

// Kind name and directory (P1-00 6.6): files in game/data/biomes/.
inline constexpr std::string_view BIOME_KIND = "biome";
inline constexpr std::string_view BIOME_DIRECTORY = "biomes";

// The schema of a biome definition (P1-04 4.1): code int(1, 254), name
// string(64), colour int(0, 16777215), water bool, water_body enum("none",
// "lake", "ocean"), priority int(0, 255), fallback bool, rules
// array(record(biome_rule), 0, 16), fertility fx(0, 1), timber fx(0, 1),
// forage fx(0, 1) (P1-05 4.1, from generator 0.10.0), region_noun string(32), in this order, all required. Record biome_rule:
// temp_min_c, temp_max_c fx(-60, 50); rain_min_mm, rain_max_mm int(0,
// 10000); elev_min_m, elev_max_m int(0, 3000); slope_max fx(0, 100);
// hand_max_m fx(0, 3000); coast_max_m int(0, 32767). Named checks of one
// definition: biome_water_body, biome_water_rules (biome_rule_ranges is
// kind-level, so that it reports every failing pair).
[[nodiscard]] Schema biome_schema();

// The kind-level named checks of P1-04 4.1, in this order:
// biome_code_unique, biome_water_unique, biome_fallback,
// biome_priority_unique, biome_rule_ranges (messages as in the table of
// 4.1).
[[nodiscard]] std::vector<KindCheck> biome_kind_checks();

// The schema of definition worldgen.climate (P1-03 4): every field of the
// table of 4 in its order, all required. Named check, a load error at the
// definition's header (P0-03 7.5):
//   wind_weights_positive   the sum of wind_weights is at least 1.
[[nodiscard]] Schema worldgen_climate_schema();

// The schema of definition worldgen.resources (P1-05 4.2): every field of
// the table of 4.2 in its order, all required. Named checks, each a load
// error at the definition's header (P0-03 7.5), in this order:
// fresh_rain_ordered, fert_slope_ordered, timber_slope_ordered,
// ore_vein_ordered, ore_elev_ordered, ore_slope_ordered, hab_slope_ordered
// (each "<lo> must be below <hi>"), hab_weights_positive ("the hab_weight
// fields must not all be 0").
[[nodiscard]] Schema worldgen_resources_schema();

// The schema of record `region_names` (P1-06 4): central, north, northeast,
// east, southeast, south, southwest, west, northwest, isle, upland (r2), each
// string(32, word), all required, in this order.
[[nodiscard]] Schema region_names_schema();

// The schema of definition worldgen.regions (P1-06 4): every field of the
// table of 4 in its order, all required. Named check, a load error at the
// definition's header (P0-03 7.5):
//   regions_tile_fits_spacing   seed_tile_m <= 2 seed_spacing_m.
[[nodiscard]] Schema worldgen_regions_schema();

// --- The history kinds (P3-01 sections 3 to 5) --------------------------------

// Kind names and directories: race (races/), culture (cultures/), history
// (history/, fixed ids), all required.
inline constexpr std::string_view RACE_KIND = "race";
inline constexpr std::string_view RACE_DIRECTORY = "races";
inline constexpr std::string_view CULTURE_KIND = "culture";
inline constexpr std::string_view CULTURE_DIRECTORY = "cultures";
inline constexpr std::string_view HISTORY_KIND = "history";
inline constexpr std::string_view HISTORY_DIRECTORY = "history";
// The ids of kind history registered so far (P3-01 5).
inline constexpr std::string_view HISTORY_CHRONOLOGY_ID = "chronology";

// The schema of a race definition (P3-01 3): order int(1, 6); name, plural,
// adjective string(32, word); camp enum(good, neutral, evil);
// awakening_share fx(0, 1); lifespan_years int(1, 10000); generation_years
// int(1, 1000); growth_permille int(0, 1000); site_weights record
// race_site_weights (fertility, forage, timber, ore, fresh_water, each
// int(0, 1000)); biomes array(record race_biome {biome ref(biome), weight
// int(-1000, 1000)}, 0, 16); upland, coast int(-1000, 1000); expansion,
// aggression int(0, 1000); settlement_kinds array(enum(hamlet, village,
// town, city, fortress), 1, 5); culture ref(culture); in this order, all
// required. Named checks, in this order: race_generation_below_lifespan,
// race_site_weights_positive, race_biomes_unique,
// race_settlement_kinds_unique.
[[nodiscard]] Schema race_schema();

// The kind-level checks of kind race (P3-01 3), in this order:
// race_order_unique, race_order_dense (not reported when two definitions
// share an order), race_awakening_order.
[[nodiscard]] std::vector<KindCheck> race_kind_checks();

// The schema of record culture_naming (P3-02 3): onsets, nuclei, codas
// array(string(8, syllable), 1, 64); patterns array(string(16,
// name_pattern), 1, 16); min_letters, max_letters int(1, 32); forbidden
// array(string(8, syllable), 0, 32); god_forms, civilization_forms,
// settlement_forms array(string(48, name_form), 1, 8); in this order, all
// required.
[[nodiscard]] Schema culture_naming_schema();

// The schema of a culture definition (P3-01 4): race ref(race), name
// string(32, word), naming record culture_naming, all required. Named
// checks of P3-02 3, in this order: naming_nuclei_nonempty,
// naming_letters_ordered, naming_forbidden_nonempty.
[[nodiscard]] Schema culture_schema();

// The schema of definition history.chronology (P3-01 5.1):
// awakening_jitter_frac fx(0, 0.05), awakening_min_gap_years int(1, 2000),
// age_name_form string(64), all required. Named check
// chronology_age_name_form.
[[nodiscard]] Schema history_chronology_schema();

}  // namespace worldgen::data
