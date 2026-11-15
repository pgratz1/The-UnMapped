// G3 routing flood, G4 receivers, G5 accumulation (P1-00 7.3 to 7.5).

#include "worldgen/geo/flow.hpp"

#include <array>
#include <bit>
#include <cstddef>

#include "worldgen/core/error.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/geo/blocks.hpp"

namespace worldgen::geo {

namespace {

// Marks a cell not yet reached by the flood (parent values are 0..8).
constexpr Direction UNVISITED = 0xFF;

// Neighbour index offsets per D8 code for a grid of this width.
std::array<std::int64_t, 8> neighbour_offsets(Grid grid) noexcept {
    std::array<std::int64_t, 8> d{};
    for (std::size_t k = 0; k < 8U; ++k) {
        d[k] = std::int64_t{D8_DY[k]} * grid.width + D8_DX[k];
    }
    return d;
}

}  // namespace

// --- G3 ---------------------------------------------------------------------------------

// A monotone radix heap keyed by uint64, with FIFO buckets.
//
// Why the pop order is exactly ascending (key, seq), seq the push counter:
// `last` is the key of the most recent pop; every pushed key is >= last
// (G3 pushes max(h, L[c]) >= L[c]). An entry with key k sits in bucket
// bit_length(k ^ last), so bucket 0 holds exactly the entries with key ==
// last, the current minimum. Each bucket keeps its entries in push order:
// a push appends the newest entry; when bucket 0 is empty, the lowest
// non-empty bucket b is emptied into the (then all empty) lower buckets by
// scanning it in order, which keeps the order of every subsequence; popping
// takes bucket 0's oldest entry. So among the minimum-key entries the one
// popped has the smallest seq, which is the (key, seq) order of P1-00 7.3.
// The rest is the standard radix heap: when the minimum m of bucket b
// becomes `last`, entries in buckets above b keep their bucket, since m and
// the old last agree on every bit from b - 1 up.
struct RoutingFloodScratch::Impl {
    struct Entry {
        std::uint64_t key;
        CellIndex cell;
    };
    static constexpr std::size_t BUCKETS = 65;  // bit_length of a uint64 xor: 0 .. 64

    std::array<std::vector<Entry>, BUCKETS> buckets;
    std::size_t head0 = 0;     // next entry of bucket 0 to pop
    std::uint64_t nonempty = 0;  // bit b - 1 set when bucket b (1..64) is non-empty
    std::uint64_t last = 0;
    std::uint64_t size = 0;

    void reset() noexcept {
        for (std::vector<Entry>& b : buckets) {
            b.clear();
        }
        head0 = 0;
        nonempty = 0;
        last = 0;
        size = 0;
    }

    void push(std::uint64_t key, CellIndex cell) {
        const auto b = static_cast<std::size_t>(bit_length(key ^ last));
        buckets[b].push_back(Entry{key, cell});
        if (b != 0U) {
            nonempty |= std::uint64_t{1} << (b - 1U);
        }
        ++size;
    }

    // Pops the entry with the smallest (key, seq). size must be > 0.
    Entry pop() {
        std::vector<Entry>& b0 = buckets[0];
        if (head0 == b0.size()) {
            b0.clear();
            head0 = 0;
            const auto b = static_cast<std::size_t>(std::countr_zero(nonempty)) + 1U;
            std::vector<Entry>& src = buckets[b];
            std::uint64_t m = src[0].key;
            for (const Entry& e : src) {
                m = e.key < m ? e.key : m;
            }
            last = m;
            nonempty &= ~(std::uint64_t{1} << (b - 1U));
            for (const Entry& e : src) {
                const auto t = static_cast<std::size_t>(bit_length(e.key ^ m));  // t < b
                buckets[t].push_back(e);
                if (t != 0U) {
                    nonempty |= std::uint64_t{1} << (t - 1U);
                }
            }
            src.clear();
        }
        --size;
        const Entry e = b0[head0];
        ++head0;
        return e;
    }
};

RoutingFloodScratch::RoutingFloodScratch() : impl_(std::make_unique<Impl>()) {}
RoutingFloodScratch::~RoutingFloodScratch() = default;
RoutingFloodScratch::RoutingFloodScratch(RoutingFloodScratch&&) noexcept = default;
RoutingFloodScratch& RoutingFloodScratch::operator=(RoutingFloodScratch&&) noexcept = default;

void routing_flood(Grid grid, std::span<const std::int64_t> keys, std::span<const std::uint8_t> ocean,
                   RoutingFlood& out, RoutingFloodScratch& scratch) {
    check_grid_array(grid, keys.size(), "routing_flood: keys");
    check_grid_array(grid, ocean.size(), "routing_flood: ocean");
    if (!scratch.impl_) {
        scratch.impl_ = std::make_unique<RoutingFloodScratch::Impl>();  // a moved-from scratch
    }
    RoutingFloodScratch::Impl& q = *scratch.impl_;
    q.reset();

    const std::uint32_t n = grid.cells();
    out.level.assign(n, 0);
    out.parent.assign(n, UNVISITED);
    out.order.clear();
    out.order.reserve(n);

    // Seeds: every ocean cell in index order, key 0, seq 0, 1, ...
    for (CellIndex i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            out.parent[i] = NO_DIRECTION;
            q.push(0U, i);
        } else if (keys[i] < 1) {
            fatal("routing_flood: a non-ocean key is below 1");
        }
    }
    if (q.size == 0U) {
        fatal("routing_flood: no cell is ocean");
    }

    const std::array<std::int64_t, 8> offset = neighbour_offsets(grid);
    const std::int32_t w = grid.width;
    const std::int32_t h = grid.height;
    std::int64_t* const level = out.level.data();
    Direction* const parent = out.parent.data();

    while (q.size != 0U) {
        const RoutingFloodScratch::Impl::Entry e = q.pop();
        const CellIndex c = e.cell;
        out.order.push_back(c);
        const auto lc = static_cast<std::int64_t>(e.key);  // == level[c]
        const std::int32_t x = grid.x_of(c);
        const std::int32_t y = grid.y_of(c);
        const bool interior = x > 0 && y > 0 && x < w - 1 && y < h - 1;
        for (std::size_t code = 0; code < 8U; ++code) {
            CellIndex m = 0;
            if (interior) {
                m = static_cast<CellIndex>(static_cast<std::int64_t>(c) + offset[code]);
            } else {
                const std::int32_t mx = x + D8_DX[code];
                const std::int32_t my = y + D8_DY[code];
                if (!grid.contains(mx, my)) {
                    continue;
                }
                m = grid.index(mx, my);
            }
            if (parent[m] != UNVISITED) {
                continue;
            }
            const std::int64_t lm = keys[m] > lc ? keys[m] : lc;
            level[m] = lm;
            parent[m] = opposite(static_cast<Direction>(code));
            q.push(static_cast<std::uint64_t>(lm), m);
        }
    }
}

RoutingFlood routing_flood(Grid grid, std::span<const std::int64_t> keys, std::span<const std::uint8_t> ocean) {
    RoutingFlood out;
    RoutingFloodScratch scratch;
    routing_flood(grid, keys, ocean, out, scratch);
    return out;
}

// --- G4 ---------------------------------------------------------------------------------

namespace {

// True if (w_a, d_a) scores strictly higher than (w_b, d_b): w_a * d_a^2 >
// w_b * d_b^2, exactly. Drops are positive and below 2^63, so w * d^2 <
// 2^127 fits U128; when both drops are below 2^31 the products fit uint64
// and are compared directly (the same comparison).
bool score_greater(std::uint64_t wa, std::uint64_t da, std::uint64_t wb, std::uint64_t db) noexcept {
    constexpr std::uint64_t SMALL = std::uint64_t{1} << 31U;
    if (da < SMALL && db < SMALL) {
        return wa * da * da > wb * db * db;
    }
    const U128 sa = mul_u64_wide(da, da);
    const U128 sb = mul_u64_wide(db, db);
    const U128 a = wa == 2U ? sa + sa : sa;
    const U128 b = wb == 2U ? sb + sb : sb;
    return a > b;
}

void receivers_rows(Grid grid, std::span<const std::int64_t> level, std::span<const Direction> parent,
                    std::span<const std::uint8_t> ocean, std::span<Direction> rcv, std::int32_t y_begin,
                    std::int32_t y_end) {
    for (std::int32_t y = y_begin; y < y_end; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            const CellIndex c = grid.index(x, y);
            if (ocean[c] != 0U) {
                rcv[c] = NO_DIRECTION;
                continue;
            }
            const std::int64_t lc = level[c];
            Direction best = NO_DIRECTION;
            std::uint64_t best_w = 0;
            std::uint64_t best_d = 0;
            for (std::size_t code = 0; code < 8U; ++code) {
                const std::int32_t mx = x + D8_DX[code];
                const std::int32_t my = y + D8_DY[code];
                if (!grid.contains(mx, my)) {
                    continue;
                }
                const std::int64_t lm = level[grid.index(mx, my)];
                if (lm >= lc) {
                    continue;
                }
                const auto d = static_cast<std::uint64_t>(lc - lm);  // 0 <= lm < lc
                const std::uint64_t wgt = is_diagonal(static_cast<Direction>(code)) ? 1U : 2U;
                if (best == NO_DIRECTION || score_greater(wgt, d, best_w, best_d)) {
                    best = static_cast<Direction>(code);
                    best_w = wgt;
                    best_d = d;
                }
            }
            if (best == NO_DIRECTION) {
                best = parent[c];
                if (best > 7U) {
                    fatal("receivers: a cell on a flat has no flood parent");
                }
            }
            rcv[c] = best;
        }
    }
}

}  // namespace

void receivers(Grid grid, std::span<const std::int64_t> level, std::span<const Direction> parent,
               std::span<const std::uint8_t> ocean, std::span<Direction> rcv, std::uint32_t threads) {
    check_grid_array(grid, level.size(), "receivers: level");
    check_grid_array(grid, parent.size(), "receivers: parent");
    check_grid_array(grid, ocean.size(), "receivers: ocean");
    check_grid_array(grid, rcv.size(), "receivers: rcv");
    for (const std::int64_t v : level) {
        if (v < 0) {
            fatal("receivers: a flood level is negative");
        }
    }
    for_each_row_block(grid.height, threads, [&](const RowBlock& block) {
        receivers_rows(grid, level, parent, ocean, rcv, block.y_begin, block.y_end);
    });
}

std::vector<Direction> receivers(Grid grid, std::span<const std::int64_t> level, std::span<const Direction> parent,
                                 std::span<const std::uint8_t> ocean, std::uint32_t threads) {
    check_grid(grid);
    std::vector<Direction> rcv(grid.cells(), NO_DIRECTION);
    receivers(grid, level, parent, ocean, rcv, threads);
    return rcv;
}

// --- G5 ---------------------------------------------------------------------------------

namespace {

// The receiver cell of non-ocean cell c. Fatal unless rcv[c] is 0..7 and
// points inside the grid.
CellIndex receiver_cell(Grid grid, std::span<const Direction> rcv, CellIndex c) {
    const Direction code = rcv[c];
    CellIndex m = 0;
    if (code > 7U || !neighbour(grid, c, code, m)) {
        fatal("accumulate: a receiver is not a D8 code pointing inside the grid");
    }
    return m;
}

}  // namespace

std::vector<std::uint32_t> accumulate(Grid grid, std::span<const Direction> rcv, std::span<const std::uint8_t> ocean,
                                      std::span<const CellIndex> order) {
    check_grid_array(grid, rcv.size(), "accumulate: rcv");
    check_grid_array(grid, ocean.size(), "accumulate: ocean");
    check_grid_array(grid, order.size(), "accumulate: order");
    const std::uint32_t n = grid.cells();
    std::vector<std::uint32_t> a(n, 0U);
    for (CellIndex c = 0; c < n; ++c) {
        a[c] = ocean[c] != 0U ? 0U : 1U;
    }
    // done[c]: c has been processed. Every cell must appear once, and a
    // receiver must not be processed before its donor (it comes earlier in
    // order). Totals are at most n <= 2^24, so uint32 never overflows.
    std::vector<std::uint8_t> done(n, 0U);
    for (std::size_t j = order.size(); j-- > 0U;) {
        const CellIndex c = order[j];
        if (c >= n || done[c] != 0U) {
            fatal("accumulate: order is not a permutation of the cells");
        }
        done[c] = 1U;
        if (ocean[c] != 0U) {
            continue;
        }
        const CellIndex m = receiver_cell(grid, rcv, c);
        if (ocean[m] != 0U) {
            continue;
        }
        if (done[m] != 0U) {
            fatal("accumulate: order lists a receiver after its donor");
        }
        a[m] += a[c];
    }
    return a;
}

std::vector<std::uint32_t> accumulate_topological(Grid grid, std::span<const Direction> rcv,
                                                  std::span<const std::uint8_t> ocean) {
    check_grid_array(grid, rcv.size(), "accumulate_topological: rcv");
    check_grid_array(grid, ocean.size(), "accumulate_topological: ocean");
    const std::uint32_t n = grid.cells();
    std::vector<std::uint32_t> a(n, 0U);
    std::vector<CellIndex> down(n, 0U);
    std::vector<std::uint32_t> indeg(n, 0U);
    std::uint32_t land = 0;
    for (CellIndex c = 0; c < n; ++c) {
        if (ocean[c] != 0U) {
            continue;
        }
        ++land;
        a[c] = 1U;
        down[c] = receiver_cell(grid, rcv, c);
        if (ocean[down[c]] == 0U) {
            ++indeg[down[c]];
        }
    }
    std::vector<CellIndex> queue;
    queue.reserve(land);
    for (CellIndex c = 0; c < n; ++c) {
        if (ocean[c] == 0U && indeg[c] == 0U) {
            queue.push_back(c);
        }
    }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const CellIndex u = queue[head];
        const CellIndex m = down[u];
        if (ocean[m] != 0U) {
            continue;
        }
        a[m] += a[u];
        if (--indeg[m] == 0U) {
            queue.push_back(m);
        }
    }
    if (queue.size() != land) {
        fatal("accumulate_topological: the receivers contain a cycle");
    }
    return a;
}

}  // namespace worldgen::geo
