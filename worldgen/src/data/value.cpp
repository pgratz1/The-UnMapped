// Validated values (P0-03 sections 4, 8).

#include "worldgen/data/value.hpp"

#include <string>
#include <utility>

#include "worldgen/core/error.hpp"

namespace worldgen::data {

// --- Record -------------------------------------------------------------------

Record::Record() = default;
Record::~Record() = default;
Record::Record(const Record& other) = default;
Record::Record(Record&& other) noexcept = default;
Record& Record::operator=(const Record& other) = default;
Record& Record::operator=(Record&& other) noexcept = default;

void Record::add(std::string name, Value value) {
    if (find(name) != nullptr) {
        fatal("Record::add: field already present");
    }
    names_.push_back(std::move(name));
    values_.push_back(std::move(value));
}

std::size_t Record::size() const noexcept {
    return names_.size();
}

const std::string& Record::name_at(std::size_t i) const {
    if (i >= names_.size()) {
        fatal("Record::name_at: index out of range");
    }
    return names_[i];
}

const Value& Record::value_at(std::size_t i) const {
    if (i >= values_.size()) {
        fatal("Record::value_at: index out of range");
    }
    return values_[i];
}

const Value* Record::find(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == name) {
            return &values_[i];
        }
    }
    return nullptr;
}

const Value& Record::get(std::string_view name) const {
    const Value* v = find(name);
    if (v == nullptr) {
        fatal("Record::get: no such field");
    }
    return *v;
}

std::int64_t Record::get_int(std::string_view name) const {
    return get(name).as_int();
}
Fx Record::get_fx(std::string_view name) const {
    return get(name).as_fx();
}
bool Record::get_bool(std::string_view name) const {
    return get(name).as_bool();
}
const std::string& Record::get_string(std::string_view name) const {
    return get(name).as_string();
}
const std::vector<Value>& Record::get_array(std::string_view name) const {
    return get(name).as_array();
}
const Record& Record::get_record(std::string_view name) const {
    return get(name).as_record();
}

bool operator==(const Record& a, const Record& b) {
    return a.names_ == b.names_ && a.values_ == b.values_;
}

// --- Value --------------------------------------------------------------------

Value Value::integer(std::int64_t v) {
    Value out;
    out.kind_ = ValueKind::integer;
    out.integer_ = v;
    return out;
}

Value Value::fixed(Fx v) {
    Value out;
    out.kind_ = ValueKind::fixed;
    out.fixed_ = v;
    return out;
}

Value Value::boolean(bool v) {
    Value out;
    out.kind_ = ValueKind::boolean;
    out.boolean_ = v;
    return out;
}

Value Value::string(std::string v) {
    Value out;
    out.kind_ = ValueKind::string;
    out.string_ = std::move(v);
    return out;
}

Value Value::array(std::vector<Value> elements) {
    Value out;
    out.kind_ = ValueKind::array;
    out.array_ = std::move(elements);
    return out;
}

Value Value::record(Record r) {
    Value out;
    out.kind_ = ValueKind::record;
    out.record_ = std::move(r);
    return out;
}

std::int64_t Value::as_int() const {
    if (kind_ != ValueKind::integer) {
        fatal("Value::as_int: not an integer");
    }
    return integer_;
}

Fx Value::as_fx() const {
    if (kind_ != ValueKind::fixed) {
        fatal("Value::as_fx: not an fx value");
    }
    return fixed_;
}

bool Value::as_bool() const {
    if (kind_ != ValueKind::boolean) {
        fatal("Value::as_bool: not a bool");
    }
    return boolean_;
}

const std::string& Value::as_string() const {
    if (kind_ != ValueKind::string) {
        fatal("Value::as_string: not a string");
    }
    return string_;
}

const std::vector<Value>& Value::as_array() const {
    if (kind_ != ValueKind::array) {
        fatal("Value::as_array: not an array");
    }
    return array_;
}

const Record& Value::as_record() const {
    if (kind_ != ValueKind::record) {
        fatal("Value::as_record: not a record");
    }
    return record_;
}

bool operator==(const Value& a, const Value& b) {
    if (a.kind_ != b.kind_) {
        return false;
    }
    switch (a.kind_) {
        case ValueKind::integer: return a.integer_ == b.integer_;
        case ValueKind::fixed: return a.fixed_ == b.fixed_;
        case ValueKind::boolean: return a.boolean_ == b.boolean_;
        case ValueKind::string: return a.string_ == b.string_;
        case ValueKind::array: return a.array_ == b.array_;
        case ValueKind::record: return a.record_ == b.record_;
    }
    return false;
}

}  // namespace worldgen::data
