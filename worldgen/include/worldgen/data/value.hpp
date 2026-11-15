#pragma once
// Validated values of the TOML loader (spec P0-03 sections 3, 4 and 8).
//
// A Value is what a schema field holds after validation and defaults: an
// int64, an Fx, a bool, a string (also used for enum and ref fields), an
// ordered array, or a nested record. A Record holds its fields in the
// declaration order of its schema, which is also the order of the canonical
// encoding (section 8).
//
// Accessors with the wrong kind, or get() of a field that does not exist,
// are programming errors and raise worldgen::FatalError (P0-01 4.1).
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"

namespace worldgen::data {

class Value;

// The kind of a Value (P0-03 section 4). Enum and ref fields hold strings;
// their tag in the canonical encoding is the string tag 0x04 (section 8).
enum class ValueKind : std::uint8_t {
    integer,  // int(min, max)
    fixed,    // fx(min, max)
    boolean,  // bool
    string,   // string, enum, ref
    array,    // array(T, min_len, max_len)
    record,   // record(schema)
};

// A validated record (P0-03 sections 3, 7.5): named fields in schema
// declaration order, each present (required fields given, optional fields
// given or defaulted).
class Record {
public:
    Record();
    ~Record();
    Record(const Record& other);
    Record(Record&& other) noexcept;
    Record& operator=(const Record& other);
    Record& operator=(Record&& other) noexcept;

    // Appends a field. Used by the loader, and by schema code to build a
    // record default. Fatal if a field of that name is already present.
    void add(std::string name, Value value);

    // Number of fields.
    [[nodiscard]] std::size_t size() const noexcept;
    // Name and value of the i-th field in declaration order. Fatal if i is
    // out of range.
    [[nodiscard]] const std::string& name_at(std::size_t i) const;
    [[nodiscard]] const Value& value_at(std::size_t i) const;

    // The field of that name, or nullptr.
    [[nodiscard]] const Value* find(std::string_view name) const noexcept;
    // The field of that name. Fatal if absent.
    [[nodiscard]] const Value& get(std::string_view name) const;

    // Typed shortcuts for get(name).as_...(). Fatal if absent or of another kind.
    [[nodiscard]] std::int64_t get_int(std::string_view name) const;
    [[nodiscard]] Fx get_fx(std::string_view name) const;
    [[nodiscard]] bool get_bool(std::string_view name) const;
    [[nodiscard]] const std::string& get_string(std::string_view name) const;
    [[nodiscard]] const std::vector<Value>& get_array(std::string_view name) const;
    [[nodiscard]] const Record& get_record(std::string_view name) const;

    // Same field names, same order, equal values.
    friend bool operator==(const Record& a, const Record& b);

private:
    std::vector<std::string> names_;
    std::vector<Value> values_;
};

// A validated value (P0-03 section 4). Construct with the named functions.
class Value {
public:
    // integer 0 (so that Value can be held in containers).
    Value() = default;

    [[nodiscard]] static Value integer(std::int64_t v);
    [[nodiscard]] static Value fixed(Fx v);
    [[nodiscard]] static Value boolean(bool v);
    // Also the value of enum and ref fields.
    [[nodiscard]] static Value string(std::string v);
    [[nodiscard]] static Value array(std::vector<Value> elements);
    [[nodiscard]] static Value record(Record r);

    [[nodiscard]] ValueKind kind() const noexcept { return kind_; }

    // The held value. Fatal if the kind differs.
    [[nodiscard]] std::int64_t as_int() const;
    [[nodiscard]] Fx as_fx() const;
    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const std::vector<Value>& as_array() const;
    [[nodiscard]] const Record& as_record() const;

    // Same kind and equal content.
    friend bool operator==(const Value& a, const Value& b);

private:
    ValueKind kind_ = ValueKind::integer;
    std::int64_t integer_ = 0;
    Fx fixed_;
    bool boolean_ = false;
    std::string string_;
    std::vector<Value> array_;
    Record record_;
};

}  // namespace worldgen::data
