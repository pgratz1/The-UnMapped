// Kind schemas and the kind registry (P0-03 sections 4, 7.1, 7.5).

#include "worldgen/data/schema.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/text.hpp"
#include "worldgen/core/decimal.hpp"
#include "worldgen/core/error.hpp"

namespace worldgen::data {

// --- Grammars -----------------------------------------------------------------

bool is_valid_name(std::string_view text) noexcept {
    if (text.empty() || !detail::is_lower(text[0])) {
        return false;
    }
    for (const char c : text) {
        if (!(detail::is_lower(c) || detail::is_digit(c) || c == '_')) {
            return false;
        }
    }
    return true;
}

bool is_valid_id(std::string_view text) noexcept {
    return text.size() <= 64 && is_valid_name(text);
}

Fx decimal_literal(std::string_view text) {
    const DecimalParse p = parse_decimal(text);
    if (!p.ok()) {
        fatal("decimal_literal: not a valid decimal");
    }
    return p.value;
}

StringGrammar id_grammar() noexcept {
    return StringGrammar{"an id ([a-z][a-z0-9_]*, at most 64 bytes)", &is_valid_id};
}

// --- Type ---------------------------------------------------------------------

Type Type::integer(std::int64_t min, std::int64_t max) {
    if (min > max) {
        fatal("Type::integer: min > max");
    }
    Type t;
    t.kind_ = TypeKind::integer;
    t.int_min_ = min;
    t.int_max_ = max;
    return t;
}

Type Type::fixed(Fx min, Fx max) {
    if (min > max) {
        fatal("Type::fixed: min > max");
    }
    Type t;
    t.kind_ = TypeKind::fixed;
    t.fx_min_ = min;
    t.fx_max_ = max;
    return t;
}

Type Type::fixed(std::string_view min, std::string_view max) {
    return fixed(decimal_literal(min), decimal_literal(max));
}

Type Type::boolean() {
    Type t;
    t.kind_ = TypeKind::boolean;
    return t;
}

Type Type::string(std::uint64_t max_bytes) {
    if (max_bytes == 0) {
        fatal("Type::string: max_bytes is 0");
    }
    Type t;
    t.kind_ = TypeKind::string;
    t.max_bytes_ = max_bytes;
    return t;
}

Type Type::string(std::uint64_t max_bytes, StringGrammar grammar) {
    if (grammar.matches == nullptr || grammar.description == nullptr) {
        fatal("Type::string: grammar without a predicate or description");
    }
    Type t = string(max_bytes);
    t.grammar_ = grammar;
    return t;
}

Type Type::enumeration(std::vector<std::string> values) {
    if (values.empty()) {
        fatal("Type::enumeration: no values");
    }
    // Pairwise comparison: no sorting, and the values keep the order given.
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) {
                fatal("Type::enumeration: duplicate value");
            }
        }
    }
    Type t;
    t.kind_ = TypeKind::enumeration;
    t.enum_values_ = std::move(values);
    return t;
}

Type Type::reference(std::string kind) {
    if (!is_valid_name(kind)) {
        fatal("Type::reference: invalid kind name");
    }
    Type t;
    t.kind_ = TypeKind::reference;
    t.ref_kind_ = std::move(kind);
    return t;
}

Type Type::array(Type element, std::uint64_t min_len, std::uint64_t max_len) {
    if (min_len > max_len) {
        fatal("Type::array: min_len > max_len");
    }
    Type t;
    t.kind_ = TypeKind::array;
    t.element_ = std::make_shared<const Type>(std::move(element));
    t.min_len_ = min_len;
    t.max_len_ = max_len;
    return t;
}

Type Type::record(Schema schema) {
    Type t;
    t.kind_ = TypeKind::record;
    t.schema_ = std::make_shared<const Schema>(std::move(schema));
    return t;
}

namespace {
void require_kind(TypeKind have, TypeKind want) {
    if (have != want) {
        fatal("Type: accessor used on a type of another kind");
    }
}
}  // namespace

std::int64_t Type::int_min() const {
    require_kind(kind_, TypeKind::integer);
    return int_min_;
}
std::int64_t Type::int_max() const {
    require_kind(kind_, TypeKind::integer);
    return int_max_;
}
Fx Type::fx_min() const {
    require_kind(kind_, TypeKind::fixed);
    return fx_min_;
}
Fx Type::fx_max() const {
    require_kind(kind_, TypeKind::fixed);
    return fx_max_;
}
std::uint64_t Type::max_bytes() const {
    require_kind(kind_, TypeKind::string);
    return max_bytes_;
}
const StringGrammar& Type::grammar() const {
    require_kind(kind_, TypeKind::string);
    return grammar_;
}
const std::vector<std::string>& Type::enum_values() const {
    require_kind(kind_, TypeKind::enumeration);
    return enum_values_;
}
const std::string& Type::ref_kind() const {
    require_kind(kind_, TypeKind::reference);
    return ref_kind_;
}
const Type& Type::element() const {
    require_kind(kind_, TypeKind::array);
    return *element_;
}
std::uint64_t Type::min_len() const {
    require_kind(kind_, TypeKind::array);
    return min_len_;
}
std::uint64_t Type::max_len() const {
    require_kind(kind_, TypeKind::array);
    return max_len_;
}
const Schema& Type::schema() const {
    require_kind(kind_, TypeKind::record);
    return *schema_;
}

bool Type::accepts(const Value& v) const {
    switch (kind_) {
        case TypeKind::integer:
            return v.kind() == ValueKind::integer && v.as_int() >= int_min_ && v.as_int() <= int_max_;
        case TypeKind::fixed:
            return v.kind() == ValueKind::fixed && v.as_fx() >= fx_min_ && v.as_fx() <= fx_max_;
        case TypeKind::boolean:
            return v.kind() == ValueKind::boolean;
        case TypeKind::string: {
            if (v.kind() != ValueKind::string) {
                return false;
            }
            const std::string& s = v.as_string();
            return static_cast<std::uint64_t>(s.size()) <= max_bytes_ && detail::first_forbidden_control(s) < 0 &&
                   (grammar_.matches == nullptr || grammar_.matches(s));
        }
        case TypeKind::enumeration:
            return v.kind() == ValueKind::string &&
                   std::find(enum_values_.begin(), enum_values_.end(), v.as_string()) != enum_values_.end();
        case TypeKind::reference:
            return v.kind() == ValueKind::string && is_valid_id(v.as_string());
        case TypeKind::array: {
            if (v.kind() != ValueKind::array) {
                return false;
            }
            const auto& items = v.as_array();
            const auto n = static_cast<std::uint64_t>(items.size());
            if (n < min_len_ || n > max_len_) {
                return false;
            }
            return std::all_of(items.begin(), items.end(), [this](const Value& e) { return element_->accepts(e); });
        }
        case TypeKind::record: {
            if (v.kind() != ValueKind::record) {
                return false;
            }
            const Record& r = v.as_record();
            const auto& fields = schema_->fields();
            if (r.size() != fields.size()) {
                return false;
            }
            for (std::size_t i = 0; i < fields.size(); ++i) {
                if (r.name_at(i) != fields[i].name || !fields[i].type.accepts(r.value_at(i))) {
                    return false;
                }
            }
            return std::all_of(schema_->checks().begin(), schema_->checks().end(),
                               [&r](const Check& c) { return !c.rule(r).has_value(); });
        }
    }
    return false;
}

// --- Field, Schema ------------------------------------------------------------

Field Field::required(std::string name, Type type) {
    if (!is_valid_name(name)) {
        fatal("Field::required: invalid field name");
    }
    return Field{std::move(name), std::move(type), std::nullopt};
}

Field Field::optional(std::string name, Type type, Value default_value) {
    if (!is_valid_name(name)) {
        fatal("Field::optional: invalid field name");
    }
    if (!type.accepts(default_value)) {
        fatal("Field::optional: the default value is not valid for the field type");
    }
    return Field{std::move(name), std::move(type), std::move(default_value)};
}

Schema::Schema(std::vector<Field> fields, std::vector<Check> checks)
    : fields_(std::move(fields)), checks_(std::move(checks)) {
    for (std::size_t i = 0; i < fields_.size(); ++i) {
        for (std::size_t j = i + 1; j < fields_.size(); ++j) {
            if (fields_[i].name == fields_[j].name) {
                fatal("Schema: duplicate field name");
            }
        }
    }
    for (const Check& c : checks_) {
        if (c.name.empty() || !c.rule) {
            fatal("Schema: check without a name or rule");
        }
    }
}

const Field* Schema::find(std::string_view name) const noexcept {
    for (const Field& f : fields_) {
        if (f.name == name) {
            return &f;
        }
    }
    return nullptr;
}

// --- KindRegistry -------------------------------------------------------------

void KindRegistry::add(Kind kind) {
    if (!is_valid_name(kind.name)) {
        fatal("KindRegistry::add: invalid kind name");
    }
    if (!is_valid_name(kind.directory)) {
        fatal("KindRegistry::add: invalid directory name");
    }
    for (const Kind& k : kinds_) {
        if (k.name == kind.name) {
            fatal("KindRegistry::add: kind name already registered");
        }
        if (k.directory == kind.directory) {
            fatal("KindRegistry::add: directory already used by another kind");
        }
    }
    const auto pos = std::lower_bound(kinds_.begin(), kinds_.end(), kind.name,
                                      [](const Kind& k, const std::string& n) { return detail::bytes_less(k.name, n); });
    kinds_.insert(pos, std::move(kind));
}

const Kind* KindRegistry::find(std::string_view name) const noexcept {
    for (const Kind& k : kinds_) {
        if (k.name == name) {
            return &k;
        }
    }
    return nullptr;
}

}  // namespace worldgen::data
