#pragma once

// The formulas of spec P5-03 (docs/specs/p5-03-race-relations.md r1) that the spec states completely, as
// test-side functions: the tests check them on hand-computed values of the spec and use them as the oracle of the
// row checks (history_relations_*_test.cpp). Needs no library name: written from the spec alone. Every value is
// int64; `div` is floor division of non-negative values (the spec's), floor_div where a value can be negative.

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rel_f {

// Codes (6.2.3, 6.3).
inline constexpr std::int64_t HATRED = 0, WARINESS = 1, FRIENDSHIP = 2;
inline constexpr std::int64_t SUBJECT = 0, TOLERATED = 1, ACCEPTED = 2, RULING = 3;

inline std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

// 6.2.1: B(a, b) = camp_table[min][max] + floor_div(stance(a->b) + stance(b->a), 2).
inline std::int64_t base(std::int64_t camp_value, std::int64_t stance_ab, std::int64_t stance_ba) {
    return camp_value + floor_div(stance_ab + stance_ba, 2);
}

// 6.2.1: D = B + M / 1000 (C++'s truncation toward zero; M in milli-units).
inline std::int64_t disposition(std::int64_t b, std::int64_t memory_milli) { return b + memory_milli / 1000; }

// 6.2.2: memory moved by an event of delta units (negative: lost), clamped to [min x 1000, max x 1000].
inline std::int64_t move_memory(std::int64_t memory_milli, std::int64_t delta_units, std::int64_t memory_min,
                                std::int64_t memory_max) {
    return std::clamp(memory_milli + delta_units * 1000, memory_min * 1000, memory_max * 1000);
}

// 6.2.2 decay: d = years x rate x 1000 div 100; toward 0, never across it.
inline std::int64_t decay(std::int64_t memory_milli, std::int64_t years, std::int64_t rate_per_century) {
    const std::int64_t d = years * rate_per_century * 1000 / 100;
    if (memory_milli > 0) return std::max<std::int64_t>(0, memory_milli - d);
    if (memory_milli < 0) return std::min<std::int64_t>(0, memory_milli + d);
    return 0;
}

// 6.2.3: the plain band (at a meeting; and the band of an unmet pair, 6.3.5).
inline std::int64_t plain_band(std::int64_t d, std::int64_t hatred_max, std::int64_t friend_min) {
    if (d <= hatred_max) return HATRED;
    if (d >= friend_min) return FRIENDSHIP;
    return WARINESS;
}

// 6.2.3: the band after one step's test with the hysteresis m (at most one change).
inline std::int64_t next_band(std::int64_t band, std::int64_t d, std::int64_t h, std::int64_t f, std::int64_t m) {
    if (band == WARINESS) {
        if (d <= h) return HATRED;
        if (d >= f) return FRIENDSHIP;
        return WARINESS;
    }
    if (band == HATRED) return d > h + m ? WARINESS : HATRED;
    return d < f - m ? WARINESS : FRIENDSHIP;
}

// A share list: (race, share per mille), ascending race.
using Shares = std::vector<std::pair<std::int64_t, std::int64_t>>;

inline std::int64_t share_of(const Shares& s, std::int64_t race) {
    for (const auto& [g, x] : s) {
        if (g == race) return x;
    }
    return 0;
}

// Largest remainder of 6.3.2: values c_g over T; shares c_g div T, the 1000 - sum left over one each in the order
// (c_g mod T descending, race ascending). Races of share 0 dropped. `cs` ascending race.
inline Shares largest_remainder(const std::vector<std::pair<std::int64_t, std::int64_t>>& cs, std::int64_t total) {
    Shares out;
    std::vector<std::pair<std::int64_t, std::int64_t>> rem;   // (c mod T, race)
    std::int64_t sum = 0;
    for (const auto& [g, c] : cs) {
        out.emplace_back(g, c / total);
        sum += c / total;
        rem.emplace_back(c % total, g);
    }
    std::sort(rem.begin(), rem.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    for (std::int64_t i = 0; i < 1000 - sum; ++i) {
        const std::int64_t g = rem[static_cast<std::size_t>(i) % rem.size()].second;
        for (auto& e : out) {
            if (e.first == g) ++e.second;
        }
    }
    Shares kept;
    for (const auto& e : out) {
        if (e.second > 0) kept.push_back(e);
    }
    return kept;
}

// 6.3.2 mix(s, G, n, P_b): s's shares S with weight P_b take n people of shares G.
inline Shares mix(const Shares& s, std::int64_t p_before, const Shares& g, std::int64_t n) {
    std::vector<std::int64_t> races;
    for (const auto& e : s) races.push_back(e.first);
    for (const auto& e : g) races.push_back(e.first);
    std::sort(races.begin(), races.end());
    races.erase(std::unique(races.begin(), races.end()), races.end());
    std::vector<std::pair<std::int64_t, std::int64_t>> cs;
    for (const std::int64_t r : races) cs.emplace_back(r, share_of(s, r) * p_before + share_of(g, r) * n);
    return largest_remainder(cs, p_before + n);
}

// Shares scaled to fill `room` per mille by the largest-remainder rule with weights their shares (6.3.3 item 2,
// 6.5): c_g = share_g x room, T = sum of shares; the left-over goes as in 6.3.2 up to `room`.
inline Shares scale_to(const Shares& s, std::int64_t room) {
    std::int64_t total = 0;
    for (const auto& e : s) total += e.second;
    Shares out;
    if (total == 0 || room == 0) return out;
    std::vector<std::pair<std::int64_t, std::int64_t>> rem;
    std::int64_t sum = 0;
    for (const auto& [g, x] : s) {
        const std::int64_t c = x * room;
        out.emplace_back(g, c / total);
        sum += c / total;
        rem.emplace_back(c % total, g);
    }
    std::sort(rem.begin(), rem.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    for (std::int64_t i = 0; i < room - sum; ++i) {
        const std::int64_t g = rem[static_cast<std::size_t>(i) % rem.size()].second;
        for (auto& e : out) {
            if (e.first == g) ++e.second;
        }
    }
    Shares kept;
    for (const auto& e : out) {
        if (e.second > 0) kept.push_back(e);
    }
    return kept;
}

// 6.3.3 item 2: the conquerors' share x + (1000 - x) x permille div 1000.
inline std::int64_t conquest_share(std::int64_t x, std::int64_t settlers_permille) {
    return x + (1000 - x) * settlers_permille / 1000;
}

// 6.3.3 items 1 and 2: the shares after race q takes a settlement (the standings are not modelled here).
inline Shares conquer(const Shares& s, std::int64_t q, std::int64_t settlers_permille) {
    const std::int64_t mine = conquest_share(share_of(s, q), settlers_permille);
    Shares others;
    for (const auto& e : s) {
        if (e.first != q) others.push_back(e);
    }
    Shares out = scale_to(others, 1000 - mine);
    out.emplace_back(q, mine);
    std::sort(out.begin(), out.end());
    return out;
}

// 6.3.4: drift over delta years toward q, then fade below fade_permille.
inline Shares drift(const Shares& s, std::int64_t q, std::int64_t rate_per_century, std::int64_t delta,
                    std::int64_t fade_permille) {
    Shares out = s;
    std::int64_t gained = 0;
    for (auto& e : out) {
        if (e.first == q) continue;
        const std::int64_t lost = std::min(e.second, e.second * rate_per_century * delta / 100000);
        e.second -= lost;
        gained += lost;
    }
    for (auto& e : out) {
        if (e.first != q && e.second < fade_permille) {
            gained += e.second;
            e.second = 0;
        }
    }
    bool has_q = false;
    for (auto& e : out) {
        if (e.first == q) {
            e.second += gained;
            has_q = true;
        }
    }
    if (!has_q && gained > 0) out.emplace_back(q, gained);
    Shares kept;
    for (const auto& e : out) {
        if (e.second > 0) kept.push_back(e);
    }
    std::sort(kept.begin(), kept.end());
    return kept;
}

// 6.3.5: the target standing of an entry not of the ruling race.
inline std::int64_t target_standing(bool conquered, std::int64_t years_since, std::int64_t subject_years,
                                    std::int64_t band) {
    if (conquered && years_since < subject_years) return SUBJECT;
    return band == HATRED ? SUBJECT : band == WARINESS ? TOLERATED : ACCEPTED;
}

// 6.4.3 item 1: n split over the shares: n_g = n x share div 1000, the left-over one each by
// (n x share mod 1000 descending, race ascending); groups below migrate_min are lost (not returned).
inline Shares split_groups(std::int64_t n, const Shares& s, std::int64_t migrate_min) {
    Shares out;
    std::vector<std::pair<std::int64_t, std::int64_t>> rem;
    std::int64_t sum = 0;
    for (const auto& [g, x] : s) {
        out.emplace_back(g, n * x / 1000);
        sum += n * x / 1000;
        rem.emplace_back(n * x % 1000, g);
    }
    std::sort(rem.begin(), rem.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    for (std::int64_t i = 0; i < n - sum && !rem.empty(); ++i) {
        const std::int64_t g = rem[static_cast<std::size_t>(i) % rem.size()].second;
        for (auto& e : out) {
            if (e.first == g) ++e.second;
        }
    }
    Shares kept;
    for (const auto& e : out) {
        if (e.second >= migrate_min) kept.push_back(e);
    }
    return kept;
}

// 6.4.3 item 2: a host's room max(0, 2 x capacity - P).
inline std::int64_t room(std::int64_t capacity, std::int64_t population) {
    return std::max<std::int64_t>(0, 2 * capacity - population);
}

// --- Part B ------------------------------------------------------------------------------------------------

// 9.2: the toll of a raid.
inline std::int64_t toll(std::int64_t p, std::int64_t tau, std::int64_t kill_max, std::int64_t over_permille) {
    const std::int64_t raw = p * tau / 1000;
    std::int64_t dead = raw <= kill_max ? raw : kill_max + (raw - kill_max) * over_permille / 1000;
    dead = std::max<std::int64_t>(0, std::min(dead, p - 1));
    return dead;
}

// 9.3: the chance of a hunt after r raids within the memory (r >= 1).
inline std::int64_t hunt_chance(std::int64_t r, std::int64_t hunt_permille, std::int64_t raid_permille,
                                std::int64_t max_permille) {
    return std::min(max_permille, hunt_permille + (r - 1) * raid_permille);
}

// 14.1: the overrun chance, min(1000, overrun x M div 1000) x f_P div 1000.
inline std::int64_t overrun_factor(std::int64_t ref, std::int64_t p_after) {
    return std::min<std::int64_t>(1000, ref * 1000 / std::max<std::int64_t>(1, p_after));
}
inline std::int64_t overrun_chance(std::int64_t overrun_permille, std::int64_t m, std::int64_t ref,
                                   std::int64_t p_after) {
    return std::min<std::int64_t>(1000, overrun_permille * m / 1000) * overrun_factor(ref, p_after) / 1000;
}

// 14.2: the undead threshold undead_min_dead x 1000 div max(1, M).
inline std::int64_t undead_threshold(std::int64_t min_dead, std::int64_t m) {
    return min_dead * 1000 / std::max<std::int64_t>(1, m);
}

// 11.1: the dead of the road, P x (min + u) div 1000 with u = pick(..., max - min + 1), then min(road, P - 1).
inline std::int64_t road_loss(std::int64_t p, std::int64_t min_permille, std::int64_t u) {
    std::int64_t road = p * (min_permille + u) / 1000;
    if (p >= 1) road = std::min(road, p - 1);
    return road;
}

// 12.1: the epithet of a name form: the form without `{root}`, outer spaces trimmed.
inline std::string epithet(std::string_view form) {
    std::string s(form);
    const std::size_t at = s.find("{root}");
    if (at != std::string::npos) s.erase(at, 6);
    const std::size_t a = s.find_first_not_of(' ');
    if (a == std::string::npos) return "";
    const std::size_t b = s.find_last_not_of(' ');
    return s.substr(a, b - a + 1);
}

// 9.5, 13: the phrase number ((event id - 1) mod n).
inline std::size_t phrase_index(std::int64_t event_id, std::size_t n) {
    return static_cast<std::size_t>((event_id - 1) % static_cast<std::int64_t>(n));
}

// 9.5 grammar `phrase`: lower-case ASCII letters, single spaces, ' and -, starting and ending with a letter,
// at most 48 bytes.
inline bool is_phrase(std::string_view s) {
    if (s.empty() || s.size() > 48U) return false;
    auto letter = [](char c) { return c >= 'a' && c <= 'z'; };
    if (!letter(s.front()) || !letter(s.back())) return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (letter(c) || c == '\'' || c == '-') continue;
        if (c == ' ' && i > 0 && s[i - 1] != ' ') continue;
        return false;
    }
    return true;
}

}  // namespace rel_f
