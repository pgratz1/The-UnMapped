// Spec P3-04 12, tests 16.2 items 15 to 17: the scheduler over seeds,
// spans, detail levels and the 63 race subsets (invariant 1, SC1-SC4
// recomputed by the test's own code, SC5-SC6 recomputed for
// ForcedTension(25 k, 5000, 1), k = 1, 3, 7); hand-made tuning within the
// schema ranges with packed age starts; and stub worlds on synthetic
// geography through generate_world_with_stages (2 to 60 regions, islets,
// fewer non-islet regions than races so that S1 sets 2 and 3 and the S2
// row-major fallback are reached), with S1, S2 and invariants 2 to 6
// re-derived here. Seeds 0-15 by default; the larger sweep is in the suite
// "history-slow".

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;

// A test-owned sequence (splitmix64), not the project's generator.
struct Mix {
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    std::int64_t in(std::int64_t lo, std::int64_t hi) {
        return lo + static_cast<std::int64_t>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

struct OwnPhase {
    std::int64_t kind, start, end;  // end inclusive
};

// SC1-SC6 by the test (no events: triggers not compared). tension_period 0 = none.
std::vector<OwnPhase> own_phases(const std::vector<std::int64_t>& y, std::int64_t e, const Detail& d,
                                 const EngineTuning& t, std::int64_t period, std::int64_t value) {
    const OwnBase b = own_base(y, e, d, t);
    std::vector<OwnPhase> out;
    std::int64_t flare_total = 0;
    auto quiet = [&](std::int64_t qs, std::int64_t qe) {
        std::int64_t cur = qs;
        while (cur < qe) {
            std::int64_t s = cur;
            const std::int64_t row = cur;
            while (true) {
                const std::int64_t len = std::min(b.q, qe - s);
                s += len;
                if (s == qe) {
                    out.push_back({2, row, s - 1});
                    cur = qe;
                    break;
                }
                if (period != 0 && s % period == 0 && value >= d.flare_threshold) {
                    const std::int64_t f = std::min({b.f, qe - s, b.cap - b.sched - flare_total});
                    if (f >= 1) {
                        out.push_back({2, row, s - 1});
                        out.push_back({3, s, s + f - 1});
                        flare_total += f;
                        cur = s + f;
                        break;
                    }
                }
            }
        }
    };
    const std::size_t p = y.size();
    for (std::size_t k = 0; k < p; ++k) {
        out.push_back({1, y[k], b.ends[k] - 1});
        const std::int64_t qe = k + 1 < p ? y[k + 1] : b.s_f;
        if (b.ends[k] < qe) {
            quiet(b.ends[k], qe);
        }
    }
    if (b.s_f < e) {
        out.push_back({4, b.s_f, e - 1});
    }
    return out;
}

std::string own_text(const std::vector<OwnPhase>& v) {
    std::string s;
    for (const OwnPhase& p : v) {
        s += std::to_string(p.kind) + ":" + std::to_string(p.start) + ":" + std::to_string(p.end) + "|";
    }
    return s;
}
std::string got_text(const Tables& t) {
    std::string s;
    for (const Row& r : t.phase) {
        s += std::to_string(at(r, 2)) + ":" + std::to_string(at(r, 3)) + ":" + std::to_string(at(r, 4)) + "|";
    }
    return s;
}

// Invariant 1 on stored phase rows.
void check_invariant_1(const Tables& t, const std::vector<std::int64_t>& y, std::int64_t e, const OwnBase& b) {
    std::int64_t prev = -1;
    std::vector<std::int64_t> opening;
    for (std::size_t i = 0; i < t.phase.size(); ++i) {
        const Row& r = t.phase[i];
        CHECK(at(r, 0) == static_cast<std::int64_t>(i) + 1);
        CHECK(at(r, 3) == prev + 1);
        CHECK(at(r, 4) >= at(r, 3));
        prev = at(r, 4);
        if (at(r, 2) == 1) {
            opening.push_back(at(r, 3));
        }
        if (at(r, 2) == 4) {
            CHECK(i + 1 == t.phase.size());
        }
        if (at(r, 2) == 3) {
            CHECK((i > 0 && at(t.phase[i - 1], 2) == 2));
        }
    }
    CHECK(prev == e - 1);
    CHECK(opening == y);
    CHECK(simulated(t) >= b.floor);
    CHECK(simulated(t) <= b.cap);
}

Tables run(const std::vector<std::int64_t>& y, std::int64_t e, const std::string& lv, std::int64_t period,
           const wd::ContentSet& c = shipped()) {
    Config cfg;
    cfg.handler = std::make_unique<TestHandler>(false);
    if (period != 0) {
        cfg.add_tension(eng::forced_tension(period, 5000, 1));
    }
    return eng::run_engine(c, inputs_for(y, e, lv), cfg.get());
}

void sweep(std::uint64_t seeds) {
    const std::vector<std::vector<std::string>> subsets = chrono_test::all_subsets();
    REQUIRE(subsets.size() == 63U);
    const EngineTuning t;
    for (std::uint64_t seed = 0; seed < seeds; ++seed) {
        for (const std::int64_t e : {2000, 2001, 6000, 11999, 12000}) {
            for (const std::string lv : {"sparse", "normal", "dense"}) {
                for (std::size_t si = seed % 3; si < subsets.size(); si += 3) {
                    chrono_api::Params p = chrono_test::params_with(e, subsets[si]);
                    const chrono_api::Rows rows = chrono_api::stage(shipped(), p, seed, {});
                    std::vector<std::int64_t> y;
                    for (const auto& a : rows.ages) {
                        y.push_back(a.start_year);
                    }
                    for (const std::int64_t period : {0, 25, 75, 175}) {
                        CAPTURE(seed);
                        CAPTURE(e);
                        CAPTURE(lv);
                        CAPTURE(chrono_test::joined(subsets[si]));
                        CAPTURE(period);
                        const Tables got = run(y, e, lv, period);
                        const Detail d = detail_of(t, lv);
                        check_invariant_1(got, y, e, own_base(y, e, d, t));
                        CHECK(got_text(got) == own_text(own_phases(y, e, d, t, period, 5000)));
                    }
                }
            }
        }
    }
}

// --- Stub worlds on synthetic geography (16.2 item 17) ---------------------------------------------

struct Synthetic {
    std::int64_t n = 512;  // a valid map size (512, 1024, 2048)
    std::vector<db::RegionRow> regions;
    std::vector<std::uint16_t> region;
    std::vector<std::uint8_t> water;
};
Synthetic& synthetic() {
    static Synthetic s;
    return s;
}
Synthetic make_synthetic(std::uint64_t seed) {
    Mix m{seed * 7919 + 1};
    Synthetic s;
    // every fifth seed: every region an islet (most of it lake), so S1 set 3 is reached
    const bool all_islets = seed % 5 == 4;
    const std::int64_t count = m.in(2, 60);
    std::vector<std::int64_t> widths(static_cast<std::size_t>(count), 1);
    for (std::int64_t left = s.n - count; left > 0; --left) {
        // a few regions take most of the width: some non-islets, many islets
        const std::size_t i = static_cast<std::size_t>(m.in(0, std::min<std::int64_t>(count - 1, m.in(0, 3))));
        widths[i] += 1;
    }
    s.region.assign(static_cast<std::size_t>(s.n * s.n), 0);
    s.water.assign(static_cast<std::size_t>(s.n * s.n), 0);
    std::int64_t x = 0;
    for (std::int64_t i = 0; i < count; ++i) {
        const std::int64_t w = widths[static_cast<std::size_t>(i)];
        db::RegionRow r = region_row(i + 1, "Region " + std::to_string(i + 1), x, 0, x + w - 1, s.n - 1, w * s.n,
                                     m.in(0, w * s.n), m.in(40, 45));  // narrow range: ties in habitability
        std::int64_t lakes = 0;
        // islet mode: under 1172 dry cells, but room for six sites
        const std::int64_t dry_rows = std::max<std::int64_t>(1, std::min<std::int64_t>(6, 1100 / w));
        for (std::int64_t yy = 0; yy < s.n; ++yy) {
            for (std::int64_t xx = x; xx < x + w; ++xx) {
                const std::size_t c = static_cast<std::size_t>(yy * s.n + xx);
                s.region[c] = static_cast<std::uint16_t>(i + 1);
                // the cell after the seed cell (row-major) is a lake, and a few more
                const bool lake = (yy == 0 && xx == x + 1) || (all_islets && yy >= dry_rows) ||
                                  (m.next() % 50 == 0 && !(yy == 0 && xx == x));
                if (lake) {
                    s.water[c] = 2;
                    ++lakes;
                }
            }
        }
        r.lake_cells = lakes;
        s.regions.push_back(r);
        x += w;
    }
    return s;
}
void synthetic_hydrology(worldgen::stages::StageContext& ctx) {
    const Synthetic& s = synthetic();
    db::LayerGrid g = db::make_layer_grid(db::ElementType::u8, static_cast<std::uint32_t>(s.n),
                                          static_cast<std::uint32_t>(s.n));
    g.values = s.water;
    ctx.write_layer("water", std::move(g));
}
void synthetic_regions(worldgen::stages::StageContext& ctx) {
    const Synthetic& s = synthetic();
    db::LayerGrid g = db::make_layer_grid(db::ElementType::u16, static_cast<std::uint32_t>(s.n),
                                          static_cast<std::uint32_t>(s.n));
    g.values = s.region;
    ctx.write_layer("region", std::move(g));
    for (const db::RegionRow& r : s.regions) {
        ctx.write_region(r);
    }
}

void check_stub_world(std::uint64_t seed, std::int64_t span, const std::vector<std::string>& races,
                      const std::string& detail) {
    CAPTURE(seed);
    CAPTURE(span);
    CAPTURE(chrono_test::joined(races));
    synthetic() = make_synthetic(seed);
    const Synthetic& s = synthetic();
    static dbtest::TempDir dir("engine-stub");
    const std::string path = dir.file("s" + std::to_string(seed) + "-" + std::to_string(span) + ".db");
    chrono_api::Params p = chrono_test::params_with(span, races);
    p.detail = detail;
    const std::vector<worldgen::stages::StageInfo> stages = {
        {"hydrology", &synthetic_hydrology}, {"regions", &synthetic_regions}, chrono_api::stage_info(),
        eng::stub_stage_info()};  // stub worlds (P3-04 7); the shipped stage is peoples since 0.15.0
    worldgen::WorldParams wp = chrono_test::world_params(seed, s.n, p, shipped());
    const auto st = worldgen::stages::testing::generate_world_with_stages(wp, shipped(), stages, path,
                                                                         db::Overwrite::replace);
    REQUIRE_MESSAGE(st.ok(), st.error().message);
    const db::WorldReader r = open_world(path);
    const Tables t = tables_of_world(r);
    const std::size_t np = races.size();
    // invariant 6: P civilizations and settlements, in race order
    REQUIRE(t.civilization.size() == np);
    REQUIRE(t.settlement.size() == np);
    const auto age_rows = rows_of(r, "age");
    // S1 and S2 re-derived
    std::set<std::int64_t> held;
    std::set<std::pair<std::int64_t, std::int64_t>> taken;
    std::int64_t held_rows = 0;
    const EngineTuning tun;
    for (std::size_t k = 0; k < np; ++k) {
        auto islet = [&](const db::RegionRow& g) { return g.cells - g.lake_cells < tun.islet_max_dry_cells; };
        auto best = [&](auto pred) -> std::int64_t {
            const db::RegionRow* b = nullptr;
            for (const db::RegionRow& g : s.regions) {
                if (!pred(g)) continue;
                if (b == nullptr || g.habitability_mean > b->habitability_mean ||
                    (g.habitability_mean == b->habitability_mean && g.walk_cells > b->walk_cells)) {
                    b = &g;
                }
            }
            return b == nullptr ? 0 : b->id;
        };
        std::int64_t home = best([&](const db::RegionRow& g) { return !islet(g) && held.count(g.id) == 0; });
        if (home == 0) home = best([&](const db::RegionRow& g) { return !islet(g); });
        if (home == 0) home = best([](const db::RegionRow&) { return true; });
        CHECK(at(t.civilization[k], 4) == home);
        CHECK(at(t.civilization[k], 2) == age_rows[k][1].integer);  // race of the age
        CHECK(at(t.civilization[k], 6) == age_rows[k][2].integer);  // founded at the awakening
        if (held.insert(home).second) {
            ++held_rows;
        }
        const db::RegionRow& g = s.regions[static_cast<std::size_t>(home - 1)];
        std::pair<std::int64_t, std::int64_t> site{g.seed_x, g.seed_y};
        if (taken.count(site) != 0) {
            bool found = false;
            for (std::int64_t yy = g.y_min; yy <= g.y_max && !found; ++yy) {
                for (std::int64_t xx = g.x_min; xx <= g.x_max && !found; ++xx) {
                    const std::size_t c = static_cast<std::size_t>(yy * s.n + xx);
                    if (s.region[c] == home && s.water[c] != 2 && taken.count({xx, yy}) == 0) {
                        site = {xx, yy};
                        found = true;
                    }
                }
            }
            REQUIRE(found);
        }
        taken.insert(site);
        const Row& sr = t.settlement[k];
        CHECK(at(sr, 2) == site.first);
        CHECK(at(sr, 3) == site.second);
        CHECK(at(sr, 4) == home);
        CHECK(at(sr, 9) == 1);    // hamlet
        CHECK(at(sr, 10) == 1);   // thriving
        CHECK(at(sr, 11) == 200);
        CHECK(at(t.civilization[k], 5) == at(sr, 0));  // capital
    }
    CHECK(static_cast<std::int64_t>(t.region_control.size()) == held_rows);
    CHECK(t.relation.empty());
    // names distinct from each other and from region, age and god names (registry, 7.2)
    std::set<std::string> names;
    std::size_t total = 0;
    for (const std::string tab : {"region", "age", "god"}) {
        for (const auto& row : rows_of(r, tab)) {
            names.insert(tab == "age" ? row[4].bytes : row[1].bytes);
            ++total;
        }
    }
    for (const Row& c : t.civilization) {
        names.insert(c[1].bytes);
        ++total;
    }
    for (const Row& c : t.settlement) {
        names.insert(c[1].bytes);
        ++total;
    }
    CHECK(names.size() == total);
    // invariant 2 and the event count of invariant 6
    std::int64_t expect_events = 3 * static_cast<std::int64_t>(np);
    for (const Row& ph : t.phase) {
        CHECK(at(ph, 2) != 3);
        if (at(ph, 2) == 2) {
            for (const Row& c : t.civilization) {
                expect_events += at(c, 6) < at(ph, 3) ? 1 : 0;
            }
        }
    }
    CHECK(static_cast<std::int64_t>(t.event.size()) == expect_events);
    for (std::size_t i = 0; i < t.event.size(); ++i) {
        CHECK(at(t.event[i], 0) == static_cast<std::int64_t>(i) + 1);
        if (i > 0) {
            CHECK(at(t.event[i], 1) >= at(t.event[i - 1], 1));
        }
        CHECK(at(t.event[i], 2) >= at(t.event[i], 1));
    }
    for (const Row& c : t.event_cause) {
        CHECK(at(c, 1) < at(c, 0));
    }
    // invariant 5: one census row per settlement, at its founding
    CHECK(t.settlement_census.size() == np);
    // every sentence renders
    CHECK(eng::render_all(r, static_cast<std::int64_t>(t.event.size())).size() == t.event.size());
}

// 16.2 item 16: hand-made tuning cases 0 ... count - 1 from one test-owned
// sequence; run_case selects which are run.
void hand_made_tuning(int count, const std::function<bool(int)>& run_case) {
    Mix m{20261119};
    for (int c = 0; c < count; ++c) {
        CAPTURE(c);
        EngineTuning t;
        t.quiet_step_years = m.in(1, 200);
        t.quiet_events_max = m.in(1, 100);
        t.simulated_max_permille = m.in(100, 500);
        t.simulated_min_permille = m.in(100, t.simulated_max_permille);
        t.islet_max_dry_cells = m.in(0, 1000000);
        for (Detail* d : {&t.sparse, &t.normal, &t.dense}) {
            const std::int64_t budget = 6 * t.simulated_max_permille - 30;
            d->active_years = m.in(1, std::max<std::int64_t>(1, (budget - 1) / 7));
            d->final_years = m.in(1, budget - 6 * d->active_years);
            d->flare_years = m.in(1, 1000);
            d->flare_threshold = m.in(1, 1000000);
        }
        const std::int64_t e = m.in(2000, 12000);
        const std::int64_t np = m.in(1, 6);
        std::vector<std::int64_t> y = {0};
        for (std::int64_t k = 1; k < np; ++k) {
            // packed close, and sometimes to the last years of the span
            const std::int64_t room = e - 1 - y.back() - (np - k - 1);
            // room >= 1 by construction; the packed branch is capped by it too (every start <= E - 1)
            y.push_back(y.back() + (m.next() % 4 == 0 ? m.in(1, room) : m.in(1, std::min<std::int64_t>(5, room))));
        }
        const std::string lv = std::vector<std::string>{"sparse", "normal", "dense"}[static_cast<std::size_t>(c % 3)];
        const wd::ContentSet content = with_engine(t);
        const std::int64_t period = c % 2 == 0 ? 0 : 25 * m.in(1, 7);
        if (!run_case(c)) {
            continue;  // the sequence is drawn for every case, so case numbers do not depend on the selection
        }
        Tables got;
        CHECK(fatal_of([&] { got = run(y, e, lv, period, content); }) == "<no error>");
        const Detail d = detail_of(t, lv);
        check_invariant_1(got, y, e, own_base(y, e, d, t));
        CHECK(got_text(got) == own_text(own_phases(y, e, d, t, period, 5000)));
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 12 invariant 1, 16.2 item 15: the scheduler over seed 0, five spans, three levels, a third of the subsets") {
    sweep(1);
}

TEST_CASE("P3-04 16.2 item 16: hand-made tuning and packed starts, cases 0-39 and the defect cases 90, 141, 163") {
    // 90 and 141: floor above the cap before the clamp (lead's ruling); 163: packed starts past E - 1.
    hand_made_tuning(200, [](int c) { return c < 40 || c == 90 || c == 141 || c == 163; });
}

TEST_CASE("P3-04 16.2 item 17: stub worlds on synthetic geography, seeds 0-15") {
    const std::vector<std::vector<std::string>> subsets = chrono_test::all_subsets();
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        const std::int64_t span = std::vector<std::int64_t>{2000, 6000, 12000, 2001}[seed % 4];
        const std::vector<std::string>& races = seed % 3 == 0 ? chrono_test::six() : subsets[(seed * 11) % 63];
        check_stub_world(seed, span, races, std::vector<std::string>{"sparse", "normal", "dense"}[seed % 3]);
    }
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-04 16.2 item 15: the scheduler sweep over seeds 0-99") { sweep(100); }

TEST_CASE("P3-04 16.2 item 16: hand-made tuning and packed starts, cases 0-999") {
    hand_made_tuning(1000, [](int) { return true; });
}

}  // TEST_SUITE
