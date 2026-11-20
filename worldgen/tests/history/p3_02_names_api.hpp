#pragma once

// The one place where the P3-02 names tests touch the library's API.
//
// The tests were written from docs/specs/p3-02-names.md before the
// implementation existed (test-writer rule), against the test-owned
// functions and the plain struct `Naming` below. Only this file names
// library symbols. Written first against the interim seams of spec section
// 9, then updated to the seams as recorded in the revised spec (revision of
// 2026-11-19: choose_name, NameAttempt::root, capitalize_root,
// name_kind_text, name_stream, name_forms, NameRegistry::size and the three
// constants):
//
//   worldgen/history/draw.hpp:  history::draw(stream_key, w0, a, b, k),
//                               history::pick(u, n), history::chance(u, Fx p),
//                               history::weighted(u, span<const int64_t>),
//                               TAG_HIST (here looked up as worldgen::TAG_HIST,
//                               beside the P0-01 tags in core/hash.hpp)
//   worldgen/history/names.hpp: history::CultureNaming (fields named as the
//                               TOML fields of section 3), history::NameKind,
//                               history::NameAttempt {valid, name, root},
//                               history::name_attempt, history::NameRegistry
//                               {contains, add, size}, history::make_name,
//                               history::choose_name, history::roman,
//                               history::culture_naming_from,
//                               history::capitalize_root, name_kind_text,
//                               name_stream, name_forms, NAME_ATTEMPT_MAX_BYTES,
//                               NAME_MAX_BYTES, NAME_ATTEMPTS
//
// When the engineer records final signatures that differ (P3-02 9: "the
// engineer records the final signatures after implementation"), adapt this
// file alone. Every contract violation is expected to throw
// worldgen::FatalError.
//
// r5/r6 (P3-02 11, shipped with P3-08, generator 0.17.0), the interim seams
// of 11.5: history::NameFilter {anywhere, at_start, whole},
// history::name_filter_from(const data::ContentSet&), history::root_word,
// history::name_filtered, CultureNaming::god_patterns (after patterns),
// NameAttempt::filtered, and name_attempt / make_name / choose_name with a
// `const NameFilter&` after the CultureNaming. They are used only when
// UNMAPPED_P3_08_API is 1 (p3_08_switch.hpp). With 0 (the 0.16.0 library)
// the filter and god_patterns are not passed, so every r5 expectation fails
// visibly, and the r5-only seams throw ApiMissing (never a FatalError).

#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/names.hpp"
#include "../p3_08_switch.hpp"

namespace names_api {

// Thrown by a seam of P3-02 r5 that the library does not have yet
// (UNMAPPED_P3_08_API 0).
struct ApiMissing : std::logic_error {
    explicit ApiMissing(const char* what) : std::logic_error(what) {}
};

// --- 4.1 keyed draws ------------------------------------------------------------------------------

inline constexpr std::uint64_t tag_hist() { return worldgen::TAG_HIST; }

inline std::uint64_t draw(std::uint64_t key, std::uint64_t w0, std::uint64_t a, std::uint64_t b, std::uint64_t k) {
    return worldgen::history::draw(key, w0, a, b, k);
}
inline std::uint64_t pick(std::uint64_t u, std::uint64_t n) {
    return static_cast<std::uint64_t>(worldgen::history::pick(u, n));
}
inline bool chance(std::uint64_t u, std::int64_t p_raw) {
    return worldgen::history::chance(u, worldgen::Fx::from_raw(p_raw));
}
inline std::uint64_t weighted(std::uint64_t u, const std::vector<std::int64_t>& w) {
    return static_cast<std::uint64_t>(worldgen::history::weighted(u, std::span<const std::int64_t>(w)));
}

// --- Section 3 naming data, test-owned --------------------------------------------------------------

struct Naming {
    std::vector<std::string> onsets;
    std::vector<std::string> nuclei;
    std::vector<std::string> codas;
    std::vector<std::string> patterns;
    // r5 (11.4), after patterns. Unset (a test-made culture of the r4 tests):
    // the library gets god_patterns = patterns, with which r5 gives r4's
    // names (11.4); set to an empty list, the library gets the empty list.
    std::optional<std::vector<std::string>> god_patterns;
    std::int64_t min_letters = 1;
    std::int64_t max_letters = 32;
    std::vector<std::string> forbidden;
    std::vector<std::string> god_forms;
    std::vector<std::string> civilization_forms;
    std::vector<std::string> settlement_forms;
    friend bool operator==(const Naming&, const Naming&) = default;
};

enum class Kind { god, civilization, settlement };

inline worldgen::history::NameKind to_lib(Kind k) {
    switch (k) {
    case Kind::god: return worldgen::history::NameKind::god;
    case Kind::civilization: return worldgen::history::NameKind::civilization;
    case Kind::settlement: return worldgen::history::NameKind::settlement;
    }
    return worldgen::history::NameKind::god;
}

namespace detail {
template <class Dst>
void assign_strings(Dst& dst, const std::vector<std::string>& src) {
    dst.clear();
    for (const std::string& s : src) {
        dst.push_back(typename Dst::value_type(s));
    }
}
template <class Src>
std::vector<std::string> strings_of(const Src& src) {
    std::vector<std::string> out;
    for (const auto& s : src) {
        out.emplace_back(s);
    }
    return out;
}
}  // namespace detail

inline worldgen::history::CultureNaming to_lib(const Naming& n) {
    worldgen::history::CultureNaming c{};
    detail::assign_strings(c.onsets, n.onsets);
    detail::assign_strings(c.nuclei, n.nuclei);
    detail::assign_strings(c.codas, n.codas);
    detail::assign_strings(c.patterns, n.patterns);
#if UNMAPPED_P3_08_API
    detail::assign_strings(c.god_patterns, n.god_patterns.has_value() ? *n.god_patterns : n.patterns);
#endif
    c.min_letters = static_cast<decltype(c.min_letters)>(n.min_letters);
    c.max_letters = static_cast<decltype(c.max_letters)>(n.max_letters);
    detail::assign_strings(c.forbidden, n.forbidden);
    detail::assign_strings(c.god_forms, n.god_forms);
    detail::assign_strings(c.civilization_forms, n.civilization_forms);
    detail::assign_strings(c.settlement_forms, n.settlement_forms);
    return c;
}

inline Naming from_lib(const worldgen::history::CultureNaming& c) {
    Naming n;
    n.onsets = detail::strings_of(c.onsets);
    n.nuclei = detail::strings_of(c.nuclei);
    n.codas = detail::strings_of(c.codas);
    n.patterns = detail::strings_of(c.patterns);
#if UNMAPPED_P3_08_API
    n.god_patterns = detail::strings_of(c.god_patterns);
#endif
    n.min_letters = static_cast<std::int64_t>(c.min_letters);
    n.max_letters = static_cast<std::int64_t>(c.max_letters);
    n.forbidden = detail::strings_of(c.forbidden);
    n.god_forms = detail::strings_of(c.god_forms);
    n.civilization_forms = detail::strings_of(c.civilization_forms);
    n.settlement_forms = detail::strings_of(c.settlement_forms);
    return n;
}

// The record reader of section 2 (`culture_naming_from`), on the `naming`
// record of a loaded culture definition.
inline Naming naming_from_record(const worldgen::data::Record& naming) {
    return from_lib(worldgen::history::culture_naming_from(naming));
}

// --- 11.2, 11.3 the name filter (r5), test-owned ------------------------------------------------------

// The effective lists of 11.2 (the union over every definition).
struct Filter {
    std::vector<std::string> anywhere;
    std::vector<std::string> at_start;
    std::vector<std::string> whole;
};

#if UNMAPPED_P3_08_API
inline worldgen::history::NameFilter to_lib(const Filter& f) {
    worldgen::history::NameFilter out{};
    detail::assign_strings(out.anywhere, f.anywhere);
    detail::assign_strings(out.at_start, f.at_start);
    detail::assign_strings(out.whole, f.whole);
    return out;
}
inline Filter from_lib(const worldgen::history::NameFilter& f) {
    return Filter{detail::strings_of(f.anywhere), detail::strings_of(f.at_start), detail::strings_of(f.whole)};
}
#endif

// W of 11.3 (root_word) and filtered(W) (name_filtered), through the library.
inline std::string root_word(std::string_view form, std::string_view root) {
#if UNMAPPED_P3_08_API
    return std::string(worldgen::history::root_word(form, root));
#else
    (void)form;
    (void)root;
    throw ApiMissing("history::root_word (P3-02 r5 11.5) needs UNMAPPED_P3_08_API");
#endif
}
inline bool name_filtered(const Filter& f, std::string_view w) {
#if UNMAPPED_P3_08_API
    return worldgen::history::name_filtered(to_lib(f), w);
#else
    (void)f;
    (void)w;
    throw ApiMissing("history::name_filtered (P3-02 r5 11.5) needs UNMAPPED_P3_08_API");
#endif
}

// --- 4.2 to 4.4 -----------------------------------------------------------------------------------

// name and root are unspecified when !valid (spec 4.2): tests read them only for valid attempts.
// filtered (r5, 11.5): the r4 rules pass and the filter rejects (valid is then false).
struct Attempt {
    bool valid = false;
    std::string name;
    std::string root;
    bool filtered = false;
};

inline Attempt attempt(const Naming& n, Kind k, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                       std::uint64_t t, const Filter& f = Filter{}) {
#if UNMAPPED_P3_08_API
    const worldgen::history::NameAttempt a =
        worldgen::history::name_attempt(to_lib(n), to_lib(f), to_lib(k), seed, culture, entity, t);
    return Attempt{a.valid, std::string(a.name), std::string(a.root), a.filtered};
#else
    (void)f;
    const worldgen::history::NameAttempt a =
        worldgen::history::name_attempt(to_lib(n), to_lib(k), seed, culture, entity, t);
    return Attempt{a.valid, std::string(a.name), std::string(a.root), false};
#endif
}

// The per-world registry (4.4). Test code only uses contains and add.
class Registry {
public:
    [[nodiscard]] bool contains(std::string_view name) const { return lib_.contains(name); }
    void add(std::string name) { lib_.add(std::move(name)); }
    [[nodiscard]] std::size_t size() const { return static_cast<std::size_t>(lib_.size()); }
    worldgen::history::NameRegistry& lib() { return lib_; }

private:
    worldgen::history::NameRegistry lib_;
};

// The filter argument (r5) defaults to the empty filter, which rejects
// nothing (11.2): r4's behaviour, as the scenarios of section 4 expect.
inline std::string make_name(const Naming& n, std::string_view culture_id, Kind k, std::uint64_t seed,
                             std::uint64_t culture, std::uint64_t entity, Registry& registry,
                             const Filter& f = Filter{}) {
#if UNMAPPED_P3_08_API
    return std::string(worldgen::history::make_name(to_lib(n), to_lib(f), culture_id, to_lib(k), seed, culture,
                                                    entity, registry.lib()));
#else
    (void)f;
    return std::string(
        worldgen::history::make_name(to_lib(n), culture_id, to_lib(k), seed, culture, entity, registry.lib()));
#endif
}

// A variant that converts the naming data once, for the long property loops.
class Namer {
public:
    explicit Namer(const Naming& n, const Filter& f = Filter{}) : lib_(to_lib(n)) {
#if UNMAPPED_P3_08_API
        filter_ = to_lib(f);
#else
        (void)f;
#endif
    }
    std::string make(std::string_view culture_id, Kind k, std::uint64_t seed, std::uint64_t culture,
                     std::uint64_t entity, Registry& registry) const {
#if UNMAPPED_P3_08_API
        return std::string(
            worldgen::history::make_name(lib_, filter_, culture_id, to_lib(k), seed, culture, entity, registry.lib()));
#else
        return std::string(
            worldgen::history::make_name(lib_, culture_id, to_lib(k), seed, culture, entity, registry.lib()));
#endif
    }
    Attempt attempt(Kind k, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity, std::uint64_t t) const {
#if UNMAPPED_P3_08_API
        const worldgen::history::NameAttempt a =
            worldgen::history::name_attempt(lib_, filter_, to_lib(k), seed, culture, entity, t);
        return Attempt{a.valid, std::string(a.name), std::string(a.root), a.filtered};
#else
        const worldgen::history::NameAttempt a = worldgen::history::name_attempt(lib_, to_lib(k), seed, culture, entity, t);
        return Attempt{a.valid, std::string(a.name), std::string(a.root), false};
#endif
    }

private:
    worldgen::history::CultureNaming lib_;
#if UNMAPPED_P3_08_API
    worldgen::history::NameFilter filter_{};
#endif
};

inline std::string roman(std::uint64_t k) { return std::string(worldgen::history::roman(k)); }

// choose_name (spec 9): make_name with the attempt number; when fallback,
// attempt is the first valid attempt, whose name took the numeral.
struct Choice {
    std::string name;
    std::uint64_t attempt = 0;
    bool fallback = false;
};
inline Choice choose_name(const Naming& n, std::string_view culture_id, Kind k, std::uint64_t seed,
                          std::uint64_t culture, std::uint64_t entity, Registry& registry,
                          const Filter& f = Filter{}) {
#if UNMAPPED_P3_08_API
    const worldgen::history::NameChoice c = worldgen::history::choose_name(to_lib(n), to_lib(f), culture_id,
                                                                           to_lib(k), seed, culture, entity,
                                                                           registry.lib());
#else
    (void)f;
    const worldgen::history::NameChoice c =
        worldgen::history::choose_name(to_lib(n), culture_id, to_lib(k), seed, culture, entity, registry.lib());
#endif
    return Choice{std::string(c.name), static_cast<std::uint64_t>(c.attempt), c.fallback};
}

// The effective filter the library builds from a loaded content set
// (name_filter_from, 11.5).
inline Filter filter_from_content(const worldgen::data::ContentSet& content) {
#if UNMAPPED_P3_08_API
    return from_lib(worldgen::history::name_filter_from(content));
#else
    (void)content;
    throw ApiMissing("history::name_filter_from (P3-02 r5 11.5) needs UNMAPPED_P3_08_API");
#endif
}

inline std::string capitalize_root(std::string_view root) { return std::string(worldgen::history::capitalize_root(root)); }
inline std::string kind_text(Kind k) { return std::string(worldgen::history::name_kind_text(to_lib(k))); }
inline std::string kind_stream(Kind k) { return std::string(worldgen::history::name_stream(to_lib(k))); }
inline std::vector<std::string> kind_forms(const Naming& n, Kind k) {
    const worldgen::history::CultureNaming c = to_lib(n);
    return detail::strings_of(worldgen::history::name_forms(c, to_lib(k)));
}
inline constexpr std::uint64_t attempt_max_bytes() { return worldgen::history::NAME_ATTEMPT_MAX_BYTES; }
inline constexpr std::uint64_t name_max_bytes() { return worldgen::history::NAME_MAX_BYTES; }
inline constexpr std::uint64_t name_attempts() { return worldgen::history::NAME_ATTEMPTS; }

}  // namespace names_api
