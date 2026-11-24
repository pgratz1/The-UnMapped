#pragma once

// The formulas of spec P5-03a (docs/specs/p5-03a-relations-followups.md r1) that the spec states completely, as
// test-side functions written from the spec alone (as p5_03_relations_formulas.hpp for P5-03): 3.2's gain, 5.1's
// settlers, 8's A1, 6's undead threshold and 9.3's ruins at summary. The same functions as the reference's
// (p5_03_relations_reference.py peace_gain, settlers_permille, keep_ruling, undead_threshold_025, summary_ruins).

#include <algorithm>
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "p5_03_relations_formulas.hpp"

namespace rel_a {

using rel_f::Shares;

// 3.2: g = years x per_century x 1000 div 100; cap = cap_units x 1000; if M < cap: M = min(cap, M + g).
inline std::int64_t peace_gain(std::int64_t m, std::int64_t years, std::int64_t per_century, std::int64_t cap_units) {
    const std::int64_t g = years * per_century * 1000 / 100;
    const std::int64_t cap = cap_units * 1000;
    return m < cap ? std::min(cap, m + g) : m;
}

// P3-02's pick(u, n) = ((u >> 32) x n) >> 32, n in [1, 2^32].
inline std::int64_t pick(std::uint64_t u, std::uint64_t n) { return static_cast<std::int64_t>(((u >> 32U) * n) >> 32U); }

// 5.1: sigma = conquest_settlers_permille + pick(u, conquest_settlers_span_permille + 1).
inline std::int64_t settlers(std::int64_t least, std::int64_t span, std::uint64_t u) {
    return least + pick(u, static_cast<std::uint64_t>(span + 1));
}

// 8 A1: a ruling race q that was in S and got share 0 from mix takes 1 from the largest share (then the smaller
// race id). `s` ascending race.
inline Shares keep_ruling(const Shares& s, std::int64_t q) {
    if (rel_f::share_of(s, q) > 0) return s;
    std::int64_t big = -1, at = -1;
    for (const auto& [g, x] : s) {
        if (x > big) {
            big = x;
            at = g;
        }
    }
    Shares out;
    bool placed = false;
    for (const auto& [g, x] : s) {
        if (!placed && q < g) {
            out.emplace_back(q, 1);
            placed = true;
        }
        const std::int64_t v = g == at ? x - 1 : x;
        if (v > 0) out.emplace_back(g, v);
    }
    if (!placed) out.emplace_back(q, 1);
    return out;
}

// 6: T = max(undead_min_dead, P_W x undead_dead_permille div 1000); S(r) >= T x 1000 div max(1, M).
inline std::int64_t undead_threshold(std::int64_t min_dead, std::int64_t dead_permille, std::int64_t p_w,
                                     std::int64_t m) {
    const std::int64_t t = std::max(min_dead, p_w * dead_permille / 1000);
    return t * 1000 / std::max<std::int64_t>(1, m);
}

// 9.3: ruins (id, ruined_year, largest census population); more than 15: the union of the 5 oldest (year, id), the
// 5 largest (population descending, id) and the 5 most recent (year descending, id descending), ascending id, and
// the number not listed; otherwise every id and 0.
struct Ruin {
    std::int64_t id = 0, year = 0, size = 0;
};
inline std::pair<std::vector<std::int64_t>, std::int64_t> summary_ruins(std::vector<Ruin> ruins) {
    std::set<std::int64_t> ids;
    if (ruins.size() <= 15U) {
        for (const Ruin& r : ruins) ids.insert(r.id);
        return {std::vector<std::int64_t>(ids.begin(), ids.end()), 0};
    }
    auto take = [&](auto less) {
        std::sort(ruins.begin(), ruins.end(), less);
        for (std::size_t i = 0; i < 5U; ++i) ids.insert(ruins[i].id);
    };
    take([](const Ruin& a, const Ruin& b) { return std::tie(a.year, a.id) < std::tie(b.year, b.id); });
    take([](const Ruin& a, const Ruin& b) { return a.size != b.size ? a.size > b.size : a.id < b.id; });
    take([](const Ruin& a, const Ruin& b) { return a.year != b.year ? a.year > b.year : a.id > b.id; });
    return {std::vector<std::int64_t>(ids.begin(), ids.end()),
            static_cast<std::int64_t>(ruins.size()) - static_cast<std::int64_t>(ids.size())};
}

}  // namespace rel_a
