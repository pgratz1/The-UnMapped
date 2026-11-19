#pragma once

// The one place where the P3-02 names tests touch the library's API.
//
// The tests were written from docs/specs/p3-02-names.md before the
// implementation existed (test-writer rule), against the test-owned
// functions and the plain struct `Naming` below. Only this file names
// library symbols, using the interim seams of spec section 9:
//
//   worldgen/history/draw.hpp:  history::draw(stream_key, w0, a, b, k),
//                               history::pick(u, n), history::chance(u, Fx p),
//                               history::weighted(u, span<const int64_t>),
//                               TAG_HIST (here looked up as worldgen::TAG_HIST,
//                               beside the P0-01 tags in core/hash.hpp)
//   worldgen/history/names.hpp: history::CultureNaming (fields named as the
//                               TOML fields of section 3), history::NameKind,
//                               history::NameAttempt {valid, name},
//                               history::name_attempt, history::NameRegistry
//                               {contains, add}, history::make_name,
//                               history::roman, history::culture_naming_from
//
// When the engineer records final signatures that differ (P3-02 9: "the
// engineer records the final signatures after implementation"), adapt this
// file alone. Every contract violation is expected to throw
// worldgen::FatalError.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/names.hpp"

namespace names_api {

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

// --- 4.2 to 4.4 -----------------------------------------------------------------------------------

struct Attempt {
    bool valid = false;
    std::string name;
};

inline Attempt attempt(const Naming& n, Kind k, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity,
                       std::uint64_t t) {
    const worldgen::history::NameAttempt a =
        worldgen::history::name_attempt(to_lib(n), to_lib(k), seed, culture, entity, t);
    return Attempt{a.valid, std::string(a.name)};
}

// The per-world registry (4.4). Test code only uses contains and add.
class Registry {
public:
    [[nodiscard]] bool contains(std::string_view name) const { return lib_.contains(name); }
    void add(std::string name) { lib_.add(std::move(name)); }
    worldgen::history::NameRegistry& lib() { return lib_; }

private:
    worldgen::history::NameRegistry lib_;
};

inline std::string make_name(const Naming& n, std::string_view culture_id, Kind k, std::uint64_t seed,
                             std::uint64_t culture, std::uint64_t entity, Registry& registry) {
    return std::string(
        worldgen::history::make_name(to_lib(n), culture_id, to_lib(k), seed, culture, entity, registry.lib()));
}

// A variant that converts the naming data once, for the long property loops.
class Namer {
public:
    explicit Namer(const Naming& n) : lib_(to_lib(n)) {}
    std::string make(std::string_view culture_id, Kind k, std::uint64_t seed, std::uint64_t culture,
                     std::uint64_t entity, Registry& registry) const {
        return std::string(
            worldgen::history::make_name(lib_, culture_id, to_lib(k), seed, culture, entity, registry.lib()));
    }
    Attempt attempt(Kind k, std::uint64_t seed, std::uint64_t culture, std::uint64_t entity, std::uint64_t t) const {
        const worldgen::history::NameAttempt a = worldgen::history::name_attempt(lib_, to_lib(k), seed, culture, entity, t);
        return Attempt{a.valid, std::string(a.name)};
    }

private:
    worldgen::history::CultureNaming lib_;
};

inline std::string roman(std::uint64_t k) { return std::string(worldgen::history::roman(k)); }

}  // namespace names_api
