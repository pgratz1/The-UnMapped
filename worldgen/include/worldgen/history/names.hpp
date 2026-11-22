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
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"

namespace worldgen::history {

// The culture_naming record (P3-02 3), fields in declaration order. Filled
// from a validated record by culture_naming_from, or built directly.
struct CultureNaming {
    std::vector<std::string> onsets;    // "" allowed
    std::vector<std::string> nuclei;    // "" not allowed
    std::vector<std::string> codas;     // "" allowed
    std::vector<std::string> patterns;  // symbols O, N, C, ' and -
    std::vector<std::string> god_patterns;  // r5 (P3-02 11.4): the shapes of a god's root
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
    // r5 (P3-02 11.5): true when the r4 rules pass and the name filter
    // rejects the root word (valid is then false).
    bool filtered = false;
    // r15 (P3-02 14.2): true when the r4 rules, the filter and the
    // triple-letter rule pass and two consecutive syllables repeat (valid is
    // then false).
    bool repeated = false;
};

// P3-02 r15 14.2: whether an attempt with this pattern and these drawn
// strings (one per O, N and C symbol of the pattern, in pattern order;
// separators draw nothing) has a repeated syllable: a boundary at the
// first symbol, before every O, before every N not directly after an O and
// at every separator; syllable i's text s_i and its text b_i without its
// codas; repeated when some s_i is not empty and s_i = s_{i+1} or
// s_i = b_{i+1}. Fatal when the drawn strings do not match the pattern.
[[nodiscard]] bool repeated_syllable(std::string_view pattern, const std::vector<std::string>& drawn);

// The name filter of P3-02 r5 11.2: the effective lists, the unions over
// every name_filter definition in ascending id. Membership only: the order
// of entries never changes a result.
struct NameFilter {
    std::vector<std::string> anywhere;
    std::vector<std::string> at_start;
    std::vector<std::string> whole;
};

// The effective lists of loaded content (every definition of kind
// name_filter, ascending id). Empty lists when the kind has none.
[[nodiscard]] NameFilter name_filter_from(const data::ContentSet& content);

// W of P3-02 11.3: lower(A') + root + lower(B'), with A the form before
// {root} and A' its part after the last space, B the form after {root} and
// B' its part before the first space. Fatal unless the form holds exactly
// one {root}.
[[nodiscard]] std::string root_word(std::string_view form, std::string_view root);

// filtered(W) of P3-02 11.3: an `anywhere` entry inside W without its ' and
// -, a part of W (the runs of a-z between ' and -) starting with an
// `at_start` entry, or a part equal to a `whole` entry.
[[nodiscard]] bool name_filtered(const NameFilter& filter, std::string_view root_word);

// The stage check names_form_text of P3-02 11.3 (r6): for every culture
// (ascending id), kind (god, civilization, settlement) and form, the form's
// letters fused to the root (A', B') and its fixed words hold no filtered
// fragment. The first failure as "history: culture '<id>' <kind>_forms[<i>]
// '<form>' holds the filtered text '<entry>'", or std::nullopt.
[[nodiscard]] std::optional<std::string> names_form_text_problem(const data::ContentSet& content,
                                                                 const NameFilter& filter);
// names_form_text_problem as a FatalError.
void check_names_form_text(const data::ContentSet& content, const NameFilter& filter);

// Cap(root) of P3-02 4.2: upper-cases the first character if it is a
// letter a-z and every letter a-z that directly follows a '-'.
[[nodiscard]] std::string capitalize_root(std::string_view root);

// One attempt (P3-02 4.2) with u(j) = draw(stream_key(seed, hist.names.<K>),
// attempt, culture, entity, j). Fatal if a list it picks from is empty, a
// pattern holds a symbol other than O, N, C, ' and -, or the chosen form
// does not hold exactly one {root}.
// r5: gods take their pattern from god_patterns, every other kind from
// patterns; an attempt that passes the r4 rules is also invalid (and
// `filtered`) when its root word is filtered.
[[nodiscard]] NameAttempt name_attempt(const CultureNaming& naming, const NameFilter& filter, NameKind kind,
                                       std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                                       std::uint64_t attempt);
// The same with an empty filter.
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

// P3-02 r10 12.2: the cores of the civilization and settlement names a
// history run has drawn. The core of an attempt is its root without ' and -.
// A core is near the set when one of its cores equals it or is one letter
// apart (Levenshtein distance 1). Membership only; never iterated by the
// generator except to compare.
class NameCores {
public:
    [[nodiscard]] bool near(std::string_view core) const;
    void add(std::string core);
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::set<std::string, std::less<>> cores_;
};

// The core of a root: its letters with every ' and - removed.
[[nodiscard]] std::string name_core(std::string_view root);
// True when the two strings are equal or at Levenshtein distance 1.
[[nodiscard]] bool within_one_edit(std::string_view a, std::string_view b) noexcept;

// A world-unique name (P3-02 4.3): the first valid attempt t = 0 ... 63 not
// in the registry; failing that, the first valid attempt followed by " " and
// the smallest Roman numeral >= II that makes it new. The name is added to
// the registry. culture is the culture number c (culture.id), entity the
// entity id e. Fatal ("names: culture '<culture_id>' produced no valid <K>
// name in 64 attempts") if no attempt is valid, and ("names: a name longer
// than 64 bytes") if the result exceeds NAME_MAX_BYTES.
[[nodiscard]] std::string make_name(const CultureNaming& naming, const NameFilter& filter, std::string_view culture_id,
                                    NameKind kind, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                                    NameRegistry& registry);
// The same with an empty filter.
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
[[nodiscard]] NameChoice choose_name(const CultureNaming& naming, const NameFilter& filter,
                                     std::string_view culture_id, NameKind kind, std::uint64_t seed,
                                     std::uint64_t culture, std::uint64_t entity, NameRegistry& registry);
// P3-02 r10 12.2 (generator 0.19.0; the history stage's civilizations and
// settlements): as above, but an attempt whose core is near `cores` is
// skipped like a name in the registry; the accepted name's core (or the
// fallback's first core) is added to `cores`.
[[nodiscard]] NameChoice choose_name(const CultureNaming& naming, const NameFilter& filter,
                                     std::string_view culture_id, NameKind kind, std::uint64_t seed,
                                     std::uint64_t culture, std::uint64_t entity, NameRegistry& registry,
                                     NameCores& cores);
// The same with an empty filter.
[[nodiscard]] NameChoice choose_name(const CultureNaming& naming, std::string_view culture_id, NameKind kind,
                                     std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                                     NameRegistry& registry);

// The Roman numeral of k (P1-06 RG11: subtractive form, thousands as
// repeated M). Fatal if k < 1 or k > 100000.
[[nodiscard]] std::string roman(std::uint64_t k);

}  // namespace worldgen::history
