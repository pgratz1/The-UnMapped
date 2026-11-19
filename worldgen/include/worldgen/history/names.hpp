#pragma once

// The name generator (spec P3-02): pronounceable, world-unique names for
// gods, civilizations and settlements, drawn from a culture's naming data
// with keyed draws (P3-02 4.1) and checked against a per-world registry.
//
// Names are ASCII only: letters, spaces, apostrophes and hyphens, plus a
// Roman numeral after the fallback of P3-02 4.3.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/value.hpp"

namespace worldgen::history {

// The culture_naming record (P3-02 3), fields in declaration order. Filled
// from a validated record by culture_naming_from, or built directly.
struct CultureNaming {
    std::vector<std::string> onsets;    // "" allowed
    std::vector<std::string> nuclei;    // "" not allowed
    std::vector<std::string> codas;     // "" allowed
    std::vector<std::string> patterns;  // symbols O, N, C, ' and -
    std::int64_t min_letters = 1;       // letters a-z of a root, separators not counted
    std::int64_t max_letters = 32;
    std::vector<std::string> forbidden;  // substrings a root must not contain
    std::vector<std::string> god_forms;  // each holds exactly one {root}
    std::vector<std::string> civilization_forms;
    std::vector<std::string> settlement_forms;
};

// Reads the naming record of a culture (the value of its `naming` field,
// P3-01 4, P3-02 3). The record has been validated by the loader; a
// missing field or a field of the wrong kind is fatal.
[[nodiscard]] CultureNaming culture_naming_from(const data::Record& naming);

// The name kinds of P3-02 4.5.
enum class NameKind : std::uint8_t {
    god,
    civilization,
    settlement,
};

// "god", "civilization" or "settlement".
[[nodiscard]] std::string_view name_kind_text(NameKind kind) noexcept;

// The stream name of a kind: hist.names.<kind> (P3-02 4.5).
[[nodiscard]] std::string_view name_stream(NameKind kind) noexcept;

// The forms of a kind: naming.<kind>_forms.
[[nodiscard]] const std::vector<std::string>& name_forms(const CultureNaming& naming, NameKind kind) noexcept;

// The longest name of one attempt (P3-02 4.2) and the hard limit on any
// name, with a numeral (P3-02 4.4), in bytes.
inline constexpr std::size_t NAME_ATTEMPT_MAX_BYTES = 48;
inline constexpr std::size_t NAME_MAX_BYTES = 64;
// Attempts per name (P3-02 4.3).
inline constexpr std::uint64_t NAME_ATTEMPTS = 64;

// The result of one attempt (P3-02 4.2): root is the drawn root before
// capitalization, name the form with {root} replaced by Cap(root). When
// valid is false, name and root are unspecified (P3-02 4.2): only valid is
// defined, and callers must not rely on either.
struct NameAttempt {
    bool valid = false;
    std::string name;
    std::string root;
};

// Cap(root) of P3-02 4.2: upper-cases the first character if it is a
// letter a-z and every letter a-z that directly follows a '-'.
[[nodiscard]] std::string capitalize_root(std::string_view root);

// One attempt (P3-02 4.2) with u(j) = draw(stream_key(seed, hist.names.<K>),
// attempt, culture, entity, j). Fatal if a list it picks from is empty, a
// pattern holds a symbol other than O, N, C, ' and -, or the chosen form
// does not hold exactly one {root}.
[[nodiscard]] NameAttempt name_attempt(const CultureNaming& naming, NameKind kind, std::uint64_t seed,
                                       std::uint64_t culture, std::uint64_t entity, std::uint64_t attempt);

// The per-world set of names (P3-02 4.4), compared as exact bytes. Used for
// membership only; never iterated by the generator.
class NameRegistry {
public:
    [[nodiscard]] bool contains(std::string_view name) const;
    // Adds a name; adding a name already present changes nothing.
    void add(std::string name);
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::set<std::string, std::less<>> names_;
};

// A world-unique name (P3-02 4.3): the first valid attempt t = 0 ... 63 not
// in the registry; failing that, the first valid attempt followed by " " and
// the smallest Roman numeral >= II that makes it new. The name is added to
// the registry. culture is the culture number c (culture.id), entity the
// entity id e. Fatal ("names: culture '<culture_id>' produced no valid <K>
// name in 64 attempts") if no attempt is valid, and ("names: a name longer
// than 64 bytes") if the result exceeds NAME_MAX_BYTES.
[[nodiscard]] std::string make_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind,
                                    std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                                    NameRegistry& registry);

// make_name with the attempt that produced the name: attempt is the
// number t of the accepted attempt, or, when fallback is true, of the
// first valid attempt whose name took the numeral.
struct NameChoice {
    std::string name;
    std::uint64_t attempt = 0;
    bool fallback = false;
};
[[nodiscard]] NameChoice choose_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind,
                                     std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                                     NameRegistry& registry);

// The Roman numeral of k (P1-06 RG11: subtractive form, thousands as
// repeated M). Fatal if k < 1 or k > 100000.
[[nodiscard]] std::string roman(std::uint64_t k);

}  // namespace worldgen::history
