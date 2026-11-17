// Stage P1-01 elevation, step E6.5: the pass layout (fourth version,
// docs/specs/p1-01-elevation.md E6.5.0 to E6.5.8). Single-threaded; every
// queue pops by a unique key, every box query visits cells in index order
// and every argmin or argmax breaks ties to the smallest index (section 7).

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/flow.hpp"
#include "worldgen/geo/select.hpp"
#include "worldgen/stages/elevation.hpp"

namespace worldgen::stages {

namespace {

using geo::CellIndex;
using geo::Direction;
using geo::Grid;

constexpr std::int64_t TWO_POW_32 = std::int64_t{1} << 32U;
constexpr CellIndex NO_CELL = PASS_NO_CELL;
constexpr std::int64_t CELL = geo::CELL_M;  // 16 m

// Constants of the code (P1-01 4.2, "Constants of the code").
constexpr std::size_t CROSSING_LINE_POS = 20;   // E6.5.4 2: the 20th chain cell
constexpr std::int64_t DIVIDE_DIR_CELLS = 37;   // E6.5.6 (e) 1: 600 m
constexpr std::int64_t SHOULDER_OUTER = 187;    // E6.5.6 (n): 3000 div 16
constexpr std::int64_t SHOULDER_INNER2 = 81;    // E6.5.6 (n): (150 div 16)^2
constexpr std::int64_t END_TAPER_M = 300;       // E6.5.7
constexpr std::int64_t WIDTH_WAVELENGTH_M = 1500;  // E6.5.7 width noise
constexpr std::int64_t JITTER_GRADE = 16;       // E6.5.6 (h): hash_at second coordinates
constexpr std::int64_t JITTER_WIDTH = 17;
constexpr std::uint64_t WIDTH_SUBSTREAM = 3;    // E6.5.7: substream(K_pass, 3)
constexpr std::uint32_t DEPTH_STEPS = 32;       // E6.5.6 (o)

void check_len(Grid grid, std::size_t size, const char* what) {
    geo::check_grid_array(grid, size, what);
}

// fm(f): fifths of a cell to metres (E6.5.0).
Fx fifths_m(std::int64_t f) {
    return Fx::from_ratio(f * CELL, 5);
}

// floor_int(m) div 16: whole cells of a non-negative length in metres (4.5).
std::int64_t whole_cells(Fx m) {
    return m.floor_int() / CELL;
}

std::int64_t step5(Grid grid, CellIndex a, CellIndex b) {
    return (grid.x_of(a) != grid.x_of(b) && grid.y_of(a) != grid.y_of(b)) ? 7 : 5;
}

std::int64_t d2(Grid grid, CellIndex a, CellIndex b) {
    const std::int64_t dx = std::int64_t{grid.x_of(a)} - grid.x_of(b);
    const std::int64_t dy = std::int64_t{grid.y_of(a)} - grid.y_of(b);
    return dx * dx + dy * dy;
}

// Floor division (rounds toward -inf), b > 0.
std::int64_t fdiv(std::int64_t a, std::int64_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// B(c, r) of E6.5.0: the disc of radius r clipped to the grid, in index
// order; fn(q, dx, dy) with (dx, dy) the offset of q from c.
template <typename Fn>
void box(Grid grid, CellIndex c, std::int64_t r, Fn&& fn) {
    const std::int64_t cx = grid.x_of(c);
    const std::int64_t cy = grid.y_of(c);
    const std::int64_t y0 = std::max<std::int64_t>(0, cy - r);
    const std::int64_t y1 = std::min<std::int64_t>(grid.height - 1, cy + r);
    const std::int64_t x0 = std::max<std::int64_t>(0, cx - r);
    const std::int64_t x1 = std::min<std::int64_t>(grid.width - 1, cx + r);
    const std::int64_t r2 = r * r;
    for (std::int64_t y = y0; y <= y1; ++y) {
        const std::int64_t dy = y - cy;
        for (std::int64_t x = x0; x <= x1; ++x) {
            const std::int64_t dx = x - cx;
            if (dx * dx + dy * dy <= r2) {
                fn(grid.index(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)), dx, dy);
            }
        }
    }
}

// A 5/7 Dijkstra (E6.5.0, D6). dist (-1 = unset) and src are whole-grid
// scratch owned by the caller; every cell given a distance is appended to
// touched (so the caller can reset it). Sources are pushed in the order
// given; a source listed twice keeps its first value.
struct Search {
    std::vector<std::int32_t> dist;
    std::vector<std::uint32_t> src;
    std::vector<CellIndex> touched;

    explicit Search(std::size_t cells) : dist(cells, -1), src(cells, 0) {}

    void reset() {
        for (const CellIndex c : touched) {
            dist[c] = -1;
        }
        touched.clear();
    }
};

template <typename Domain>
void dijkstra(Grid grid, std::span<const std::pair<CellIndex, std::uint32_t>> sources, Domain&& in_domain,
              std::int64_t limit, Search& s) {
    using Entry = std::pair<std::int64_t, CellIndex>;
    // determinism-lint: allow(heap-order) -- entries (distance, cell) are unique pairs: a pair is pushed only when its distance is strictly smaller than the cell's current one, so std::greater is a strict total order (P1-01 E6.5.0, D6)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    for (const auto& [c, v] : sources) {
        if (s.dist[c] == 0) {
            continue;
        }
        if (s.dist[c] < 0) {
            s.touched.push_back(c);
        }
        s.dist[c] = 0;
        s.src[c] = v;
        queue.emplace(0, c);
    }
    while (!queue.empty()) {
        const auto [d, c] = queue.top();
        queue.pop();
        if (d != s.dist[c]) {
            continue;  // stale
        }
        for (const Direction code : geo::NEIGHBOURS_8) {
            CellIndex m = 0;
            if (!geo::neighbour(grid, c, code, m) || !in_domain(m)) {
                continue;
            }
            const std::int64_t nd = d + (geo::is_diagonal(code) ? 7 : 5);
            if (nd > limit || (s.dist[m] >= 0 && nd >= s.dist[m])) {
                continue;
            }
            if (s.dist[m] < 0) {
                s.touched.push_back(m);
            }
            s.dist[m] = static_cast<std::int32_t>(nd);
            s.src[m] = s.src[c];
            queue.emplace(nd, m);
        }
    }
}

// E6.5.3 step 2 on predicates: support(c) and key(c) (units).
template <typename Support, typename Key>
DivideFlood divide_flood_impl(Grid grid, Support&& support, Key&& key, std::span<const std::int64_t> nb, Fx offset) {
    const std::uint32_t cells = grid.cells();
    DivideFlood out;
    out.parent.assign(cells, NO_CELL);
    out.side.assign(cells, 0);
    std::vector<std::int64_t> level(cells, -1);
    using Entry = std::tuple<std::int64_t, std::uint64_t, CellIndex>;
    // determinism-lint: allow(heap-order) -- entries (level, seq, cell) with seq the global push counter, unique per entry, so std::greater is a strict total order (P1-01 E6.5.3, D6)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    std::uint64_t seq = 0;
    const std::int64_t o = offset.raw();
    for (CellIndex c = 0; c < cells; ++c) {
        if (support(c)) {
            continue;
        }
        bool touches = false;
        for (const Direction code : geo::NEIGHBOURS_8) {
            CellIndex m = 0;
            if (geo::neighbour(grid, c, code, m) && support(m)) {
                touches = true;
                break;
            }
        }
        if (!touches) {
            continue;
        }
        level[c] = 0;
        out.parent[c] = c;
        out.side[c] = static_cast<std::uint8_t>(nb[c] >= o ? 2U : 1U);
        queue.emplace(0, seq++, c);
    }
    while (!queue.empty()) {
        const auto [k, sq, c] = queue.top();
        queue.pop();
        static_cast<void>(sq);
        for (const Direction code : geo::NEIGHBOURS_8) {
            CellIndex m = 0;
            if (!geo::neighbour(grid, c, code, m) || !support(m) || level[m] >= 0) {
                continue;
            }
            level[m] = std::max(key(m), k);
            out.parent[m] = c;
            out.side[m] = out.side[c];
            queue.emplace(level[m], seq++, m);
        }
    }
    return out;
}

// One divide cell of E6.5.3 step 3, with its range (step 4).
struct DivideCell {
    CellIndex cell = 0;
    CellIndex partner = 0;
    std::int64_t dh = 0;      // units
    std::int32_t range = 0;   // label k, 0 = none
};

// One candidate of E6.5.4.
struct Candidate {
    CellIndex cell = 0;
    std::int64_t ax = 0;  // across vector a(c), cells
    std::int64_t ay = 0;
    bool tried = false;
};

// One cell reached by a site's factor search (E6.5.7).
struct Reach {
    CellIndex cell = 0;
    std::uint32_t pos = 0;  // axis position a
    Fx bump;
};

// The working state of one pass_layout call.
class Layout {
public:
    Layout(std::uint64_t seed, const PassInputs& in, const ElevationTuning& t)
        : in_(in), t_(t), grid_(in.grid), cells_(in.grid.cells()), key_(stream_key(seed, streams::geo_elevation_pass)) {}

    PassLayout run();

private:
    [[nodiscard]] std::int64_t mh(CellIndex c) const {
        return mul(in_.scale, Fx::from_raw(in_.unit[c])).raw() >> 16U;
    }
    [[nodiscard]] std::int64_t ter(CellIndex c) const {
        return (in_.h0[c] >> 16U) + mh(c);
    }
    [[nodiscard]] bool land(CellIndex c) const { return in_.ocean[c] == 0U; }
    [[nodiscard]] Fx lowered(CellIndex c, Fx p) const {
        return pass_lowered(in_.scale, Fx::from_raw(in_.mask[c]), Fx::from_raw(in_.profile[c]), p,
                            t_.foothill_fraction, t_.pass_texture_keep);
    }
    [[nodiscard]] bool in_range(CellIndex q, std::int32_t k) const {
        const std::uint32_t d = divide_index_[q];
        return d != NO_CELL && divide_[d].range == k;
    }
    [[nodiscard]] const DivideCell& divide_of(CellIndex q) const { return divide_[divide_index_[q]]; }

    void derived();
    void ranges(PassLayout& out);
    void divide(PassLayout& out);
    std::vector<Candidate> candidates(PassRange& range);
    void choose(PassRange& range, std::vector<Candidate>& cands, PassLayout& out);
    [[nodiscard]] std::vector<CellIndex> chain(CellIndex c) const;
    // E6.5.6 on candidate cand of range r for slot j: a site, or the rule that failed.
    std::optional<PassReject> crossing(const PassRange& range, const Candidate& cand, PassSite& site);
    void basins();
    [[nodiscard]] bool near_ocean(CellIndex c) const;
    void factor(PassLayout& out);

    const PassInputs& in_;
    const ElevationTuning& t_;
    Grid grid_;
    std::uint32_t cells_;
    std::uint64_t key_;

    // Derived integers (4.5).
    std::int64_t coast_clear_ = 0;
    std::int64_t min_len_ = 0;
    std::int64_t spacing_ = 0;
    Fx side_max_;
    std::int64_t reach5_ = 0;
    std::int64_t win_ = 0;
    std::int64_t rise_b_ = 0;
    std::int64_t spacing_b_ = 0;
    std::int64_t tarn_b_ = 0;
    std::int64_t rise_u_ = 0;
    std::int64_t body_u_ = 0;

    CellIndex world_top_ = 0;
    std::vector<std::uint32_t> label_;  // range label per cell, 0 = not crest
    std::vector<std::int32_t> da_;      // dA per crest cell (fifths), -1 elsewhere
    std::vector<std::uint8_t> region_;  // bit 0 support, bit 1 body
    std::vector<CellIndex> parent_;     // divide flood
    std::vector<std::uint8_t> side_;
    std::vector<DivideCell> divide_;
    std::vector<std::uint32_t> divide_index_;  // per cell: index into divide_, or NO_CELL
    std::vector<std::uint8_t> basin_;   // bit 0 basin, bit 1 bigbasin; computed on first use
    bool basins_done_ = false;
    std::vector<CellIndex> world_cols_;
    std::vector<std::vector<Reach>> reached_;  // per site
};

void Layout::derived() {
    const std::int64_t num = std::min<std::int64_t>(grid_.width, t_.pass_size_ref_n);
    const std::int64_t den = t_.pass_size_ref_n;
    coast_clear_ = mul_div(t_.pass_coast_clear_m.raw(), 5 * num, CELL * TWO_POW_32 * den);
    min_len_ = (t_.pass_min_length_m * 5 * num) / (CELL * den);
    spacing_ = (t_.pass_spacing_m * 5) / CELL;
    side_max_ = Fx::from_raw(mul_div(t_.pass_side_max_m.raw(), num, den));
    reach5_ = mul_div(t_.pass_divide_reach_m.raw(), 5, CELL * TWO_POW_32);
    win_ = whole_cells(t_.pass_saddle_window_m);
    rise_b_ = whole_cells(t_.pass_saddle_rise_reach_m);
    spacing_b_ = whole_cells(t_.pass_min_spacing_m);
    tarn_b_ = whole_cells(t_.pass_col_tarn_m);
    rise_u_ = t_.pass_saddle_rise_m.raw() >> 16U;
    body_u_ = t_.pass_body_m.raw() >> 16U;
}

// E6.5.1 step 1 and E6.5.2.
void Layout::ranges(PassLayout& out) {
    std::int64_t top_v = -1;
    for (CellIndex c = 0; c < cells_; ++c) {
        if (land(c) && mh(c) > top_v) {
            top_v = mh(c);
            world_top_ = c;
        }
    }
    std::vector<std::uint8_t> crest(cells_, 0);
    const std::int64_t o = in_.offset.raw();
    const std::int64_t crest_lim = mul(t_.pass_crest_frac, in_.w_b).raw();
    const std::int64_t core = t_.pass_core_mask.raw();
    for (CellIndex c = 0; c < cells_; ++c) {
        if (land(c) && in_.mask[c] >= core) {
            const std::int64_t a = in_.nb[c] >= o ? in_.nb[c] - o : o - in_.nb[c];
            crest[c] = static_cast<std::uint8_t>(a <= crest_lim ? 1U : 0U);
        }
    }
    geo::Components comp = geo::label_components(grid_, crest, geo::Connectivity::eight);
    crest = {};
    label_ = std::move(comp.label);
    comp.parent = {};
    da_.assign(cells_, -1);
    for (std::uint32_t k = 1; k <= comp.count; ++k) {
        const std::uint32_t b = comp.start[k - 1U];
        const std::uint32_t e = comp.start[k];
        const std::span<const CellIndex> members(comp.order.data() + b, e - b);
        PassRange r;
        r.label = static_cast<std::int32_t>(k);
        r.s0 = *std::min_element(members.begin(), members.end());
        // Geodesic distances inside the range, computed into da_ (its cells only).
        const auto geodesic = [&](CellIndex from) {
            for (const CellIndex c : members) {
                da_[c] = -1;
            }
            using Entry = std::pair<std::int64_t, CellIndex>;
            // determinism-lint: allow(heap-order) -- entries (distance, cell) are unique pairs (pushed only on a strictly smaller distance), so std::greater is a strict total order (P1-01 E6.5.2; the distances are values)
            std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
            da_[from] = 0;
            queue.emplace(0, from);
            while (!queue.empty()) {
                const auto [d, c] = queue.top();
                queue.pop();
                if (d != da_[c]) {
                    continue;
                }
                for (const Direction code : geo::NEIGHBOURS_8) {
                    CellIndex m = 0;
                    if (!geo::neighbour(grid_, c, code, m) || label_[m] != k) {
                        continue;
                    }
                    const std::int64_t nd = d + (geo::is_diagonal(code) ? 7 : 5);
                    if (da_[m] < 0 || nd < da_[m]) {
                        da_[m] = static_cast<std::int32_t>(nd);
                        queue.emplace(nd, m);
                    }
                }
            }
            CellIndex far = from;
            std::int32_t fd = -1;
            for (const CellIndex c : members) {
                if (da_[c] > fd || (da_[c] == fd && c < far)) {
                    fd = da_[c];
                    far = c;
                }
            }
            return far;
        };
        r.end_a = geodesic(r.s0);
        r.end_b = geodesic(r.end_a);
        r.length = da_[r.end_b];
        r.slots = r.length < min_len_ ? 0
                                      : static_cast<std::int32_t>(std::min<std::int64_t>(
                                            t_.pass_count_max, 1 + (r.length - min_len_) / spacing_));
        std::vector<std::int64_t> heights;
        heights.reserve(members.size());
        std::int64_t tv = -1;
        r.top = r.s0;
        for (const CellIndex c : members) {
            const std::int64_t v = mh(c);
            heights.push_back(v);
            if (v > tv || (v == tv && c < r.top)) {
                tv = v;
                r.top = c;
            }
        }
        r.p90 = geo::kth_in_place(heights, geo::quantile_rank(Fx::from_ratio(9, 10), heights.size()));
        out.ranges.push_back(std::move(r));
    }
}

// E6.5.3.
void Layout::divide(PassLayout& out) {
    region_.assign(cells_, 0);
    const std::int64_t rmask = t_.pass_region_mask.raw();
    for (CellIndex c = 0; c < cells_; ++c) {
        if (land(c) && in_.mask[c] >= rmask) {
            region_[c] = static_cast<std::uint8_t>(mh(c) >= body_u_ ? 3U : 1U);
        }
    }
    const auto support = [this](CellIndex c) { return (region_[c] & 1U) != 0U; };
    const auto body = [this](CellIndex c) { return (region_[c] & 2U) != 0U; };
    const auto key = [this](CellIndex c) { return std::max<std::int64_t>(0, ter(c)); };
    {
        DivideFlood flood = divide_flood_impl(grid_, support, key, in_.nb, in_.offset);
        parent_ = std::move(flood.parent);
        side_ = std::move(flood.side);
    }
    // Step 3: divide cells and partners.
    divide_index_.assign(cells_, NO_CELL);
    for (CellIndex c = 0; c < cells_; ++c) {
        if (!body(c) || side_[c] != 1U) {
            continue;
        }
        CellIndex best = NO_CELL;
        std::int64_t best_k = 0;
        for (const Direction code : geo::NEIGHBOURS_4) {
            CellIndex m = 0;
            if (geo::neighbour(grid_, c, code, m) && body(m) && side_[m] == 2U) {
                const std::int64_t km = key(m);
                if (best == NO_CELL || km < best_k) {
                    best = m;
                    best_k = km;
                }
            }
        }
        if (best == NO_CELL) {
            continue;
        }
        divide_index_[c] = static_cast<std::uint32_t>(divide_.size());
        divide_.push_back(DivideCell{c, best, std::max(key(c), best_k), 0});
    }
    // Step 4: the range of each divide cell (Dijkstra over the support from
    // every crest cell, limit reach5).
    {
        Search s(cells_);
        std::vector<std::pair<CellIndex, std::uint32_t>> sources;
        for (CellIndex c = 0; c < cells_; ++c) {
            if (label_[c] != 0U) {
                sources.emplace_back(c, c);
            }
        }
        dijkstra(grid_, sources, support, reach5_, s);
        for (DivideCell& d : divide_) {
            if (s.dist[d.cell] >= 0) {
                d.range = static_cast<std::int32_t>(label_[s.src[d.cell]]);
            }
        }
    }
    for (const DivideCell& d : divide_) {
        if (d.range > 0) {
            out.ranges[static_cast<std::size_t>(d.range - 1)].divide.push_back(d.cell);
        }
    }
}

std::vector<CellIndex> Layout::chain(CellIndex c) const {
    std::vector<CellIndex> v;
    CellIndex x = c;
    while (true) {
        v.push_back(x);
        const CellIndex p = parent_[x];
        if (p == x || p == NO_CELL) {
            break;
        }
        x = p;
    }
    return v;
}

// E6.5.4.
std::vector<Candidate> Layout::candidates(PassRange& range) {
    std::vector<Candidate> out;
    const std::int32_t k = range.label;
    for (const CellIndex c : range.divide) {
        const std::int64_t dhc = divide_of(c).dh;
        bool is_min = true;
        box(grid_, c, win_, [&](CellIndex q, std::int64_t, std::int64_t) {
            if (is_min && q != c && in_range(q, k)) {
                const std::int64_t dq = divide_of(q).dh;
                if (dq < dhc || (dq == dhc && q < c)) {
                    is_min = false;
                }
            }
        });
        if (!is_min) {
            continue;
        }
        const std::vector<CellIndex> ca = chain(c);
        const std::vector<CellIndex> cb = chain(divide_of(c).partner);
        const CellIndex pa = ca[std::min(CROSSING_LINE_POS, ca.size() - 1U)];
        const CellIndex pb = cb[std::min(CROSSING_LINE_POS, cb.size() - 1U)];
        const std::int64_t ax = std::int64_t{grid_.x_of(pb)} - grid_.x_of(pa);
        const std::int64_t ay = std::int64_t{grid_.y_of(pb)} - grid_.y_of(pa);
        std::int64_t lmax = -1;
        std::int64_t rmax = -1;
        box(grid_, c, rise_b_, [&](CellIndex q, std::int64_t dx, std::int64_t dy) {
            if (!in_range(q, k)) {
                return;
            }
            const std::int64_t cr = ax * dy - ay * dx;
            if (cr > 0) {
                lmax = std::max(lmax, divide_of(q).dh);
            } else if (cr < 0) {
                rmax = std::max(rmax, divide_of(q).dh);
            }
        });
        if (lmax >= dhc + rise_u_ && rmax >= dhc + rise_u_) {
            out.push_back(Candidate{c, ax, ay, false});
            range.candidates.push_back(c);
        }
    }
    return out;
}

// E6.5.1 steps 2 and 3, computed when a crossing test first needs them.
void Layout::basins() {
    if (basins_done_) {
        return;
    }
    basins_done_ = true;
    basin_.assign(cells_, 0);
    {
        // G3's flood levels Lb on the keys kb (only the levels are used, a
        // function of the keys: P1-01 E6.5.1 step 2 allows any method). A
        // priority flood from the land cells next to the ocean: Lb(c) = the
        // smallest, over 8-paths from c to the ocean, of the largest key on
        // the path, which is G3's level on land (ocean counts as 0 and every
        // land key is >= 1). Each cell is pushed once, so the pairs (level,
        // cell) are unique; the pop order among equal levels does not change
        // a level. Without a key grid, an order or a parent this needs one
        // int64 per cell plus the frontier (8.1).
        const auto kb = [this](CellIndex c) {
            return std::max<std::int64_t>(1,
                                          (Fx::from_raw(in_.h0[c]) + mul(in_.scale, Fx::from_raw(in_.unit[c]))).raw());
        };
        std::vector<std::int64_t> level(cells_, -1);
        using Entry = std::pair<std::int64_t, CellIndex>;
        // determinism-lint: allow(heap-order) -- entries (level, cell) are unique pairs (each cell is pushed once), so std::greater is a strict total order; only the levels are kept, and they do not depend on the pop order (P1-01 E6.5.1 step 2)
        std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
        for (CellIndex c = 0; c < cells_; ++c) {
            if (!land(c)) {
                continue;
            }
            for (const Direction code : geo::NEIGHBOURS_8) {
                CellIndex m = 0;
                if (geo::neighbour(grid_, c, code, m) && !land(m)) {
                    level[c] = kb(c);
                    queue.emplace(level[c], c);
                    break;
                }
            }
        }
        while (!queue.empty()) {
            const auto [lv, c] = queue.top();
            queue.pop();
            for (const Direction code : geo::NEIGHBOURS_8) {
                CellIndex m = 0;
                if (!geo::neighbour(grid_, c, code, m) || !land(m) || level[m] >= 0) {
                    continue;
                }
                level[m] = std::max(kb(m), lv);
                queue.emplace(level[m], m);
            }
        }
        const std::int64_t depth = t_.pass_basin_depth_m.raw();
        for (CellIndex c = 0; c < cells_; ++c) {
            if (land(c) && level[c] - kb(c) >= depth) {
                basin_[c] = 1;
            }
        }
    }
    const geo::Components comp = geo::label_components(grid_, basin_, geo::Connectivity::eight);
    for (std::uint32_t k = 1; k <= comp.count; ++k) {
        if (std::int64_t{comp.size(k)} < t_.pass_tarn_min_cells) {
            continue;
        }
        for (std::uint32_t q = comp.start[k - 1U]; q < comp.start[k]; ++q) {
            basin_[comp.order[q]] = 3;
        }
    }
}

// E6.5.6 (a): docean[c] < coast_clear, by the closed form of E6.5.1 step 4
// over the ocean cells that can be nearer than coast_clear.
bool Layout::near_ocean(CellIndex c) const {
    if (coast_clear_ <= 0) {
        return false;
    }
    const std::int64_t r = (coast_clear_ - 1) / 5;
    const std::int64_t cx = grid_.x_of(c);
    const std::int64_t cy = grid_.y_of(c);
    for (std::int64_t y = std::max<std::int64_t>(0, cy - r); y <= std::min<std::int64_t>(grid_.height - 1, cy + r);
         ++y) {
        for (std::int64_t x = std::max<std::int64_t>(0, cx - r); x <= std::min<std::int64_t>(grid_.width - 1, cx + r);
             ++x) {
            const CellIndex q = grid_.index(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
            if (in_.ocean[q] == 0U) {
                continue;
            }
            const std::int64_t ax = x > cx ? x - cx : cx - x;
            const std::int64_t ay = y > cy ? y - cy : cy - y;
            if (5 * std::max(ax, ay) + 2 * std::min(ax, ay) < coast_clear_) {
                return true;
            }
        }
    }
    return false;
}

std::optional<PassReject> Layout::crossing(const PassRange& range, const Candidate& cand, PassSite& site) {
    const CellIndex c = cand.cell;
    const std::int32_t k = range.label;
    const auto body = [this](CellIndex q) { return (region_[q] & 2U) != 0U; };
    // (a) coast, (b) barrier.
    if (near_ocean(c)) {
        return PassReject::coast;
    }
    if (mul(Fx::from_raw(in_.mask[c]), in_.scale) < t_.pass_barrier_m) {
        return PassReject::barrier;
    }
    // (c) crossing polyline and sea.
    const DivideCell& dc = divide_of(c);
    const std::vector<CellIndex> ca = chain(c);
    const std::vector<CellIndex> cb = chain(dc.partner);
    if (!land(ca.back()) || !land(cb.back())) {
        return PassReject::sea;
    }
    std::vector<CellIndex> axis(ca.rbegin(), ca.rend());
    axis.insert(axis.end(), cb.begin(), cb.end());
    const std::size_t ci = ca.size() - 1U;
    const std::size_t last = axis.size() - 1U;
    std::vector<std::int64_t> arc(axis.size(), 0);
    for (std::size_t q = 1; q < axis.size(); ++q) {
        arc[q] = arc[q - 1U] + step5(grid_, axis[q - 1U], axis[q]);
    }
    const Fx reach_a = fifths_m(arc[ci]);
    const Fx reach_b = fifths_m(arc[last] - arc[ci]);
    // (d) side length; body exits.
    std::size_t qa = 0;
    for (std::size_t q = ci; q-- > 0;) {
        if (!body(axis[q])) {
            qa = q;
            break;
        }
    }
    std::size_t qb = last;
    for (std::size_t q = ci + 1U; q <= last; ++q) {
        if (!body(axis[q])) {
            qb = q;
            break;
        }
    }
    const CellIndex exit_a = axis[qa];
    const CellIndex exit_b = axis[qb];
    const std::int64_t body5_a = arc[ci] - arc[qa];
    const std::int64_t body5_b = arc[qb] - arc[ci];
    if (max(fifths_m(body5_a), fifths_m(body5_b)) > side_max_) {
        return PassReject::side;
    }
    const std::int64_t cx = grid_.x_of(c);
    const std::int64_t cy = grid_.y_of(c);
    const std::array<CellIndex, 2> exits{exit_a, exit_b};
    const auto along = [&](std::int64_t tx, std::int64_t ty) {
        for (const CellIndex e : exits) {
            const std::int64_t vx = grid_.x_of(e) - cx;
            const std::int64_t vy = grid_.y_of(e) - cy;
            const std::int64_t dot = vx * tx + vy * ty;
            if (2 * dot * dot > (vx * vx + vy * vy) * (tx * tx + ty * ty)) {
                return true;
            }
        }
        return false;
    };
    // (e) 1 local divide direction.
    {
        std::array<std::int64_t, 6> acc{};  // sx, sy, n: left (cr > 0) then right (cr < 0)
        box(grid_, c, DIVIDE_DIR_CELLS, [&](CellIndex q, std::int64_t dx, std::int64_t dy) {
            if (!in_range(q, k)) {
                return;
            }
            const std::int64_t cr = cand.ax * dy - cand.ay * dx;
            if (cr != 0) {
                const std::size_t o3 = cr > 0 ? 0U : 3U;
                acc[o3] += dx;
                acc[o3 + 1U] += dy;
                acc[o3 + 2U] += 1;
            }
        });
        if (acc[2] > 0 && acc[5] > 0) {
            const std::int64_t tx = fdiv(acc[3], acc[5]) - fdiv(acc[0], acc[2]);
            const std::int64_t ty = fdiv(acc[4], acc[5]) - fdiv(acc[1], acc[2]);
            if (along(tx, ty)) {
                return PassReject::along_divide;
            }
        }
    }
    // (e) 2 crest axis.
    CellIndex cmin = NO_CELL;
    CellIndex cmax = NO_CELL;
    box(grid_, c, rise_b_, [&](CellIndex q, std::int64_t, std::int64_t) {
        if (label_[q] != static_cast<std::uint32_t>(k)) {
            return;
        }
        if (cmin == NO_CELL || da_[q] < da_[cmin]) {
            cmin = q;
        }
        if (cmax == NO_CELL || da_[q] > da_[cmax]) {
            cmax = q;
        }
    });
    const bool has_axis = cmin != NO_CELL && cmax != NO_CELL && cmin != cmax;
    const std::int64_t tpx = has_axis ? std::int64_t{grid_.x_of(cmax)} - grid_.x_of(cmin) : 0;
    const std::int64_t tpy = has_axis ? std::int64_t{grid_.y_of(cmax)} - grid_.y_of(cmin) : 0;
    if (has_axis && along(tpx, tpy)) {
        return PassReject::along_crest;
    }
    // (f) chord.
    {
        const std::int64_t chx = std::int64_t{grid_.x_of(exit_b)} - grid_.x_of(exit_a);
        const std::int64_t chy = std::int64_t{grid_.y_of(exit_b)} - grid_.y_of(exit_a);
        const std::int64_t chord2 = chx * chx + chy * chy;
        const std::int64_t arc5 = body5_a + body5_b;
        if (4 * arc5 * arc5 > 225 * chord2) {
            return PassReject::chord;
        }
        if (has_axis) {
            const std::int64_t dot = chx * tpx + chy * tpy;
            if (100 * dot * dot > 33 * chord2 * (tpx * tpx + tpy * tpy)) {
                return PassReject::chord;
            }
        }
    }
    // (g) tarn on the crossing.
    basins();
    for (const CellIndex q : axis) {
        if (body(q) && basin_[q] == 3U) {
            return PassReject::tarn_crossing;
        }
    }
    // (h) values.
    std::int64_t maxm = 0;
    for (const CellIndex q : axis) {
        maxm = std::max(maxm, mh(q));
    }
    const Fx big_m = Fx::from_raw(maxm << 16U);
    const auto jitter = [&](std::int64_t slot) {
        const auto v = static_cast<std::int64_t>(hash_at(key_, static_cast<std::int64_t>(c), slot) >> 32U);
        return Fx::from_raw(v * 2) - Fx::ONE;
    };
    const Fx vj = jitter(JITTER_GRADE);
    const Fx wj = jitter(JITTER_WIDTH);
    const Fx grade = max(Fx(), mul(t_.pass_grade, Fx::ONE + mul(t_.pass_grade_jitter, vj)));
    Fx saddle = clamp(mul(mul(grade, min(reach_a, reach_b)), Fx::from_ratio(8, 15)), t_.pass_min_col_m,
                      t_.pass_saddle_max_m);
    bool capped = false;
    if (big_m - t_.pass_max_cut_m > saddle) {
        saddle = big_m - t_.pass_max_cut_m;
        capped = true;
    }
    const Fx halfwidth = mul(t_.pass_halfwidth_m, Fx::ONE + mul(t_.pass_width_jitter, wj));
    site.range = k;
    site.col = c;
    site.partner = dc.partner;
    site.col_index = static_cast<std::int32_t>(ci);
    site.dh = dc.dh;
    site.maxm = maxm;
    site.saddle = saddle;
    site.halfwidth = halfwidth;
    // (i) natural gap.
    if (big_m <= saddle) {
        site.natural_gap = true;
        site.capped = false;
        site.depth = Fx();
        site.axis = std::move(axis);
        return std::nullopt;
    }
    // (j) tarn at the col.
    {
        bool tarn = false;
        box(grid_, c, tarn_b_, [&](CellIndex q, std::int64_t, std::int64_t) { tarn = tarn || basin_[q] != 0U; });
        if (tarn) {
            return PassReject::col_tarn;
        }
    }
    // (k) summits near the crossing.
    const std::int64_t hr = whole_cells(halfwidth) / 2;
    for (const CellIndex q : axis) {
        if (d2(grid_, q, range.top) <= hr * hr || d2(grid_, q, world_top_) <= hr * hr) {
            return PassReject::summit_top;
        }
    }
    // (l) zone below the crest p90.
    {
        std::int64_t zmax = 0;
        for (std::size_t a = 0; a < axis.size(); a += 4U) {
            box(grid_, axis[a], hr, [&](CellIndex q, std::int64_t, std::int64_t) {
                if (land(q)) {
                    zmax = std::max(zmax, mh(q));
                }
            });
        }
        if (zmax > range.p90) {
            return PassReject::zone_p90;
        }
    }
    // (m) above the feet.
    {
        const Fx foot = max(Fx::from_raw(in_.h0[axis.front()]), Fx::from_raw(in_.h0[axis.back()]));
        if ((Fx::from_raw(in_.h0[c]) + saddle) - foot < t_.pass_min_col_m) {
            return PassReject::feet;
        }
    }
    // (n) shoulders.
    {
        const std::int64_t need = std::max(t_.pass_barrier_m.raw(), (saddle + t_.pass_peak_rise_m).raw()) >> 16U;
        std::int64_t hl = -1;
        std::int64_t hrr = -1;
        box(grid_, c, SHOULDER_OUTER, [&](CellIndex q, std::int64_t dx, std::int64_t dy) {
            if (dx * dx + dy * dy < SHOULDER_INNER2 || !in_range(q, k)) {
                return;
            }
            const std::int64_t cr = cand.ax * dy - cand.ay * dx;
            const std::int64_t tv = std::max(ter(q), ter(divide_of(q).partner));
            if (cr > 0) {
                hl = std::max(hl, tv);
            } else if (cr < 0) {
                hrr = std::max(hrr, tv);
            }
        });
        if (hl < need || hrr < need) {
            return PassReject::shoulders;
        }
    }
    // (o) depth.
    std::vector<std::int64_t> amask(axis.size());
    std::vector<std::int64_t> aprof(axis.size());
    for (std::size_t q = 0; q < axis.size(); ++q) {
        amask[q] = in_.mask[axis[q]];
        aprof[q] = in_.profile[axis[q]];
    }
    const PassDepth pd = pass_depth(amask, aprof, in_.scale, saddle, capped, maxm, t_);
    site.natural_gap = false;
    site.capped = pd.capped;
    site.depth = Fx::ONE - pd.lo;
    site.axis = std::move(axis);
    return std::nullopt;
}

// E6.5.5 for one range.
void Layout::choose(PassRange& range, std::vector<Candidate>& cands, PassLayout& out) {
    std::vector<CellIndex> cols;  // cols_k
    const std::int64_t spacing2 = spacing_b_ * spacing_b_;
    for (std::int32_t j = 0; j < range.slots; ++j) {
        bool placed = false;
        while (!placed) {
            std::size_t best = cands.size();
            std::int64_t best_dh = 0;
            std::int64_t best_score = 0;
            for (std::size_t i = 0; i < cands.size(); ++i) {
                const Candidate& cd = cands[i];
                if (cd.tried) {
                    continue;
                }
                bool spaced = true;
                for (const CellIndex w : world_cols_) {
                    if (d2(grid_, cd.cell, w) < spacing2) {
                        spaced = false;
                        break;
                    }
                }
                if (!spaced) {
                    continue;
                }
                std::int64_t dmin = std::min(d2(grid_, cd.cell, range.end_a), d2(grid_, cd.cell, range.end_b));
                for (const CellIndex w : cols) {
                    dmin = std::min(dmin, d2(grid_, cd.cell, w));
                }
                const auto hj = static_cast<std::int64_t>(hash_at(key_, static_cast<std::int64_t>(cd.cell), j) >> 54U);
                const std::int64_t score = dmin * (1024 + (hj * 3) / 10);
                const std::int64_t dh = divide_of(cd.cell).dh;
                if (best == cands.size() || dh < best_dh ||
                    (dh == best_dh && (score > best_score || (score == best_score && cd.cell < cands[best].cell)))) {
                    best = i;
                    best_dh = dh;
                    best_score = score;
                }
            }
            if (best == cands.size()) {
                return;  // no further site on this range
            }
            Candidate& cd = cands[best];
            cd.tried = true;
            PassSite site;
            const std::optional<PassReject> reject = crossing(range, cd, site);
            out.tries.push_back(PassTry{cd.cell, range.label, j, reject});
            if (reject) {
                continue;
            }
            cols.push_back(cd.cell);
            world_cols_.push_back(cd.cell);
            out.sites.push_back(std::move(site));
            placed = true;
        }
    }
}

// E6.5.7 and E6.5.8.
void Layout::factor(PassLayout& out) {
    const std::uint64_t key_width = substream(key_, WIDTH_SUBSTREAM);
    const Fx width_freq = Fx::from_ratio(1, WIDTH_WAVELENGTH_M);
    const Fx taper_m = Fx::from_int(END_TAPER_M);
    Search s(cells_);
    reached_.assign(out.sites.size(), {});
    const auto land_domain = [this](CellIndex m) { return land(m); };
    for (std::size_t si = 0; si < out.sites.size(); ++si) {
        const PassSite& site = out.sites[si];
        if (site.natural_gap) {
            continue;
        }
        const std::size_t n_axis = site.axis.size();
        std::vector<std::int64_t> arc(n_axis, 0);
        for (std::size_t q = 1; q < n_axis; ++q) {
            arc[q] = arc[q - 1U] + step5(grid_, site.axis[q - 1U], site.axis[q]);
        }
        const std::int64_t arc_col = arc[static_cast<std::size_t>(site.col_index)];
        const std::int64_t arc_end = arc.back();
        const Fx hwmax = mul(site.halfwidth, Fx::ONE + t_.pass_width_var);
        const std::int64_t lim5 = mul_div(hwmax.raw(), 5, CELL * TWO_POW_32) + 2;
        std::vector<std::pair<CellIndex, std::uint32_t>> sources;
        sources.reserve(n_axis);
        for (std::size_t a = 0; a < n_axis; ++a) {
            sources.emplace_back(site.axis[a], static_cast<std::uint32_t>(a));
        }
        dijkstra(grid_, sources, land_domain, lim5, s);
        std::sort(s.touched.begin(), s.touched.end());  // determinism-lint: allow(unstable-sort) -- unique cell indices
        const Fx band = Fx::from_int(3 * static_cast<std::int64_t>(si) + 2);
        for (const CellIndex c : s.touched) {
            const std::size_t a = s.src[c];
            const Fx s_m = fifths_m(arc[a] - arc_col);
            const Fx dend = fifths_m(std::min(arc[a], arc_end - arc[a]));
            const Fx dist = fifths_m(s.dist[c]);
            const Fx nz = noise2(key_width, mul(s_m, width_freq), band);
            const Fx hwv = mul(site.halfwidth, Fx::ONE + mul(t_.pass_width_var, nz));
            if (dist >= hwv) {
                continue;
            }
            const Fx bump = mul(fade(Fx::ONE - div(dist, hwv)), fade(clamp(div(dend, taper_m), Fx(), Fx::ONE)));
            const Fx p = Fx::ONE - mul(site.depth, bump);
            if (p.raw() < out.factor[c]) {
                out.factor[c] = p.raw();
            }
            reached_[si].push_back(Reach{c, static_cast<std::uint32_t>(a), bump});
        }
        s.reset();
    }
    // E6.5.8: per site, Delta along the axis; then each cell takes the
    // adjustment of the site with the largest bump (ties: lower site index).
    struct Adjust {
        CellIndex cell;
        std::int64_t bump;
        std::int64_t adj;
    };
    std::vector<Adjust> all;
    for (std::size_t si = 0; si < out.sites.size(); ++si) {
        const PassSite& site = out.sites[si];
        if (site.natural_gap) {
            continue;
        }
        const std::size_t n_axis = site.axis.size();
        std::vector<std::int64_t> surf_low(n_axis);
        std::vector<std::int64_t> surf_nat(n_axis);
        for (std::size_t q = 0; q < n_axis; ++q) {
            const CellIndex c = site.axis[q];
            const Fx h0 = Fx::from_raw(in_.h0[c]);
            surf_low[q] = (h0 + lowered(c, Fx::from_raw(out.factor[c]))).raw();
            surf_nat[q] = (h0 + lowered(c, Fx::ONE)).raw();
        }
        const std::vector<std::int64_t> delta = pass_drainage(surf_low, surf_nat, site.col_index, t_.pass_drain_step_m);
        for (const Reach& r : reached_[si]) {
            all.push_back(Adjust{r.cell, r.bump.raw(), -mul(Fx::from_raw(delta[r.pos]), r.bump).raw()});
        }
    }
    // Stable: records of one cell stay in site order, so the first largest bump wins.
    std::stable_sort(all.begin(), all.end(), [](const Adjust& a, const Adjust& b) { return a.cell < b.cell; });
    for (std::size_t i = 0; i < all.size();) {
        std::size_t best = i;
        std::size_t j = i + 1U;
        for (; j < all.size() && all[j].cell == all[i].cell; ++j) {
            if (all[j].bump > all[best].bump) {
                best = j;
            }
        }
        out.adj[all[i].cell] = all[best].adj;
        i = j;
    }
}

PassLayout Layout::run() {
    PassLayout out;
    out.factor.assign(cells_, Fx::ONE.raw());
    out.adj.assign(cells_, 0);
    derived();
    ranges(out);
    bool any_slot = false;
    for (const PassRange& r : out.ranges) {
        any_slot = any_slot || r.slots > 0;
    }
    if (!any_slot) {
        return out;
    }
    divide(out);
    for (PassRange& r : out.ranges) {
        if (r.slots <= 0 || r.divide.empty()) {
            continue;
        }
        std::vector<Candidate> cands = candidates(r);
        choose(r, cands, out);
    }
    // The range and divide scratch is not needed by E6.5.7 and E6.5.8.
    label_ = {};
    da_ = {};
    region_ = {};
    parent_ = {};
    side_ = {};
    divide_index_ = {};
    divide_ = {};
    basin_ = {};
    factor(out);
    return out;
}

}  // namespace

DivideFlood divide_flood(Grid grid, std::span<const std::uint8_t> support, std::span<const std::int64_t> key,
                         std::span<const std::int64_t> nb, Fx offset) {
    check_len(grid, support.size(), "divide_flood: support");
    check_len(grid, key.size(), "divide_flood: key");
    check_len(grid, nb.size(), "divide_flood: nb");
    return divide_flood_impl(
        grid, [support](CellIndex c) { return support[c] != 0U; }, [key](CellIndex c) { return key[c]; }, nb, offset);
}

Fx pass_lowered(Fx scale, Fx mask, Fx profile, Fx p, Fx foothill_fraction, Fx texture_keep) {
    const Fx base = mul(mul(p, mask), foothill_fraction);
    const Fx relief = mul(mul(max(p, texture_keep), mask), max(Fx(), profile - foothill_fraction));
    return mul(scale, base + relief);
}

PassDepth pass_depth(std::span<const std::int64_t> mask, std::span<const std::int64_t> profile, Fx scale, Fx saddle,
                     bool capped, std::int64_t maxm, const ElevationTuning& t) {
    if (mask.size() != profile.size()) {
        fatal("pass_depth: mask and profile differ in length");
    }
    const std::size_t n = mask.size();
    std::vector<Fx> m(n);
    const auto eval = [&](Fx p) {
        for (std::size_t q = 0; q < n; ++q) {
            m[q] = pass_lowered(scale, Fx::from_raw(mask[q]), Fx::from_raw(profile[q]), p, t.foothill_fraction,
                                t.pass_texture_keep);
        }
    };
    const Fx walk = mul_int(t.pass_walk_grade, 48);
    const auto fits = [&](Fx p) {
        eval(p);
        for (std::size_t q = 0; q < n; ++q) {
            if (m[q] > saddle) {
                return false;
            }
        }
        if (!capped) {
            for (std::size_t q = 3; q < n; ++q) {
                if (abs(m[q] - m[q - 3U]) > walk) {
                    return false;
                }
            }
        }
        return true;
    };
    const auto top = [&](Fx p) {
        eval(p);
        Fx mx;
        for (std::size_t q = 0; q < n; ++q) {
            mx = max(mx, m[q]);
        }
        return mx;
    };
    PassDepth out;
    out.capped = capped;
    Fx lo;
    if (fits(Fx::ONE)) {
        lo = Fx::ONE;
    } else {
        Fx hi = Fx::ONE;
        for (std::uint32_t step = 0; step < DEPTH_STEPS; ++step) {
            const Fx mid = Fx::from_raw((lo.raw() + hi.raw()) >> 1U);
            if (fits(mid)) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
    }
    const Fx floor_m = Fx::from_raw(maxm << 16U) - t.pass_max_cut_m;
    if (top(lo) < floor_m) {
        Fx plo = lo;
        Fx phi = Fx::ONE;
        for (std::uint32_t step = 0; step < DEPTH_STEPS; ++step) {
            const Fx mid = Fx::from_raw((plo.raw() + phi.raw()) >> 1U);
            if (top(mid) >= floor_m) {
                phi = mid;
            } else {
                plo = mid;
            }
        }
        lo = phi;
        out.capped = true;
    }
    out.lo = lo;
    return out;
}

Fx pass_bump(Fx dist, Fx dend, Fx halfwidth, Fx width_var, Fx nz) {
    const Fx hwv = mul(halfwidth, Fx::ONE + mul(width_var, nz));
    if (dist >= hwv) {
        return Fx();
    }
    return mul(fade(Fx::ONE - div(dist, hwv)), fade(clamp(div(dend, Fx::from_int(END_TAPER_M)), Fx(), Fx::ONE)));
}

std::vector<std::int64_t> pass_drainage(std::span<const std::int64_t> s, std::span<const std::int64_t> nat,
                                        std::int32_t col_index, Fx eps) {
    if (s.size() != nat.size() || col_index < 0 || static_cast<std::size_t>(col_index) + 1U >= s.size()) {
        fatal("pass_drainage: lengths differ or the col is not followed by its partner");
    }
    const std::size_t n = s.size();
    const auto ci = static_cast<std::size_t>(col_index);
    std::vector<Fx> u(n);
    std::vector<Fx> l(n);
    for (std::size_t q = 0; q < n; ++q) {
        u[q] = Fx::from_raw(s[q]);
        l[q] = Fx::from_raw(nat[q]);
    }
    for (std::size_t q = ci; q-- > 0;) {
        u[q] = min(Fx::from_raw(s[q]), u[q + 1U] - eps);
    }
    for (std::size_t q = ci + 1U; q < n; ++q) {
        u[q] = min(Fx::from_raw(s[q]), u[q - 1U] - eps);
    }
    for (std::size_t q = 1; q <= ci; ++q) {
        l[q] = l[q - 1U] + eps;
    }
    for (std::size_t q = n - 1U; q-- > ci + 1U;) {
        l[q] = l[q + 1U] + eps;
    }
    std::vector<std::int64_t> out(n);
    for (std::size_t q = 0; q < n; ++q) {
        out[q] = (max(u[q], min(Fx::from_raw(nat[q]), l[q])) - Fx::from_raw(s[q])).raw();
    }
    return out;
}

PassLayout pass_layout(std::uint64_t seed, const PassInputs& in, const ElevationTuning& tuning) {
    const Grid grid = in.grid;
    check_len(grid, in.h0.size(), "pass_layout: h0");
    check_len(grid, in.unit.size(), "pass_layout: unit");
    check_len(grid, in.mask.size(), "pass_layout: mask");
    check_len(grid, in.profile.size(), "pass_layout: profile");
    check_len(grid, in.nb.size(), "pass_layout: nb");
    check_len(grid, in.ocean.size(), "pass_layout: ocean");
    if (in.w_b <= Fx()) {
        fatal("pass_layout: w_b must be positive");
    }
    Layout layout(seed, in, tuning);
    return layout.run();
}

}  // namespace worldgen::stages
