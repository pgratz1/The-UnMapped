#pragma once

// Project fatal error (spec P0-01 section 4.1).

#include <stdexcept>
#include <string>

namespace worldgen {

// The project fatal error (P0-01 4.1). Raised on every contract violation in
// the deterministic core: overflow, division by zero, square root of a
// negative number, empty weight set, invalid argument. Never caught inside
// worldgen/; worldgen-cli catches it at top level and exits with code 1.
// Derived from std::exception (through std::runtime_error); what() returns
// the message.
class FatalError : public std::runtime_error {
public:
    explicit FatalError(const std::string& message) : std::runtime_error(message) {}
    explicit FatalError(const char* message) : std::runtime_error(message) {}
    FatalError(const FatalError&) = default;
    FatalError& operator=(const FatalError&) = default;
    ~FatalError() override;  // defined out of line so the vtable has one home
};

// Throws FatalError with the given message (P0-01 4.1).
[[noreturn]] void fatal(const char* message);

}  // namespace worldgen
