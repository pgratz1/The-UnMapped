// Spec V3D-03 11.1, U19 to U23 and U25 (near part): the near grid, the
// wanted set, hysteresis, order and budgets, teleport and cancel. Workers
// are simulated: a cell handed out by start_computing is completed by the
// test when it chooses.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "terrain/near_cells.hpp"
#include "terrain/terrain_data.hpp"
#include "terrain/vegetation.hpp"

using namespace unmapped::terrain;

namespace {

Layout layout_of(int n) {
    std::string problem;
    const auto layout = layout_for(n, n, problem);
    REQUIRE(layout.has_value());
    return *layout;
}

std::uint64_t own_mix(std::uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// A 512 world of forest with rough ground (heights from -50 to 950 m).
struct World {
    Layout layout;
    std::vector<std::int16_t> elevation;
    std::vector<float> heights;
    std::vector<std::uint8_t> timber, water;
    GroundCells cells;
    VegetationInputs in;
};

World rough_world() {
    World w;
    w.layout = layout_of(512);
    const std::size_t total = 512u * 512u;
    w.elevation.resize(total);
    w.heights.resize(total);
    w.timber.assign(total, 255);
    w.water.assign(total, 0);
    w.cells.type.assign(total, 2);
    w.cells.painted.assign(total, 0);
    w.cells.rock.assign(total, 0);
    w.cells.snow.assign(total, 0);
    for (std::size_t k = 0; k < total; ++k) {
        w.elevation[k] = static_cast<std::int16_t>(static_cast<std::int64_t>(own_mix(k) % 10000) - 500);
        w.heights[k] = static_cast<float>(w.elevation[k]) / 10.0f;
        w.cells.type[k] = static_cast<std::uint8_t>(k % 7 == 0 ? 3 : 2);
    }
    return w;
}

// The test's own copy of 6.1.
struct OwnBox {
    double x0, x1, z0, z1, bot, top;
};

OwnBox own_box(const World& w, int i, int j) {
    const int n = w.layout.n;
    OwnBox b{};
    b.x0 = 16.0 * (16 * i - n / 2) - 8.0;
    b.x1 = b.x0 + 256.0;
    b.z0 = 16.0 * (16 * j - n / 2) - 8.0;
    b.z1 = b.z0 + 256.0;
    double lo = 1e30, hi = -1e30;
    for (int y = 16 * j - 1; y <= 16 * j + 16; ++y) {
        for (int x = 16 * i - 1; x <= 16 * i + 16; ++x) {
            const int cx = std::clamp(x, 0, n - 1), cy = std::clamp(y, 0, n - 1);
            const double h = static_cast<double>(w.heights[static_cast<std::size_t>(cy) * 512 + static_cast<std::size_t>(cx)]);
            lo = std::min(lo, h);
            hi = std::max(hi, h);
        }
    }
    b.bot = lo - 2.0;
    b.top = hi + 22.0;
    return b;
}

double own_d(const OwnBox& b, double px, double py, double pz) {
    const double dx = std::max({0.0, b.x0 - px, px - b.x1});
    const double dz = std::max({0.0, b.z0 - pz, pz - b.z1});
    const double dy = std::max({0.0, b.bot - py, py - b.top});
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// A small synthetic grid: m x m boxes of 256 m, flat ground at 0.
NearGrid flat_grid(int m) {
    NearGrid g;
    g.m = m;
    for (int j = 0; j < m; ++j) {
        for (int i = 0; i < m; ++i) {
            NearBox b;
            b.x0 = 256.0 * i - 128.0 * m;
            b.x1 = b.x0 + 256.0;
            b.z0 = 256.0 * j - 128.0 * m;
            b.z1 = b.z0 + 256.0;
            b.bot = -2.0;
            b.top = 22.0;
            g.boxes.push_back(b);
        }
    }
    return g;
}

NearViewer at(double x, double z, double y = 10.0, double heading = 0.0) {
    NearViewer v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.heading_deg = heading;
    return v;
}

// One simulated frame: update, release, drop, attach, start, and complete
// everything started at once.
void frame(NearCells& cells, const NearViewer& v, std::uint64_t n = 100) {
    cells.update(v);
    (void)cells.take_released();
    (void)cells.take_drops();
    (void)cells.take_attach();
    for (const int q : cells.start_computing()) {
        cells.computed(q, n);
    }
}

std::set<int> states(const NearCells& cells, NearState s) {
    std::set<int> out;
    for (const int q : cells.active()) {
        if (cells.state(q) == s) {
            out.insert(q);
        }
    }
    return out;
}

}  // namespace

TEST_CASE("U19 build_near_grid: boxes hold every instance of their cell") {
    const World w = rough_world();
    const NearGrid grid = build_near_grid(w.layout, w.heights);
    REQUIRE(grid.m == 32);
    REQUIRE(grid.boxes.size() == 32u * 32u);
    VegetationInputs in;
    in.layout = &w.layout;
    in.heights = w.heights;
    in.elevation = w.elevation;
    in.timber = w.timber;
    in.water = w.water;
    in.cells = &w.cells;
    in.seed = 3;
    std::size_t outside = 0;
    std::size_t total = 0;
    std::size_t box_bad = 0;
    for (int j = 0; j < 32; ++j) {
        for (int i = 0; i < 32; ++i) {
            const NearBox& b = grid.boxes[static_cast<std::size_t>(j * 32 + i)];
            const OwnBox o = own_box(w, i, j);
            box_bad += (b.x0 == o.x0 && b.x1 == o.x1 && b.z0 == o.z0 && b.z1 == o.z1 && b.bot == o.bot && b.top == o.top) ? 0u : 1u;
            for (const Candidate& c : place_candidates(in, 16 * i, 16 * j, 16 * i + 16, 16 * j + 16)) {
                ++total;
                const auto x = static_cast<double>(c.instance.x);
                const auto y = static_cast<double>(c.instance.y);
                const auto z = static_cast<double>(c.instance.z);
                const bool inside = x >= b.x0 && x <= b.x1 && z >= b.z0 && z <= b.z1 && y >= b.bot && y <= b.top &&
                                    y + 15.5 * 1.4 < b.top;
                outside += inside ? 0u : 1u;
            }
        }
    }
    CHECK(box_bad == 0);
    CHECK(total > 100000);
    CHECK(outside == 0);
    CHECK(build_near_grid(layout_of(1024), std::vector<float>(1024u * 1024u, 0.0f)).m == 64);
    CHECK(build_near_grid(layout_of(2048), std::vector<float>(2048u * 2048u, 0.0f)).m == 128);
}

TEST_CASE("U20 the wanted set of one update equals a brute-force pass") {
    const World w = rough_world();
    NearParams p;
    const NearGrid grid = build_near_grid(w.layout, w.heights);
    const double e = 4096.0;  // half the map side
    const std::vector<NearViewer> points = {
        at(0, 0, 500),          at(-e, -e, 300),        at(e, e, 300),          at(-e, e, 200),
        at(e, -e, 1000),        at(0, -e, 120),         at(-e, 0, 80),          at(-e - 1000, 0, 100),
        at(0, e + 1000, 5000),  at(100, 100, 30000),    at(-7.0, 300.5, 60.0),  at(1000, -2000, 2500),
    };
    for (const NearViewer& v : points) {
        CAPTURE(v.x);
        CAPTURE(v.z);
        CAPTURE(v.y);
        NearCells cells(grid, p);
        cells.update(v);
        const std::set<int> got = states(cells, NearState::queued);
        std::set<int> want;
        for (int j = 0; j < 32; ++j) {
            for (int i = 0; i < 32; ++i) {
                if (own_d(own_box(w, i, j), v.x, v.y, v.z) <= p.radius) {
                    want.insert(j * 32 + i);
                }
            }
        }
        CHECK(got == want);
        for (const int q : got) {
            CHECK(q >= 0);
            CHECK(q < 32 * 32);
        }
    }
}

TEST_CASE("U21 hysteresis") {
    NearParams p;
    NearCells cells(flat_grid(64), p);
    // A cell at the centre column; the camera walks east along z = 128 from
    // inside the cell to 4 km beyond it and back, 1 m at a time.
    const int q0 = 32 * 64 + 32;  // box x [0, 256], z [0, 256]
    std::map<int, int> queued_times;
    std::map<int, NearState> last;
    bool bad_enter = false;
    bool bad_drop = false;
    const NearGrid& g = cells.grid();
    auto step = [&](double x) {
        const NearViewer v = at(x, 128.0, 10.0, 90.0);
        cells.update(v);
        for (const int q : cells.active()) {
            const NearState s = cells.state(q);
            const NearState before = last.count(q) ? last[q] : NearState::empty;
            const double d = near_distance(g.boxes[static_cast<std::size_t>(q)], v);
            if (s == NearState::queued && before == NearState::empty) {
                ++queued_times[q];
                bad_enter = bad_enter || d > p.radius;
            }
            if (s == NearState::dropping && before == NearState::filled) {
                bad_drop = bad_drop || d <= p.radius + p.hysteresis;
            }
        }
        (void)cells.take_released();
        (void)cells.take_drops();
        (void)cells.take_attach();
        for (const int q : cells.start_computing()) {
            cells.computed(q, 10);
        }
        last.clear();
        for (const int q : cells.active()) {
            last[q] = cells.state(q);
        }
    };
    for (int x = 128; x <= 4500; ++x) {
        step(x);
    }
    CHECK(cells.state(q0) == NearState::empty);
    for (int x = 4500; x >= 128; --x) {
        step(x);
    }
    CHECK_FALSE(bad_enter);
    CHECK_FALSE(bad_drop);
    // On the way out and back each cell of the row was queued at most twice
    // (once out, once back), and the start cell once in each direction.
    CHECK(queued_times[q0] == 2);
    for (const auto& [q, times] : queued_times) {
        CHECK(times <= 2);
    }
    // A +-128 m wiggle 200 times changes no state of a filled cell.
    NearCells still(flat_grid(64), p);
    for (int k = 0; k < 400; ++k) {
        frame(still, at(1000.0, 128.0));
    }
    REQUIRE(still.settled());
    const std::set<int> filled = states(still, NearState::filled);
    REQUIRE(!filled.empty());
    for (int k = 0; k < 200; ++k) {
        const double x = 1000.0 + ((k % 2 == 0) ? 128.0 : -128.0);
        frame(still, at(x, 128.0 + ((k % 4 < 2) ? 128.0 : -128.0)));
        bool all = true;
        for (const int q : filled) {
            all = all && still.state(q) == NearState::filled;
        }
        REQUIRE(all);
    }
}

TEST_CASE("U22 order and budgets") {
    NearParams p;
    NearCells cells(flat_grid(64), p);
    const NearViewer v = at(0.0, 0.0, 10.0, 90.0);
    cells.update(v);
    const NearGrid& g = cells.grid();
    // start_computing follows (in_view, d, q) and never exceeds max_in_flight.
    const std::set<int> queued_set = states(cells, NearState::queued);
    std::vector<int> queued(queued_set.begin(), queued_set.end());
    auto key = [&](int q) {
        const NearBox& b = g.boxes[static_cast<std::size_t>(q)];
        return std::make_tuple(near_in_view(b, v) ? 0 : 1, near_distance(b, v), q);
    };
    std::sort(queued.begin(), queued.end(), [&](int a, int b) { return key(a) < key(b); });
    const std::vector<int> first = cells.start_computing();
    REQUIRE(first.size() == 4);
    CHECK(std::equal(first.begin(), first.end(), queued.begin()));
    CHECK(cells.start_computing().empty());
    CHECK(cells.counts().computing == 4);
    cells.computed(first[0], 10);
    const std::vector<int> second = cells.start_computing();
    REQUIRE(second.size() == 1);
    CHECK(second[0] == queued[4]);
    CHECK(cells.counts().computing == 4);
    // The in-view cells (east, heading 90) come first.
    CHECK(near_in_view(g.boxes[static_cast<std::size_t>(queued[0])], v));

    // take_attach: at most 4 cells and 16,384 instances, but always one.
    NearCells big(flat_grid(64), p);
    big.update(v);
    std::vector<int> done;
    for (int r = 0; r < 3; ++r) {
        for (const int q : big.start_computing()) {
            done.push_back(q);
        }
        for (std::size_t k = done.size() - 4; k < done.size(); ++k) {
            big.computed(done[k], 5000);
        }
    }
    REQUIRE(big.counts().ready == 12);
    const std::vector<int> a1 = big.take_attach();
    CHECK(a1.size() == 3);  // 15,000; a fourth would pass 16,384
    NearCells huge(flat_grid(64), p);
    huge.update(v);
    for (const int q : huge.start_computing()) {
        huge.computed(q, 20000);
    }
    CHECK(huge.take_attach().size() == 1);
    CHECK(huge.take_attach().size() == 1);
    NearCells small(flat_grid(64), p);
    small.update(v);
    for (int r = 0; r < 3; ++r) {
        for (const int q : small.start_computing()) {
            small.computed(q, 1);
        }
    }
    CHECK(small.take_attach().size() == 4);
    CHECK(small.take_attach().size() == 4);
    CHECK(small.take_attach().size() == 4);

    // take_drops: at most 16, farthest first.
    NearCells drops(flat_grid(64), p);
    for (int k = 0; k < 400; ++k) {
        frame(drops, v);
    }
    REQUIRE(drops.counts().pending() == 0);
    const std::uint64_t filled = drops.counts().filled;
    REQUIRE(filled > 100);
    const NearViewer away = at(7000.0, 7000.0);
    drops.update(away);
    const std::set<int> dropping_set = states(drops, NearState::dropping);
    std::vector<int> dropping(dropping_set.begin(), dropping_set.end());
    REQUIRE(dropping.size() == filled);
    std::sort(dropping.begin(), dropping.end(), [&](int a, int b) {
        const double da = near_distance(g.boxes[static_cast<std::size_t>(a)], away);
        const double db = near_distance(g.boxes[static_cast<std::size_t>(b)], away);
        return da != db ? da > db : a < b;
    });
    const std::vector<int> d1 = drops.take_drops();
    REQUIRE(d1.size() == 16);
    CHECK(std::equal(d1.begin(), d1.end(), dropping.begin()));
    for (const int q : d1) {
        CHECK(drops.state(q) == NearState::empty);
    }
}

TEST_CASE("U23 teleport and cancel") {
    NearParams p;
    NearCells cells(flat_grid(64), p);
    const NearViewer A = at(-3000.0, -3000.0);
    const NearViewer B = at(7000.0, 4000.0);  // 10 km away... and more
    // Fill around A, then leave cells in every state.
    for (int k = 0; k < 400; ++k) {
        frame(cells, A);
    }
    REQUIRE(cells.settled());
    const std::set<int> a_cells = states(cells, NearState::filled);
    REQUIRE(a_cells.size() > 100);
    // A second camera point near A queues more cells; start some and leave
    // them computing.
    const NearViewer A2 = at(-3000.0 + 1500.0, -3000.0);
    cells.update(A2);
    const std::vector<int> started = cells.start_computing();
    REQUIRE(started.size() == 4);
    const std::set<int> still_queued = states(cells, NearState::queued);
    REQUIRE(!still_queued.empty());
    // Teleport: every filled A cell becomes dropping in one update; queued
    // cells empty; computing ones abandoned.
    cells.update(B);
    for (const int q : a_cells) {
        if (std::find(started.begin(), started.end(), q) != started.end()) {
            continue;
        }
        CHECK(cells.state(q) == NearState::dropping);
    }
    for (const int q : still_queued) {
        CHECK(cells.state(q) == NearState::empty);
    }
    for (const int q : started) {
        CHECK(cells.state(q) == NearState::computing);
        CHECK(cells.abandoned(q));
    }
    // One computed while abandoned: released.
    cells.computed(started[0], 7);
    CHECK(cells.state(started[0]) == NearState::empty);
    const std::vector<int> released = cells.take_released();
    REQUIRE(released.size() == 1);
    CHECK(released[0] == started[0]);
    // Back to A2 before the others finish: not abandoned, not queued twice.
    cells.update(A2);
    for (std::size_t k = 1; k < started.size(); ++k) {
        CHECK(cells.state(started[k]) == NearState::computing);
        CHECK_FALSE(cells.abandoned(started[k]));
    }
    // Three still computing: one slot is free, and it goes to a cell that
    // is not one of them (the released one may be wanted again).
    const std::vector<int> next = cells.start_computing();
    CHECK(next.size() <= 1);
    for (const int q : next) {
        CHECK(std::find(started.begin() + 1, started.end(), q) == started.end());
        cells.computed(q, 7);
    }
    for (std::size_t k = 1; k < started.size(); ++k) {
        cells.computed(started[k], 7);
        CHECK(cells.state(started[k]) == NearState::ready);
    }
    const std::vector<int> more = cells.start_computing();
    for (const int q : more) {
        CHECK(std::find(started.begin() + 1, started.end(), q) == started.end());
        cells.computed(q, 7);
    }
    for (int k = 0; k < 400; ++k) {
        frame(cells, A2);
    }
    CHECK(cells.settled());
    CHECK(cells.take_released().empty());
}

TEST_CASE("U25 near constants and defaults") {
    CHECK(NEAR_CELL_CELLS == 16);
    CHECK(NEAR_FULL_END == 1200.0);
    CHECK(NEAR_FADE == 100.0);
    CHECK(NEAR_VISIBLE_MAX == 1400.0);
    CHECK(NEAR_VISIBLE_MAX == NEAR_FULL_END + 2 * NEAR_FADE);
    const NearParams p;
    CHECK(p.radius == 2048.0);
    CHECK(p.hysteresis == 512.0);
    CHECK(p.max_in_flight == 4);
    CHECK(p.max_attach_cells == 4);
    CHECK(p.max_drop_cells == 16);
    CHECK(p.max_attach_instances == 16384);
    CHECK(NEAR_RADIUS_MIN - NEAR_VISIBLE_MAX > 0.0);
    CHECK(NEAR_RADIUS_MAX == 8192.0);
}
