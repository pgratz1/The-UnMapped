#pragma once

// The test's own implementation of the fourth version's new steps of P1-01:
// the pass layout (E6.5), the composition with the pass factor and the
// floor adjustment (E7), the final talus as plain Jacobi steps (E8.7), the
// rows of table `pass` (9.1) and the walkability measure of P1-00 10.3.
//
// Written from docs/specs/p1-01-elevation.md and p1-00-geography-overview.md
// only, as a second reading beside worldgen/tests/reference/
// p1_elevation_reference.py (which produced the golden values). Nothing here
// calls the stage. Flood levels come from the support's minimax search
// (not G3), components from union-find (not G2), the ocean distance from a
// Dijkstra (not the chamfer). Every priority queue exists in two
// structures (a binary heap and an ordered set) so that tests can show the
// results do not depend on the structure (P1-01 10.3).

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "p1_01_elevation_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"

namespace elev_test {

enum class Queue { heap, ordered_set };

// --- Small helpers -----------------------------------------------------------------------

[[nodiscard]] inline std::int64_t xof(Grid g, std::int64_t c) { return c % g.width; }
[[nodiscard]] inline std::int64_t yof(Grid g, std::int64_t c) { return c / g.width; }
[[nodiscard]] inline std::int64_t d2of(Grid g, std::int64_t a, std::int64_t b) {
    const std::int64_t dx = xof(g, a) - xof(g, b);
    const std::int64_t dy = yof(g, a) - yof(g, b);
    return dx * dx + dy * dy;
}
// Floor division (rounds toward minus infinity), E6.5.6 (e) 1.
[[nodiscard]] inline std::int64_t fdiv(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}
// fifths to metres, E6.5.0.
[[nodiscard]] inline Fx fm(std::int64_t f5) { return Fx::from_ratio(16 * f5, 5); }

// m'(c, p) of E6.5.0, in the order written.
[[nodiscard]] inline std::int64_t lowered(std::int64_t scale, std::int64_t mk, std::int64_t pr, std::int64_t p,
                                          std::int64_t f, std::int64_t keep) {
    using worldgen::max;
    using worldgen::mul;
    const Fx inner = mul(mul(R(p), R(mk)), R(f)) + mul(mul(max(R(p), R(keep)), R(mk)), max(Fx(), R(pr) - R(f)));
    return mul(R(scale), inner).raw();
}

// B(c, r) of E6.5.0 over the whole grid, in index order.
inline std::vector<std::int64_t> box(Grid g, std::int64_t c, std::int64_t r) {
    std::vector<std::int64_t> out;
    const std::int64_t x0 = xof(g, c);
    const std::int64_t y0 = yof(g, c);
    for (std::int64_t y = std::max<std::int64_t>(0, y0 - r); y <= std::min<std::int64_t>(g.height - 1, y0 + r); ++y) {
        for (std::int64_t x = std::max<std::int64_t>(0, x0 - r); x <= std::min<std::int64_t>(g.width - 1, x0 + r);
             ++x) {
            if ((x - x0) * (x - x0) + (y - y0) * (y - y0) <= r * r) {
                out.push_back(y * g.width + x);
            }
        }
    }
    return out;
}

// The cells of a sorted list that lie in B(c, r), in index order.
inline std::vector<std::int64_t> box_of(Grid g, const std::vector<std::int64_t>& sorted_cells, std::int64_t c,
                                        std::int64_t r) {
    std::vector<std::int64_t> out;
    const std::int64_t lo = std::max<std::int64_t>(0, yof(g, c) - r) * g.width;
    const std::int64_t hi = (std::min<std::int64_t>(g.height - 1, yof(g, c) + r) + 1) * g.width;
    auto it = std::lower_bound(sorted_cells.begin(), sorted_cells.end(), lo);
    for (; it != sorted_cells.end() && *it < hi; ++it) {
        const std::int64_t dx = xof(g, *it) - xof(g, c);
        const std::int64_t dy = yof(g, *it) - yof(g, c);
        if (dx >= -r && dx <= r && dx * dx + dy * dy <= r * r) {
            out.push_back(*it);
        }
    }
    return out;
}

// --- Priority queues of unique keys, in two structures ----------------------------------

template <typename Key>
class MinQueue {
public:
    explicit MinQueue(Queue kind) : kind_(kind) {}
    void push(const Key& k) {
        if (kind_ == Queue::heap) {
            heap_.push(k);
        } else {
            set_.insert(k);
        }
    }
    [[nodiscard]] bool empty() const { return kind_ == Queue::heap ? heap_.empty() : set_.empty(); }
    Key pop() {
        if (kind_ == Queue::heap) {
            Key k = heap_.top();
            heap_.pop();
            return k;
        }
        Key k = *set_.begin();
        set_.erase(set_.begin());
        return k;
    }

private:
    Queue kind_;
    std::priority_queue<Key, std::vector<Key>, std::greater<>> heap_;
    std::set<Key> set_;
};

// --- Dijkstra of E6.5.0 --------------------------------------------------------------------

struct Search {
    std::vector<std::int64_t> dist;  // -1 where not reached
    std::vector<std::int64_t> src;   // source value; -1 where not reached
};

inline Search dijkstra(Grid g, const std::function<bool(std::int64_t)>& in_domain,
                       const std::vector<std::pair<std::int64_t, std::int64_t>>& sources, std::int64_t limit,
                       Queue kind) {
    const std::size_t n = cells(g);
    Search s{std::vector<std::int64_t>(n, -1), std::vector<std::int64_t>(n, -1)};
    MinQueue<std::pair<std::int64_t, std::int64_t>> q(kind);
    for (const auto& [c, v] : sources) {
        const auto ci = static_cast<std::size_t>(c);
        if (s.dist[ci] < 0) {
            s.dist[ci] = 0;
            s.src[ci] = v;
            q.push({0, c});
        }
    }
    while (!q.empty()) {
        const auto [d, c] = q.pop();
        if (s.dist[static_cast<std::size_t>(c)] != d) {
            continue;
        }
        const std::int64_t sc = s.src[static_cast<std::size_t>(c)];
        each_neighbour8(g, static_cast<std::size_t>(c), [&](std::uint8_t code, std::size_t m) {
            if (!in_domain(static_cast<std::int64_t>(m))) {
                return;
            }
            const std::int64_t nd = d + ((code & 1U) != 0U ? 7 : 5);
            if (nd > limit) {
                return;
            }
            if (s.dist[m] < 0 || nd < s.dist[m]) {
                s.dist[m] = nd;
                s.src[m] = sc;
                q.push({nd, static_cast<std::int64_t>(m)});
            }
        });
    }
    return s;
}

// --- E6.5.3 step 2: the divide flood ---------------------------------------------------

struct OwnFlood {
    std::vector<std::int64_t> level;   // -1 where unset
    std::vector<std::int64_t> parent;  // -1 where unset
    std::vector<std::uint8_t> side;    // 0 where unset
};

inline OwnFlood own_divide_flood(Grid g, std::span<const std::uint8_t> support, std::span<const std::int64_t> key,
                                 std::span<const std::int64_t> nb, std::int64_t offset, Queue kind) {
    const std::size_t n = cells(g);
    OwnFlood f{std::vector<std::int64_t>(n, -1), std::vector<std::int64_t>(n, -1), std::vector<std::uint8_t>(n, 0)};
    MinQueue<std::tuple<std::int64_t, std::int64_t, std::int64_t>> q(kind);
    std::int64_t seq = 0;
    for (std::size_t c = 0; c < n; ++c) {
        if (support[c] != 0U) {
            continue;
        }
        bool next_to = false;
        each_neighbour8(g, c, [&](std::uint8_t, std::size_t m) { next_to = next_to || support[m] != 0U; });
        if (next_to) {
            f.level[c] = 0;
            f.parent[c] = static_cast<std::int64_t>(c);
            f.side[c] = nb[c] >= offset ? 2 : 1;
            q.push({0, seq++, static_cast<std::int64_t>(c)});
        }
    }
    while (!q.empty()) {
        const auto [k, s, c] = q.pop();
        (void)s;
        each_neighbour8(g, static_cast<std::size_t>(c), [&](std::uint8_t, std::size_t m) {
            if (support[m] == 0U || f.level[m] >= 0) {
                return;
            }
            f.level[m] = std::max(key[m], k);
            f.parent[m] = c;
            f.side[m] = f.side[static_cast<std::size_t>(c)];
            q.push({f.level[m], seq++, static_cast<std::int64_t>(m)});
        });
    }
    return f;
}

// --- The layout --------------------------------------------------------------------------

struct OwnRange {
    std::int32_t label = 0;
    std::int64_t s0 = 0, end_a = 0, end_b = 0, top = 0, length = 0, p90 = 0;
    std::int32_t slots = 0;
    std::vector<std::int64_t> cells, divide, candidates;
};

struct OwnSite {
    std::int32_t range = 0, slot = 0, index = 0;
    std::int64_t col = 0, partner = 0;
    bool gap = false, capped = false;
    std::vector<std::int64_t> axis, arc;
    std::int32_t ci = 0;
    std::int64_t dh = 0, maxm = 0, saddle = 0, halfwidth = 0, depth = 0;
};

struct OwnTry {
    std::int64_t col = 0;
    std::int32_t range = 0, slot = 0;
    std::string rule;  // a reject name, or "site"
};

struct OwnLayout {
    std::vector<OwnRange> ranges;
    std::vector<OwnSite> sites;
    std::vector<OwnTry> tries;
    std::vector<std::int64_t> factor, adj;
    // Internals, for the properties of 10.2.
    std::vector<std::int64_t> mh, ter, key;
    std::vector<std::uint8_t> support, body, basin, crest;
    OwnFlood flood;
    std::vector<std::int64_t> partner, dh;  // -1 where not a divide cell
    Search range_search;
    std::int64_t world_top = -1;
    bool ran_divide = false;
};

struct PassGrids {
    Grid grid;
    std::span<const std::int64_t> h0, unit, mask, profile, nb;
    std::span<const std::uint8_t> ocean;
    std::int64_t scale = 0, offset = 0, w_b = 0;
};

namespace detail {

inline std::vector<std::int64_t> chain(const std::vector<std::int64_t>& parent, std::int64_t c) {
    std::vector<std::int64_t> out{c};
    while (parent[static_cast<std::size_t>(c)] != c) {
        c = parent[static_cast<std::size_t>(c)];
        out.push_back(c);
    }
    return out;
}

}  // namespace detail

// E6.5.6 (o) on axis values.
inline std::pair<std::int64_t, bool> own_pass_depth(std::span<const std::int64_t> mks,
                                                    std::span<const std::int64_t> prs, std::int64_t scale,
                                                    std::int64_t saddle, bool capped, std::int64_t maxm,
                                                    const Values& v) {
    const std::int64_t f = v.raw("foothill_fraction");
    const std::int64_t keep = v.raw("pass_texture_keep");
    const std::int64_t walk = v.raw("pass_walk_grade") * 48;
    const std::size_t nq = mks.size();
    auto mv = [&](std::int64_t p) {
        std::vector<std::int64_t> out(nq);
        for (std::size_t q = 0; q < nq; ++q) {
            out[q] = lowered(scale, mks[q], prs[q], p, f, keep);
        }
        return out;
    };
    auto fits = [&](std::int64_t p) {
        const std::vector<std::int64_t> m = mv(p);
        for (const std::int64_t x : m) {
            if (x > saddle) {
                return false;
            }
        }
        if (!capped) {
            for (std::size_t q = 3; q < nq; ++q) {
                const std::int64_t d = m[q] - m[q - 3];
                if ((d < 0 ? -d : d) > walk) {
                    return false;
                }
            }
        }
        return true;
    };
    auto top = [&](std::int64_t p) {
        const std::vector<std::int64_t> m = mv(p);
        return m.empty() ? std::int64_t{0} : *std::max_element(m.begin(), m.end());
    };
    std::int64_t lo = 0;
    if (fits(ONE)) {
        lo = ONE;
    } else {
        std::int64_t hi = ONE;
        for (int s = 0; s < 32; ++s) {
            const std::int64_t mid = (lo + hi) >> 1;
            if (fits(mid)) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
    }
    const std::int64_t floor_m = (maxm << 16) - v.raw("pass_max_cut_m");
    if (top(lo) < floor_m) {
        std::int64_t plo = lo;
        std::int64_t phi = ONE;
        for (int s = 0; s < 32; ++s) {
            const std::int64_t mid = (plo + phi) >> 1;
            if (top(mid) >= floor_m) {
                phi = mid;
            } else {
                plo = mid;
            }
        }
        lo = phi;
        capped = true;
    }
    return {lo, capped};
}

// E6.5.7 bump; 0 if dist >= hwv.
inline std::int64_t own_bump(std::int64_t dist, std::int64_t dend, std::int64_t halfwidth, std::int64_t width_var,
                             std::int64_t nz) {
    using worldgen::clamp;
    using worldgen::div;
    using worldgen::fade;
    using worldgen::mul;
    const Fx hwv = mul(R(halfwidth), Fx::ONE + mul(R(width_var), R(nz)));
    if (R(dist) >= hwv) {
        return 0;
    }
    return mul(fade(Fx::ONE - div(R(dist), hwv)), fade(clamp(div(R(dend), Fx::from_int(300)), Fx(), Fx::ONE))).raw();
}

// E6.5.8: Delta per axis cell.
inline std::vector<std::int64_t> own_drainage(std::span<const std::int64_t> S, std::span<const std::int64_t> N,
                                              std::int64_t ci, std::int64_t eps) {
    const auto last = static_cast<std::int64_t>(S.size()) - 1;
    std::vector<std::int64_t> U(S.begin(), S.end());
    std::vector<std::int64_t> L(N.begin(), N.end());
    auto at = [](std::vector<std::int64_t>& v, std::int64_t q) -> std::int64_t& { return v[static_cast<std::size_t>(q)]; };
    auto cat = [](std::span<const std::int64_t> v, std::int64_t q) { return v[static_cast<std::size_t>(q)]; };
    for (std::int64_t q = ci - 1; q >= 0; --q) {
        at(U, q) = std::min(cat(S, q), at(U, q + 1) - eps);
    }
    for (std::int64_t q = ci + 1; q <= last; ++q) {
        at(U, q) = std::min(cat(S, q), at(U, q - 1) - eps);
    }
    for (std::int64_t q = 1; q <= ci; ++q) {
        at(L, q) = at(L, q - 1) + eps;
    }
    for (std::int64_t q = last - 1; q > ci; --q) {
        at(L, q) = at(L, q + 1) + eps;
    }
    std::vector<std::int64_t> out(S.size());
    for (std::int64_t q = 0; q <= last; ++q) {
        out[static_cast<std::size_t>(q)] =
            std::max(at(U, q), std::min(cat(N, q), at(L, q))) - cat(S, q);
    }
    return out;
}

// E6.5 on any W x H grid.
inline OwnLayout own_pass_layout(std::uint64_t seed, const PassGrids& in, const Values& v, Queue kind = Queue::heap) {
    using worldgen::clamp;
    using worldgen::mul;
    using worldgen::mul_div;
    const Grid g = in.grid;
    const std::size_t n = cells(g);
    const Values::PassInts pi = v.pass_ints(g.width);
    const std::uint64_t K_pass = worldgen::stream_key(seed, "geo.elevation.pass");
    OwnLayout L;
    L.factor.assign(n, ONE);
    L.adj.assign(n, 0);
    L.mh.assign(n, 0);
    L.ter.assign(n, 0);
    L.key.assign(n, 0);
    L.partner.assign(n, -1);
    L.dh.assign(n, -1);
    for (std::size_t i = 0; i < n; ++i) {
        L.mh[i] = in.ocean[i] != 0U ? 0 : (mul(R(in.scale), R(in.unit[i])).raw() >> 16);
        L.ter[i] = (in.h0[i] >> 16) + L.mh[i];
        L.key[i] = std::max<std::int64_t>(0, L.ter[i]);
    }
    // E6.5.2.
    const std::int64_t crest_lim = mul(v.fx("pass_crest_frac"), R(in.w_b)).raw();
    L.crest.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const std::int64_t d = in.nb[i] - in.offset;
        L.crest[i] = (in.ocean[i] == 0U && in.mask[i] >= v.raw("pass_core_mask") && (d < 0 ? -d : d) <= crest_lim) ? 1
                                                                                                                  : 0;
    }
    const std::vector<std::uint32_t> label = uf_components(g, L.crest, true, [](std::size_t, std::size_t) { return true; });
    std::uint32_t nranges = 0;
    for (const std::uint32_t l : label) {
        nranges = std::max(nranges, l);
    }
    L.ranges.resize(nranges);
    for (std::size_t i = 0; i < n; ++i) {
        if (label[i] != 0U) {
            L.ranges[label[i] - 1].cells.push_back(static_cast<std::int64_t>(i));
        }
    }
    std::vector<std::int64_t> dA(n, -1);
    for (std::uint32_t k = 1; k <= nranges; ++k) {
        OwnRange& r = L.ranges[k - 1];
        r.label = static_cast<std::int32_t>(k);
        r.s0 = r.cells.front();
        auto inr = [&label, k](std::int64_t m) { return label[static_cast<std::size_t>(m)] == k; };
        const Search gS = dijkstra(g, inr, {{r.s0, r.s0}}, INT64_MAX / 4, kind);
        auto argmax = [&r](const std::vector<std::int64_t>& val) {
            std::int64_t best = r.cells.front();
            for (const std::int64_t c : r.cells) {
                if (val[static_cast<std::size_t>(c)] > val[static_cast<std::size_t>(best)]) {
                    best = c;
                }
            }
            return best;
        };
        r.end_a = argmax(gS.dist);
        const Search sA = dijkstra(g, inr, {{r.end_a, r.end_a}}, INT64_MAX / 4, kind);
        r.end_b = argmax(sA.dist);
        r.length = sA.dist[static_cast<std::size_t>(r.end_b)];
        r.slots = r.length < pi.min_len
                      ? 0
                      : static_cast<std::int32_t>(std::min<std::int64_t>(v.i("pass_count_max"),
                                                                         1 + (r.length - pi.min_len) / pi.spacing));
        r.top = argmax(L.mh);
        std::vector<std::int64_t> mhs;
        for (const std::int64_t c : r.cells) {
            mhs.push_back(L.mh[static_cast<std::size_t>(c)]);
            dA[static_cast<std::size_t>(c)] = sA.dist[static_cast<std::size_t>(c)];
        }
        std::sort(mhs.begin(), mhs.end());
        const auto cnt = static_cast<std::uint64_t>(mhs.size());
        const std::uint64_t rank = std::min<std::uint64_t>(cnt - 1, (static_cast<std::uint64_t>(3865470566) * cnt) >> 32);
        r.p90 = mhs[rank];
    }
    bool any_slot = false;
    for (const OwnRange& r : L.ranges) {
        any_slot = any_slot || r.slots > 0;
    }
    if (!any_slot) {
        return L;
    }
    L.ran_divide = true;
    // E6.5.1.
    for (std::size_t i = 0; i < n; ++i) {
        if (in.ocean[i] == 0U && (L.world_top < 0 || L.mh[i] > L.mh[static_cast<std::size_t>(L.world_top)])) {
            L.world_top = static_cast<std::int64_t>(i);
        }
    }
    std::vector<std::int64_t> kb(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (in.ocean[i] == 0U) {
            kb[i] = std::max<std::int64_t>(1, (R(in.h0[i]) + mul(R(in.scale), R(in.unit[i]))).raw());
        }
    }
    const std::vector<std::int64_t> Lb = minimax_levels(g, kb, in.ocean);
    L.basin.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        L.basin[i] = (in.ocean[i] == 0U && Lb[i] - kb[i] >= v.raw("pass_basin_depth_m")) ? 1 : 0;
    }
    const std::vector<std::uint32_t> blabel = uf_components(g, L.basin, true, [](std::size_t, std::size_t) { return true; });
    std::vector<std::int64_t> bsize;
    for (const std::uint32_t l : blabel) {
        if (l != 0U) {
            if (bsize.size() < l) {
                bsize.resize(l, 0);
            }
            bsize[l - 1] += 1;
        }
    }
    std::vector<std::uint8_t> bigbasin(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        bigbasin[i] = (blabel[i] != 0U && bsize[blabel[i] - 1] >= v.i("pass_tarn_min_cells")) ? 1 : 0;
    }
    const std::vector<std::int64_t> docean = chamfer_dijkstra(g, in.ocean);
    // E6.5.3.
    L.support.assign(n, 0);
    L.body.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        L.support[i] = (in.ocean[i] == 0U && in.mask[i] >= v.raw("pass_region_mask")) ? 1 : 0;
        L.body[i] = (L.support[i] != 0U && L.mh[i] >= pi.body_u) ? 1 : 0;
    }
    L.flood = own_divide_flood(g, L.support, L.key, in.nb, in.offset, kind);
    const auto& side = L.flood.side;
    for (std::size_t c = 0; c < n; ++c) {
        if (L.body[c] == 0U || side[c] != 1U) {
            continue;
        }
        std::int64_t best = -1;
        for (const std::uint8_t code : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{6}}) {
            worldgen::geo::CellIndex m = 0;
            if (!worldgen::geo::neighbour(g, static_cast<worldgen::geo::CellIndex>(c), code, m)) {
                continue;
            }
            if (L.body[m] != 0U && side[m] == 2U && (best < 0 || L.key[m] < L.key[static_cast<std::size_t>(best)])) {
                best = static_cast<std::int64_t>(m);
            }
        }
        if (best >= 0) {
            L.partner[c] = best;
            L.dh[c] = std::max(L.key[c], L.key[static_cast<std::size_t>(best)]);
        }
    }
    std::vector<std::pair<std::int64_t, std::int64_t>> crest_src;
    for (std::size_t i = 0; i < n; ++i) {
        if (L.crest[i] != 0U) {
            crest_src.emplace_back(static_cast<std::int64_t>(i), static_cast<std::int64_t>(i));
        }
    }
    const auto& sup = L.support;
    L.range_search = dijkstra(g, [&sup](std::int64_t m) { return sup[static_cast<std::size_t>(m)] != 0U; },
                              crest_src, pi.reach5, kind);
    for (std::size_t c = 0; c < n; ++c) {
        if (L.partner[c] >= 0 && L.range_search.dist[c] >= 0) {
            const auto src = static_cast<std::size_t>(L.range_search.src[c]);
            L.ranges[label[src] - 1].divide.push_back(static_cast<std::int64_t>(c));
        }
    }
    // E6.5.4 to E6.5.6.
    std::vector<std::int64_t> world_cols;
    const std::int64_t sb2 = pi.spacing_b * pi.spacing_b;
    const Values::PassInts& P = pi;
    for (OwnRange& r : L.ranges) {
        if (r.slots == 0 || r.divide.empty()) {
            continue;
        }
        const std::vector<std::int64_t>& D = r.divide;
        struct Info {
            std::int64_t ax, ay;
            std::vector<std::int64_t> chA, chB;
        };
        std::vector<Info> infos;
        for (const std::int64_t c : D) {
            const std::int64_t dc = L.dh[static_cast<std::size_t>(c)];
            bool local_min = true;
            for (const std::int64_t q : box_of(g, D, c, P.win)) {
                const std::int64_t dq = L.dh[static_cast<std::size_t>(q)];
                if (q != c && (dq < dc || (dq == dc && q < c))) {
                    local_min = false;
                }
            }
            if (!local_min) {
                continue;
            }
            Info inf{0, 0, detail::chain(L.flood.parent, c),
                     detail::chain(L.flood.parent, L.partner[static_cast<std::size_t>(c)])};
            const std::int64_t pa = inf.chA[std::min<std::size_t>(20, inf.chA.size() - 1)];
            const std::int64_t pb = inf.chB[std::min<std::size_t>(20, inf.chB.size() - 1)];
            inf.ax = xof(g, pb) - xof(g, pa);
            inf.ay = yof(g, pb) - yof(g, pa);
            std::int64_t lmax = -1;
            std::int64_t rmax = -1;
            for (const std::int64_t q : box_of(g, D, c, P.rise_b)) {
                const std::int64_t cr = inf.ax * (yof(g, q) - yof(g, c)) - inf.ay * (xof(g, q) - xof(g, c));
                if (cr > 0) {
                    lmax = std::max(lmax, L.dh[static_cast<std::size_t>(q)]);
                } else if (cr < 0) {
                    rmax = std::max(rmax, L.dh[static_cast<std::size_t>(q)]);
                }
            }
            if (lmax >= dc + P.rise_u && rmax >= dc + P.rise_u) {
                r.candidates.push_back(c);
                infos.push_back(std::move(inf));
            }
        }
        std::vector<std::int64_t> cols_k;
        std::vector<bool> tried(r.candidates.size(), false);
        for (std::int32_t j = 0; j < r.slots; ++j) {
            bool placed = false;
            while (!placed) {
                std::int64_t best = -1;
                std::int64_t best_score = 0;
                for (std::size_t ci = 0; ci < r.candidates.size(); ++ci) {
                    const std::int64_t c = r.candidates[ci];
                    bool ok = !tried[ci];
                    for (const std::int64_t w : world_cols) {
                        ok = ok && d2of(g, c, w) >= sb2;
                    }
                    if (!ok) {
                        continue;
                    }
                    std::int64_t dmin = std::min(d2of(g, c, r.end_a), d2of(g, c, r.end_b));
                    for (const std::int64_t w : cols_k) {
                        dmin = std::min(dmin, d2of(g, c, w));
                    }
                    const auto hj = static_cast<std::int64_t>(worldgen::hash_at(K_pass, c, j) >> 54U);
                    const std::int64_t score = dmin * (1024 + (hj * 3) / 10);
                    if (best < 0) {
                        best = static_cast<std::int64_t>(ci);
                        best_score = score;
                        continue;
                    }
                    const std::int64_t bc = r.candidates[static_cast<std::size_t>(best)];
                    const std::int64_t dh_c = L.dh[static_cast<std::size_t>(c)];
                    const std::int64_t dh_b = L.dh[static_cast<std::size_t>(bc)];
                    if (dh_c < dh_b || (dh_c == dh_b && (score > best_score || (score == best_score && c < bc)))) {
                        best = static_cast<std::int64_t>(ci);
                        best_score = score;
                    }
                }
                if (best < 0) {
                    break;
                }
                tried[static_cast<std::size_t>(best)] = true;
                const std::int64_t c = r.candidates[static_cast<std::size_t>(best)];
                const Info& inf = infos[static_cast<std::size_t>(best)];
                // The crossing tests, E6.5.6.
                std::string rule;
                OwnSite site;
                const std::int64_t cx = xof(g, c);
                const std::int64_t cy = yof(g, c);
                [&]() {
                    if (docean[static_cast<std::size_t>(c)] < P.coast_clear) {
                        rule = "coast";
                        return;
                    }
                    if (mul(R(in.mask[static_cast<std::size_t>(c)]), R(in.scale)) < v.fx("pass_barrier_m")) {
                        rule = "barrier";
                        return;
                    }
                    std::vector<std::int64_t> axis(inf.chA.rbegin(), inf.chA.rend());
                    axis.insert(axis.end(), inf.chB.begin(), inf.chB.end());
                    const auto ci = static_cast<std::int64_t>(inf.chA.size()) - 1;
                    if (in.ocean[static_cast<std::size_t>(inf.chA.back())] != 0U ||
                        in.ocean[static_cast<std::size_t>(inf.chB.back())] != 0U) {
                        rule = "sea";
                        return;
                    }
                    const auto last = static_cast<std::int64_t>(axis.size()) - 1;
                    std::vector<std::int64_t> arc(axis.size(), 0);
                    for (std::size_t q = 1; q < axis.size(); ++q) {
                        const bool diag = xof(g, axis[q - 1]) != xof(g, axis[q]) && yof(g, axis[q - 1]) != yof(g, axis[q]);
                        arc[q] = arc[q - 1] + (diag ? 7 : 5);
                    }
                    auto A = [&](std::int64_t q) { return axis[static_cast<std::size_t>(q)]; };
                    auto ARC = [&](std::int64_t q) { return arc[static_cast<std::size_t>(q)]; };
                    const Fx reach_a = fm(ARC(ci));
                    const Fx reach_b = fm(ARC(last) - ARC(ci));
                    std::int64_t qa = ci - 1;
                    while (L.body[static_cast<std::size_t>(A(qa))] != 0U) {
                        --qa;
                    }
                    std::int64_t qb = ci + 1;
                    while (L.body[static_cast<std::size_t>(A(qb))] != 0U) {
                        ++qb;
                    }
                    const std::int64_t ea = A(qa);
                    const std::int64_t eb = A(qb);
                    const std::int64_t b5a = ARC(ci) - ARC(qa);
                    const std::int64_t b5b = ARC(qb) - ARC(ci);
                    if (worldgen::max(fm(b5a), fm(b5b)) > R(P.side_max)) {
                        rule = "side";
                        return;
                    }
                    const std::int64_t ex[2][2] = {{xof(g, ea) - cx, yof(g, ea) - cy}, {xof(g, eb) - cx, yof(g, eb) - cy}};
                    auto along = [&](std::int64_t tx, std::int64_t ty) {
                        for (const auto& e : ex) {
                            const std::int64_t vt = e[0] * tx + e[1] * ty;
                            if (2 * vt * vt > (e[0] * e[0] + e[1] * e[1]) * (tx * tx + ty * ty)) {
                                return true;
                            }
                        }
                        return false;
                    };
                    std::int64_t sxl = 0, syl = 0, nl = 0, sxr = 0, syr = 0, nr = 0;
                    for (const std::int64_t q : box_of(g, D, c, 37)) {
                        const std::int64_t dx = xof(g, q) - cx;
                        const std::int64_t dy = yof(g, q) - cy;
                        const std::int64_t cr = inf.ax * dy - inf.ay * dx;
                        if (cr > 0) {
                            sxl += dx;
                            syl += dy;
                            ++nl;
                        } else if (cr < 0) {
                            sxr += dx;
                            syr += dy;
                            ++nr;
                        }
                    }
                    if (nl > 0 && nr > 0 && along(fdiv(sxr, nr) - fdiv(sxl, nl), fdiv(syr, nr) - fdiv(syl, nl))) {
                        rule = "along_divide";
                        return;
                    }
                    std::int64_t cmin = -1;
                    std::int64_t cmax = -1;
                    for (const std::int64_t q : box_of(g, r.cells, c, P.rise_b)) {
                        const std::int64_t dq = dA[static_cast<std::size_t>(q)];
                        if (cmin < 0 || dq < dA[static_cast<std::size_t>(cmin)]) {
                            cmin = q;
                        }
                        if (cmax < 0 || dq > dA[static_cast<std::size_t>(cmax)]) {
                            cmax = q;
                        }
                    }
                    const bool has_t = cmin >= 0 && cmax >= 0 && cmin != cmax;
                    const std::int64_t tx = has_t ? xof(g, cmax) - xof(g, cmin) : 0;
                    const std::int64_t ty = has_t ? yof(g, cmax) - yof(g, cmin) : 0;
                    if (has_t && along(tx, ty)) {
                        rule = "along_crest";
                        return;
                    }
                    const std::int64_t chx = xof(g, eb) - xof(g, ea);
                    const std::int64_t chy = yof(g, eb) - yof(g, ea);
                    const std::int64_t chord2 = chx * chx + chy * chy;
                    const std::int64_t arc5 = b5a + b5b;
                    if (4 * arc5 * arc5 > 225 * chord2) {
                        rule = "chord";
                        return;
                    }
                    if (has_t) {
                        const std::int64_t ct = chx * tx + chy * ty;
                        if (100 * ct * ct > 33 * chord2 * (tx * tx + ty * ty)) {
                            rule = "chord";
                            return;
                        }
                    }
                    for (const std::int64_t q : axis) {
                        if (L.body[static_cast<std::size_t>(q)] != 0U && bigbasin[static_cast<std::size_t>(q)] != 0U) {
                            rule = "tarn_crossing";
                            return;
                        }
                    }
                    std::int64_t maxm = 0;
                    for (const std::int64_t q : axis) {
                        maxm = std::max(maxm, L.mh[static_cast<std::size_t>(q)]);
                    }
                    const Fx M = R(maxm << 16);
                    const Fx vj = R(static_cast<std::int64_t>(worldgen::hash_at(K_pass, c, 16) >> 32U) * 2) - Fx::ONE;
                    const Fx wj = R(static_cast<std::int64_t>(worldgen::hash_at(K_pass, c, 17) >> 32U) * 2) - Fx::ONE;
                    const Fx grade = worldgen::max(Fx(), mul(v.fx("pass_grade"), Fx::ONE + mul(v.fx("pass_grade_jitter"), vj)));
                    Fx saddle = clamp(mul(mul(grade, worldgen::min(reach_a, reach_b)), Fx::from_ratio(8, 15)),
                                      v.fx("pass_min_col_m"), v.fx("pass_saddle_max_m"));
                    bool capped = false;
                    if (M - v.fx("pass_max_cut_m") > saddle) {
                        saddle = M - v.fx("pass_max_cut_m");
                        capped = true;
                    }
                    const Fx halfwidth = mul(v.fx("pass_halfwidth_m"), Fx::ONE + mul(v.fx("pass_width_jitter"), wj));
                    site.range = r.label;
                    site.col = c;
                    site.partner = L.partner[static_cast<std::size_t>(c)];
                    site.axis = axis;
                    site.arc = arc;
                    site.ci = static_cast<std::int32_t>(ci);
                    site.dh = L.dh[static_cast<std::size_t>(c)];
                    site.maxm = maxm;
                    site.saddle = saddle.raw();
                    site.halfwidth = halfwidth.raw();
                    if (M <= saddle) {
                        site.gap = true;
                        rule = "site";
                        return;
                    }
                    for (const std::int64_t q : box(g, c, P.tarn_b)) {
                        if (L.basin[static_cast<std::size_t>(q)] != 0U) {
                            rule = "col_tarn";
                            return;
                        }
                    }
                    const std::int64_t hr = (halfwidth.floor_int() / 16) / 2;
                    for (const std::int64_t q : axis) {
                        if (d2of(g, q, r.top) <= hr * hr || d2of(g, q, L.world_top) <= hr * hr) {
                            rule = "summit_top";
                            return;
                        }
                    }
                    std::int64_t zmax = -1;
                    for (std::size_t a = 0; a < axis.size(); a += 4) {
                        for (const std::int64_t q : box(g, axis[a], hr)) {
                            if (in.ocean[static_cast<std::size_t>(q)] == 0U) {
                                zmax = std::max(zmax, L.mh[static_cast<std::size_t>(q)]);
                            }
                        }
                    }
                    if (zmax > r.p90) {
                        rule = "zone_p90";
                        return;
                    }
                    const Fx feet = worldgen::max(R(in.h0[static_cast<std::size_t>(axis.front())]),
                                                  R(in.h0[static_cast<std::size_t>(axis.back())]));
                    if ((R(in.h0[static_cast<std::size_t>(c)]) + saddle) - feet < v.fx("pass_min_col_m")) {
                        rule = "feet";
                        return;
                    }
                    const std::int64_t need =
                        std::max(v.raw("pass_barrier_m"), (saddle + v.fx("pass_peak_rise_m")).raw()) >> 16;
                    std::int64_t hl = -1;
                    std::int64_t hrr = -1;
                    for (const std::int64_t q : box_of(g, D, c, 187)) {
                        if (d2of(g, q, c) < 81) {
                            continue;
                        }
                        const std::int64_t cr = inf.ax * (yof(g, q) - cy) - inf.ay * (xof(g, q) - cx);
                        const std::int64_t tv = std::max(L.ter[static_cast<std::size_t>(q)],
                                                         L.ter[static_cast<std::size_t>(L.partner[static_cast<std::size_t>(q)])]);
                        if (cr > 0) {
                            hl = std::max(hl, tv);
                        } else if (cr < 0) {
                            hrr = std::max(hrr, tv);
                        }
                    }
                    if (hl < need || hrr < need) {
                        rule = "shoulders";
                        return;
                    }
                    std::vector<std::int64_t> mks;
                    std::vector<std::int64_t> prs;
                    for (const std::int64_t q : axis) {
                        mks.push_back(in.mask[static_cast<std::size_t>(q)]);
                        prs.push_back(in.profile[static_cast<std::size_t>(q)]);
                    }
                    const auto [lo, cap2] = own_pass_depth(mks, prs, in.scale, saddle.raw(), capped, maxm, v);
                    site.capped = cap2;
                    site.depth = ONE - lo;
                    rule = "site";
                }();
                L.tries.push_back(OwnTry{c, r.label, j, rule});
                if (rule == "site") {
                    site.slot = j;
                    site.index = static_cast<std::int32_t>(L.sites.size());
                    L.sites.push_back(site);
                    cols_k.push_back(c);
                    world_cols.push_back(c);
                    placed = true;
                }
            }
            if (!placed) {
                break;
            }
        }
    }
    // E6.5.7.
    const std::uint64_t Knz = worldgen::substream(K_pass, 3);
    const std::int64_t f = v.raw("foothill_fraction");
    const std::int64_t keep = v.raw("pass_texture_keep");
    const std::int64_t width_var = v.raw("pass_width_var");
    struct Rec {
        std::int64_t c, a, bump;
    };
    std::vector<std::vector<Rec>> records(L.sites.size());
    const auto& oc = in.ocean;
    for (const OwnSite& s : L.sites) {
        if (s.gap) {
            continue;
        }
        const auto last = static_cast<std::int64_t>(s.axis.size()) - 1;
        const Fx hwmax = mul(R(s.halfwidth), Fx::ONE + R(width_var));
        const std::int64_t lim5 = mul_div(hwmax.raw(), 5, 16 * ONE) + 2;
        std::vector<std::pair<std::int64_t, std::int64_t>> src;
        for (std::size_t a = 0; a < s.axis.size(); ++a) {
            src.emplace_back(s.axis[a], static_cast<std::int64_t>(a));
        }
        const Search sr = dijkstra(g, [&oc](std::int64_t m) { return oc[static_cast<std::size_t>(m)] == 0U; }, src,
                                   lim5, kind);
        for (std::size_t c = 0; c < n; ++c) {
            if (sr.dist[c] < 0) {
                continue;
            }
            const std::int64_t a = sr.src[c];
            const std::int64_t arc_a = s.arc[static_cast<std::size_t>(a)];
            const std::int64_t arc_ci = s.arc[static_cast<std::size_t>(s.ci)];
            const std::int64_t arc_last = s.arc[static_cast<std::size_t>(last)];
            const Fx s_m = fm(arc_a - arc_ci);
            const Fx dend = fm(std::min(arc_a, arc_last - arc_a));
            const Fx dist = fm(sr.dist[c]);
            const Fx nz = worldgen::noise2(Knz, mul(s_m, Fx::from_ratio(1, 1500)), Fx::from_int(3 * s.index + 2));
            const Fx hwv = mul(R(s.halfwidth), Fx::ONE + mul(R(width_var), nz));
            if (dist >= hwv) {
                continue;
            }
            const std::int64_t bump = own_bump(dist.raw(), dend.raw(), s.halfwidth, width_var, nz.raw());
            L.factor[c] = std::min(L.factor[c], ONE - mul(R(s.depth), R(bump)).raw());
            records[static_cast<std::size_t>(s.index)].push_back(Rec{static_cast<std::int64_t>(c), a, bump});
        }
    }
    // E6.5.8.
    std::vector<std::int64_t> best(n, -1);
    const std::int64_t eps = v.raw("pass_drain_step_m");
    for (const OwnSite& s : L.sites) {
        if (s.gap) {
            continue;
        }
        std::vector<std::int64_t> S;
        std::vector<std::int64_t> N;
        for (const std::int64_t c : s.axis) {
            const auto ci = static_cast<std::size_t>(c);
            S.push_back((R(in.h0[ci]) + R(lowered(in.scale, in.mask[ci], in.profile[ci], L.factor[ci], f, keep))).raw());
            N.push_back((R(in.h0[ci]) + R(lowered(in.scale, in.mask[ci], in.profile[ci], ONE, f, keep))).raw());
        }
        const std::vector<std::int64_t> delta = own_drainage(S, N, s.ci, eps);
        for (const Rec& rc : records[static_cast<std::size_t>(s.index)]) {
            const auto ci = static_cast<std::size_t>(rc.c);
            if (rc.bump > best[ci]) {
                best[ci] = rc.bump;
                L.adj[ci] = -mul(R(delta[static_cast<std::size_t>(rc.a)]), R(rc.bump)).raw();
            }
        }
    }
    return L;
}

// E7 land cells (ocean from `ocean_h`).
inline std::vector<std::int64_t> own_compose(const PassGrids& in, std::span<const std::int64_t> factor,
                                             std::span<const std::int64_t> adj, std::span<const std::int64_t> ocean_h,
                                             const Values& v) {
    using worldgen::mul;
    const std::size_t n = cells(in.grid);
    std::vector<std::int64_t> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (in.ocean[i] != 0U) {
            out[i] = ocean_h[i];
            continue;
        }
        const Fx nat = mul(R(in.scale), R(in.unit[i]));
        Fx m = nat;
        if (factor[i] < ONE) {
            m = R(lowered(in.scale, in.mask[i], in.profile[i], factor[i], v.raw("foothill_fraction"),
                          v.raw("pass_texture_keep")));
        }
        if (adj[i] != 0) {
            m = worldgen::min(m - R(adj[i]), nat);
        }
        out[i] = worldgen::clamp(R(in.h0[i]) + m, v.fx("land_floor_m"), R(H_MAX_RAW)).raw();
    }
    return out;
}

// E8.7 as full Jacobi steps over every land cell (no active set).
inline std::vector<std::int64_t> own_final_talus(Grid g, std::span<const std::int64_t> h,
                                                 std::span<const std::uint8_t> ocean, std::int64_t drop_o,
                                                 std::int64_t drop_d, std::int64_t rate, std::int64_t floor,
                                                 std::int32_t iterations) {
    using worldgen::mul;
    std::vector<std::int64_t> cur(h.begin(), h.end());
    std::vector<std::int64_t> nxt(cur);
    for (std::int32_t it = 0; it < iterations; ++it) {
        for (std::size_t c = 0; c < cur.size(); ++c) {
            if (ocean[c] != 0U) {
                nxt[c] = cur[c];
                continue;
            }
            Fx delta;
            each_neighbour8(g, c, [&](std::uint8_t code, std::size_t m) {
                const Fx hm = ocean[m] != 0U ? Fx() : R(cur[m]);
                const Fx dk = R((code & 1U) != 0U ? drop_d : drop_o);
                const Fx x = R(cur[c]) - hm;
                if (x > dk) {
                    delta = delta - mul(R(rate), x - dk);
                } else if (ocean[m] == 0U && (Fx() - x) > dk) {
                    delta = delta + mul(R(rate), (Fx() - x) - dk);
                }
            });
            nxt[c] = worldgen::max(R(floor), R(cur[c]) + delta).raw();
        }
        cur.swap(nxt);
    }
    return cur;
}

// --- E5.2 per land cell (the stage does not expose h0) -------------------------------------

struct CellHeights {
    std::int64_t h0 = 0, unit = 0, mask = 0, profile = 0;
};

// E5 coast factor, mask and E5.2 for one land cell, every statement as written.
inline CellHeights e52_cell(std::uint64_t seed, std::int32_t x, std::int32_t y, std::int64_t c, std::int64_t nb,
                            std::int64_t ns, std::int64_t c_sea, std::int64_t s_land, std::int64_t w_b,
                            std::int64_t o_star, std::int64_t beta, const Values& v) {
    using worldgen::abs;
    using worldgen::clamp;
    using worldgen::div;
    using worldgen::fade;
    using worldgen::mul;
    using worldgen::geo::fbm_m;
    using worldgen::geo::ridged_m;
    const Fx X = Fx::from_int(16 * x + 8);
    const Fx Y = Fx::from_int(16 * y + 8);
    const Fx s = R(c) - R(c_sea);
    const Fx a_s = abs(s);
    const Fx tl = a_s >= worldgen::mul_int(R(s_land), 1024) ? Fx::from_int(1024) : div(a_s, R(s_land));
    const Fx gm = fade(clamp(div(tl, v.fx("mountain_coast_t")), Fx(), Fx::ONE));
    const Fx co = div(tl, Fx::ONE + tl);
    const Fx un = fbm_m(key(seed, "basin"), X, Y, v.m_rec("basin_noise"));
    const Fx base = mul(v.fx("lowland_height_m"), co) + mul(mul(v.fx("basin_amp_m"), un), co);
    const Fx a = abs(R(nb) - R(o_star));
    const Fx b_o = a >= R(w_b) ? Fx() : fade(Fx::ONE - div(a, R(w_b)));
    const Fx sg = fade(clamp((Fx::HALF + R(beta)) + mul(v.fx("segment_contrast"), R(ns)), Fx(), Fx::ONE));
    const Fx mask = mul(mul(b_o, sg), gm);
    const Fx qx = X + mul(v.fx("relief_warp_amp_m"), fbm_m(key(seed, "relief_warp_x"), X, Y, v.m_rec("relief_warp")));
    const Fx qy = Y + mul(v.fx("relief_warp_amp_m"), fbm_m(key(seed, "relief_warp_y"), X, Y, v.m_rec("relief_warp")));
    CellHeights out;
    out.mask = mask.raw();
    if (mask > Fx()) {
        const Fx ridge = ridged_m(key(seed, "ridge"), qx, qy, v.m_rec("ridge_noise"), v.fx("ridge_weight"));
        const Fx profile = v.fx("foothill_fraction") + mul(Fx::ONE - v.fx("foothill_fraction"), ridge);
        out.profile = profile.raw();
        out.unit = mul(mask, profile).raw();
    }
    const Fx hh = fbm_m(key(seed, "hills"), qx, qy, v.m_rec("hill_noise"));
    const Fx rn = fbm_m(key(seed, "roughness"), X, Y, v.m_rec("roughness_noise"));
    const Fx rho = v.fx("roughness_min") +
                   mul(Fx::ONE - v.fx("roughness_min"), fade(clamp(Fx::HALF + rn, Fx(), Fx::ONE)));
    const Fx hf = v.fx("hill_coast_min") + mul(Fx::ONE - v.fx("hill_coast_min"), co);
    const Fx hills = mul(v.fx("hill_height_m"), mul(mul(hh, rho), hf));
    const Fx det = mul(v.fx("detail_amp_m"), fbm_m(key(seed, "detail"), X, Y, v.m_rec("detail_noise")));
    out.h0 = ((base + hills) + det).raw();
    return out;
}

// --- Table `pass` (9.1) and the walkability measure (P1-00 10.3) ----------------------------

struct PassRow {
    std::int64_t id = 0, range_id = 0, kind = 0, capped = 0, col_x = 0, col_y = 0, saddle_dm = 0, col_dm = 0;
    std::vector<std::uint8_t> path;  // u32 little-endian cell indices
    friend bool operator==(const PassRow&, const PassRow&) = default;
};

inline PassRow pass_row(std::int64_t id, std::int64_t range, bool gap, bool capped, std::int64_t col,
                        std::int64_t dh, std::int64_t h7_at_col, std::span<const std::int64_t> axis, std::int32_t n) {
    PassRow r;
    r.id = id;
    r.range_id = range;
    r.kind = gap ? 0 : 1;
    r.capped = (capped && !gap) ? 1 : 0;
    r.col_x = col % n;
    r.col_y = col / n;
    r.saddle_dm = worldgen::mul_div(dh, 10, std::int64_t{1} << 16);
    r.col_dm = worldgen::mul_div(h7_at_col, 10, std::int64_t{1} << 32);
    for (const std::int64_t q : axis) {
        for (int k = 0; k < 4; ++k) {
            r.path.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(q) >> (8 * k)) & 0xFFU));
        }
    }
    return r;
}

// P1-00 10.3 cell slope S2 on e' = max(e, 0).
[[nodiscard]] inline std::int64_t cell_s2(Grid g, std::span<const std::int16_t> e, std::int64_t x, std::int64_t y) {
    auto ep = [&](std::int64_t xx, std::int64_t yy) {
        return std::max<std::int64_t>(0, e[static_cast<std::size_t>(yy * g.width + xx)]);
    };
    const std::int64_t xa = std::max<std::int64_t>(x - 1, 0);
    const std::int64_t xb = std::min<std::int64_t>(x + 1, g.width - 1);
    const std::int64_t ya = std::max<std::int64_t>(y - 1, 0);
    const std::int64_t yb = std::min<std::int64_t>(y + 1, g.height - 1);
    const std::int64_t gx = xb > xa ? (ep(xb, y) - ep(xa, y)) * (2 / (xb - xa)) : 0;
    const std::int64_t gy = yb > ya ? (ep(x, yb) - ep(x, ya)) * (2 / (yb - ya)) : 0;
    return gx * gx + gy * gy;
}

// "Walkable end to end" of one row's path (P1-00 10.3).
inline bool walkable_end_to_end(Grid g, std::span<const std::int16_t> e, const std::vector<std::int64_t>& path) {
    const std::size_t n = cells(g);
    std::vector<std::uint8_t> in_r(n, 0);
    for (const std::int64_t q : path) {
        for (const std::int64_t c : box(g, q, 19)) {
            in_r[static_cast<std::size_t>(c)] = e[static_cast<std::size_t>(c)] > 0 ? 1 : 0;
        }
    }
    const std::int64_t a = path.front();
    const std::int64_t b = path.back();
    for (const std::int64_t q : {a, b}) {
        for (const std::int64_t c : box(g, q, 37)) {
            in_r[static_cast<std::size_t>(c)] = e[static_cast<std::size_t>(c)] > 0 ? 1 : 0;
        }
    }
    std::vector<std::uint8_t> walk(n, 0);
    for (std::size_t c = 0; c < n; ++c) {
        walk[c] = (in_r[c] != 0U && cell_s2(g, e, xof(g, static_cast<std::int64_t>(c)),
                                            yof(g, static_cast<std::int64_t>(c))) <= 50204)
                      ? 1
                      : 0;
    }
    std::vector<std::uint8_t> seen(n, 0);
    std::vector<std::size_t> stack;
    for (std::size_t c = 0; c < n; ++c) {
        if (walk[c] != 0U && d2of(g, static_cast<std::int64_t>(c), a) <= 1369) {
            seen[c] = 1;
            stack.push_back(c);
        }
    }
    while (!stack.empty()) {
        const std::size_t c = stack.back();
        stack.pop_back();
        if (d2of(g, static_cast<std::int64_t>(c), b) <= 1369) {
            return true;
        }
        each_neighbour8(g, c, [&](std::uint8_t, std::size_t m) {
            if (walk[m] != 0U && seen[m] == 0U) {
                seen[m] = 1;
                stack.push_back(m);
            }
        });
    }
    return false;
}

// metres1 of P1-00 10.3.
inline std::string metres1(std::int64_t v) {
    const std::int64_t a = v < 0 ? -v : v;
    return std::string(v < 0 ? "-" : "") + std::to_string(a / 10) + "." + std::to_string(a % 10);
}

// The five pass lines of P1-00 10.3 for rows on an N x N layer e.
inline std::vector<std::string> report_pass_lines(Grid g, std::span<const std::int16_t> e,
                                                  const std::vector<PassRow>& rows) {
    std::int64_t lowered_n = 0, gaps = 0, capped = 0, walk = 0;
    std::vector<std::int64_t> cols;
    for (const PassRow& r : rows) {
        gaps += r.kind == 0 ? 1 : 0;
        capped += r.capped == 1 ? 1 : 0;
        if (r.kind != 1) {
            continue;
        }
        ++lowered_n;
        cols.push_back(e[static_cast<std::size_t>(r.col_y * g.width + r.col_x)]);
        std::vector<std::int64_t> path;
        for (std::size_t k = 0; k + 3 < r.path.size(); k += 4) {
            path.push_back(static_cast<std::int64_t>(r.path[k]) | (static_cast<std::int64_t>(r.path[k + 1]) << 8) |
                           (static_cast<std::int64_t>(r.path[k + 2]) << 16) |
                           (static_cast<std::int64_t>(r.path[k + 3]) << 24));
        }
        walk += walkable_end_to_end(g, e, path) ? 1 : 0;
    }
    std::sort(cols.begin(), cols.end());
    std::string pc = "n/a n/a n/a";
    if (!cols.empty()) {
        const auto cn = static_cast<std::int64_t>(cols.size());
        pc.clear();
        for (const std::int64_t p : {0, 50, 100}) {
            pc += (pc.empty() ? "" : " ") + metres1(cols[static_cast<std::size_t>(std::min(cn - 1, p * cn / 100))]);
        }
    }
    return {"passes = " + std::to_string(lowered_n), "natural_gaps = " + std::to_string(gaps),
            "passes_capped = " + std::to_string(capped), "pass_col_m = " + pc,
            "passes_walkable = " + std::to_string(walk)};
}

}  // namespace elev_test
