#pragma once

// Test-owned helpers of the P3-05 peoples tests: the reader of
// worldgen/tests/golden/p3-05-peoples.txt (written by the independent
// reference reference/p3_peoples_reference.py), the synthetic geography of
// 16.2 item 17 built exactly as the reference's make_synthetic, the stress
// tuning, hand-made inputs, the peoples engine configuration with probe
// subsystems, the [history] lines of P3-05 15.4 from rows, and the
// invariants of 14 re-derived from rows. Library API of P3-05 only through
// p3_05_peoples_api.hpp.

#include "p3_05_peoples_api.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "p3_04_engine_support.hpp"

#ifndef UNMAPPED_P3_05_GOLDEN_PATH
#error "UNMAPPED_P3_05_GOLDEN_PATH must name worldgen/tests/golden/p3-05-peoples.txt"
#endif

namespace peoples_test {

using namespace engine_test;

// Event kind ids of P3-04 8.2 and P3-05 11.
enum Kind : std::int64_t {
    k_awakening = 1, k_civ_founded, k_settlement_founded, k_quiet_era, k_flare_up, k_colony_founded,
    k_became_village, k_became_town, k_became_city, k_settlement_dwindled, k_famine, k_plague, k_plague_spread,
    k_succession_crisis, k_civ_seceded, k_settlement_abandoned, k_capital_moved, k_civ_ended, k_ruin_faded
};
enum SKind : std::int64_t { hamlet = 1, village, town, city, fortress };
enum Status : std::int64_t { thriving = 1, declining, ruined, faded };

// --- Golden file ------------------------------------------------------------------------------------

struct GCase {
    std::string kind;  // world, synthetic, stress
    std::string name;
    std::uint64_t seed = 0;
    std::int64_t size = 0, span = 0;
    std::string races, detail = "normal", civs = "auto", intensity = "normal";
    std::string input_region;
    std::map<std::pair<std::int64_t, std::int64_t>, std::pair<std::int64_t, std::int64_t>> score;  // (R, r) -> (sigma, C)
    std::vector<std::string> homes;     // "ra<r> y<year> n<n> <list>"
    std::map<std::string, std::string> tables;
    std::vector<std::string> report;    // after "[history]"
    std::vector<std::string> stats;
    std::map<std::int64_t, std::string> sentences;
};

inline const std::vector<GCase>& peoples_golden() {
    static const std::vector<GCase> g = [] {
        std::vector<GCase> out;
        std::ifstream f(UNMAPPED_P3_05_GOLDEN_PATH, std::ios::binary);
        REQUIRE_MESSAGE(f.good(), "cannot read " << UNMAPPED_P3_05_GOLDEN_PATH);
        std::map<std::string, std::size_t> idx;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::vector<std::string> w = split(line, ' ');
            if (w[0] == "world" || w[0] == "synthetic" || w[0] == "stress") {
                GCase c;
                c.kind = w[0];
                c.name = w[1];
                c.seed = std::stoull(w[2]);
                if (w[0] == "world") {
                    c.size = std::stoll(w[3]);
                    c.span = std::stoll(w[4]);
                    c.races = w[5];
                    c.detail = w[6];
                } else {
                    c.size = 192;
                    c.span = std::stoll(w[3]);
                    c.races = w[4];
                    c.civs = w[5];
                    c.intensity = w[6];
                }
                idx[c.name] = out.size();
                out.push_back(c);
                continue;
            }
            const auto it = idx.find(w[0]);
            REQUIRE_MESSAGE(it != idx.end(), "golden line of no case: " << line);
            GCase& c = out[it->second];
            const std::string rest = line.substr(w[0].size() + 1);
            if (w[1] == "input_region") {
                c.input_region = w[2];
            } else if (w[1] == "score") {
                c.score[{std::stoll(w[2].substr(2)), std::stoll(w[3].substr(2))}] = {std::stoll(w[4]), std::stoll(w[5])};
            } else if (w[1] == "homes") {
                c.homes.push_back(rest.substr(6));
            } else if (w[1] == "table") {
                c.tables[w[2]] = w[3];
            } else if (w[1] == "report") {
                c.report.push_back(rest.substr(7));
            } else if (w[1] == "stats") {
                c.stats.push_back(rest.substr(6));
            } else if (w[1].size() > 1 && w[1][0] == 'e') {
                c.sentences[std::stoll(w[1].substr(1))] = rest.substr(w[1].size() + 1);
            } else {
                FAIL("unknown golden item: " << line);
            }
        }
        return out;
    }();
    return g;
}
// (a const char* argument: GCC's -Wdangling-reference warns on a class temporary)
inline const GCase& golden_case(const char* name) {
    for (const GCase& c : peoples_golden()) {
        if (c.name == name) {
            return c;
        }
    }
    FAIL("no golden case " << name);
    return peoples_golden().front();
}

// --- Content ----------------------------------------------------------------------------------------

// history.peoples with fields replaced (past the loader).
inline wd::ContentSet with_peoples(const std::map<std::string, wd::Value>& edits, const wd::ContentSet& base = shipped()) {
    for (const wd::KindContent& k : base.kinds) {
        if (k.kind != "history") {
            continue;
        }
        for (const wd::Definition& d : k.definitions) {
            if (d.id != "peoples") {
                continue;
            }
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const auto it = edits.find(d.record.name_at(i));
                r.add(d.record.name_at(i), it != edits.end() ? it->second : d.record.value_at(i));
            }
            return with_history_record(base, "peoples", std::move(r));
        }
    }
    FAIL("no history.peoples in the content");
    return base;
}
inline wd::Value by_kind(std::int64_t h, std::int64_t v, std::int64_t t, std::int64_t c, std::int64_t f) {
    wd::Record r;
    r.add("hamlet", wd::Value::integer(h));
    r.add("village", wd::Value::integer(v));
    r.add("town", wd::Value::integer(t));
    r.add("city", wd::Value::integer(c));
    r.add("fortress", wd::Value::integer(f));
    return wd::Value::record(std::move(r));
}
inline wd::Value I64(std::int64_t v) { return wd::Value::integer(v); }

// The reference's STRESS tuning (p3_peoples_reference.py; the golden header).
inline const wd::ContentSet& stress_content() {
    static const wd::ContentSet c = with_peoples({
        {"famine_ppm", I64(60000)},
        {"famine_crowding_permille", I64(0)},
        {"plague_ppm", by_kind(20000, 20000, 20000, 20000, 20000)},
        {"abandon_permille", by_kind(1000, 1000, 500, 200, 200)},
        {"abandon_frail_max", I64(100000)},
        {"ruin_fade_years", I64(200)},
        {"split_min_settlements", I64(2)},
        {"split_min_regions", I64(2)},
        {"split_permille", I64(1000)},
        {"crisis_per_generation_permille", I64(600)},
        {"found_rate_permille", I64(1000)},
    });
    return c;
}

// --- Geography (synthetic and hand-made) ------------------------------------------------------------

struct Geo {
    std::int64_t n = 0;
    std::vector<db::RegionRow> regions;
    std::vector<db::RegionBorderRow> borders;
    std::vector<db::RegionBiomeRow> region_biomes;
    std::vector<std::uint16_t> region;
    std::vector<std::uint8_t> water, hab, river, flow;
};

// splitmix64, the reference's Mix (test-owned, not the project's generator).
struct Mix {
    std::uint64_t s;
    std::uint64_t next() {
        s += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    std::int64_t in(std::int64_t lo, std::int64_t hi) {
        return lo + static_cast<std::int64_t>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

inline db::RegionRow plain_region(std::int64_t id, std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) {
    db::RegionRow r{};
    r.id = id;
    r.name = "Unit " + std::to_string(id);
    r.x_min = x0;
    r.y_min = y0;
    r.x_max = x1;
    r.y_max = y1;
    r.seed_x = x0 + 1;
    r.seed_y = y0 + 1;
    r.cells = 4000;
    r.walk_cells = 3000;
    r.habitability_mean = 50;
    r.fertility_mean = 100;
    r.forage_mean = 100;
    r.timber_mean = 100;
    r.fresh_water_mean = 100;
    return r;
}

// make_synthetic(seed) of the reference, statement for statement.
inline Geo make_synthetic(std::uint64_t seed) {
    Mix m{0x5EED0000ULL + seed};
    Geo g;
    const std::int64_t n = 192;
    const std::int64_t ocean = 8;
    g.n = n;
    const std::int64_t gx = m.in(1, 8);
    std::int64_t gy = gx == 1 ? m.in(2, 8) : m.in(1, 8);
    if (gx * gy > 60) {
        gy = 60 / gx;
    }
    std::vector<std::int64_t> xs = {ocean};
    for (std::int64_t i = 1; i < gx; ++i) {
        xs.push_back(ocean + (n - ocean) * i / gx + m.in(0, 3));
    }
    xs.push_back(n);
    std::vector<std::int64_t> ys = {0};
    for (std::int64_t j = 1; j < gy; ++j) {
        ys.push_back(n * j / gy + m.in(0, 3));
    }
    ys.push_back(n);
    const auto N = static_cast<std::size_t>(n * n);
    g.region.assign(N, 0);
    g.water.assign(N, 0);
    g.hab.assign(N, 0);
    g.river.assign(N, 0);
    g.flow.assign(N, 8);
    auto at_ = [=](std::int64_t x, std::int64_t y) { return static_cast<std::size_t>(y * n + x); };
    for (std::int64_t y = 0; y < n; ++y) {
        for (std::int64_t x = 0; x < ocean; ++x) {
            g.water[at_(x, y)] = 3;
        }
    }
    std::vector<db::RegionBiomeRow> rb;
    std::int64_t rid = 0;
    const std::int64_t land_biomes[9] = {3, 4, 5, 6, 7, 9, 10, 11, 12};
    for (std::int64_t j = 0; j < gy; ++j) {
        for (std::int64_t i = 0; i < gx; ++i) {
            ++rid;
            const std::int64_t x0 = xs[static_cast<std::size_t>(i)], x1 = xs[static_cast<std::size_t>(i + 1)] - 1;
            const std::int64_t y0 = ys[static_cast<std::size_t>(j)], y1 = ys[static_cast<std::size_t>(j + 1)] - 1;
            const std::int64_t mode = m.in(0, 9);
            std::int64_t cells = 0, lake = 0;
            for (std::int64_t y = y0; y <= y1; ++y) {
                for (std::int64_t x = x0; x <= x1; ++x) {
                    const std::size_t c = at_(x, y);
                    g.region[c] = static_cast<std::uint16_t>(rid);
                    ++cells;
                    const bool is_lake = (mode == 0 && (x - x0) > 2) || m.next() % 97 == 0;
                    if (is_lake && !(x == x0 && y == y0)) {
                        g.water[c] = 2;
                        ++lake;
                    }
                    g.hab[c] = static_cast<std::uint8_t>(m.in(0, 120) + ((x + y) % 7 == 0 ? 60 : 0));
                }
            }
            const std::int64_t dry = cells - lake;
            const std::int64_t walk = mode == 1 ? 0 : m.in(dry / 5, dry);
            if (i == 0 && mode != 0) {
                const std::int64_t ry = (y0 + y1) / 2;
                for (std::int64_t x = x0; x <= x1; ++x) {
                    const std::size_t c = at_(x, ry);
                    if (g.water[c] == 0) {
                        g.water[c] = 1;
                        g.river[c] = static_cast<std::uint8_t>(m.in(3, 6));
                        g.flow[c] = 4;
                    }
                }
            }
            db::RegionRow r{};
            r.id = rid;
            r.name = "Synthetic " + std::to_string(rid);
            r.x_min = x0;
            r.y_min = y0;
            r.x_max = x1;
            r.y_max = y1;
            r.seed_x = x0 + 1;
            r.seed_y = y0 + 1;
            r.cells = cells;
            r.lake_cells = lake;
            r.walk_cells = walk;
            r.habitability_mean = m.in(10, 120);
            r.coast_cells = i == 0 ? (y1 - y0 + 1) : 0;
            r.fertility_mean = m.in(0, 255);
            r.forage_mean = m.in(0, 255);
            r.timber_mean = m.in(0, 255);
            r.fresh_water_mean = m.in(0, 255);
            r.ore_reach_cells = m.in(0, 400);
            g.regions.push_back(r);
            std::int64_t left = dry;
            std::set<std::int64_t> used;
            for (int k = 0; k < 3; ++k) {
                const std::int64_t b = land_biomes[m.in(0, 8)];
                if (used.count(b) != 0) {
                    continue;
                }
                used.insert(b);
                const std::int64_t cnt = m.in(0, left);
                left -= cnt;
                rb.push_back({rid, b, cnt});
            }
            if (left > 0) {
                std::int64_t b = used.count(4) == 0 ? 4 : 3;
                if (used.count(b) != 0) {
                    b = 12;
                    if (used.count(b) != 0) {
                        b = 0;
                    }
                }
                if (b != 0) {
                    rb.push_back({rid, b, left});
                    used.insert(b);
                }
            }
            if (lake > 0) {
                rb.push_back({rid, 2, lake});
            }
        }
    }
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> agg;
    for (const auto& r : rb) {
        agg[{r.region, r.biome}] += r.cells;
    }
    for (const auto& [k, c] : agg) {
        g.region_biomes.push_back({k.first, k.second, c});
    }
    for (std::int64_t j = 0; j < gy; ++j) {
        for (std::int64_t i = 0; i < gx; ++i) {
            const std::int64_t a = j * gx + i + 1;
            for (const auto& [di, dj] : {std::pair<std::int64_t, std::int64_t>{1, 0}, {0, 1}}) {
                if (i + di >= gx || j + dj >= gy) {
                    continue;
                }
                const std::int64_t b = (j + dj) * gx + (i + di) + 1;
                const std::int64_t kind = m.in(0, 9);
                const std::int64_t length =
                    16 * (di != 0 ? ys[static_cast<std::size_t>(j + 1)] - ys[static_cast<std::size_t>(j)]
                                  : xs[static_cast<std::size_t>(i + 1)] - xs[static_cast<std::size_t>(i)]);
                const std::int64_t walk_pairs = kind <= 5 ? m.in(1, 50) : 0;
                const std::int64_t pass_id = (kind == 6 || kind == 7) ? m.in(1, 9) : 0;
                const std::int64_t river_pairs =
                    (kind == 3 || kind == 7) ? length / 16 / 2 : (kind == 4 ? length / 32 - 1 : 0);
                db::RegionBorderRow br{};
                br.a = std::min(a, b);
                br.b = std::max(a, b);
                br.length_m = kind == 5 ? 0 : length;
                br.walk_pairs = walk_pairs;
                br.river_pairs = std::max<std::int64_t>(0, river_pairs);
                br.pass_id = pass_id;
                g.borders.push_back(br);
            }
        }
    }
    std::sort(g.borders.begin(), g.borders.end(),
              [](const auto& x, const auto& y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
    return g;
}

// A hand-made geography: N x N, every cell of the given rectangles' regions
// (later regions win), water 0, habitability 0, no river.
inline Geo hand_made(std::int64_t n, std::vector<db::RegionRow> regions) {
    Geo g;
    g.n = n;
    const auto N = static_cast<std::size_t>(n * n);
    g.region.assign(N, 0);
    g.water.assign(N, 0);
    g.hab.assign(N, 0);
    g.river.assign(N, 0);
    g.flow.assign(N, 8);
    for (const auto& r : regions) {
        for (std::int64_t y = r.y_min; y <= r.y_max; ++y) {
            for (std::int64_t x = r.x_min; x <= r.x_max; ++x) {
                g.region[static_cast<std::size_t>(y * n + x)] = static_cast<std::uint16_t>(r.id);
            }
        }
    }
    g.regions = std::move(regions);
    return g;
}

inline std::vector<db::BiomeRow> biome_rows() {
    const char* keys[12] = {"ocean", "lake",  "temperate_forest", "grassland", "marsh",        "highland_moor",
                            "alpine", "blighted_waste", "heath", "steppe", "wet_woodland", "fen"};
    std::vector<db::BiomeRow> v;
    for (std::int64_t i = 0; i < 12; ++i) {
        v.push_back(db::BiomeRow{i + 1, keys[i], keys[i], 0, i < 2 ? 1 : 0});
    }
    return v;
}

template <class T>
db::LayerGrid grid_of(db::ElementType type, std::int64_t n, const std::vector<T>& values) {
    db::LayerGrid g = db::make_layer_grid(type, static_cast<std::uint32_t>(n), static_cast<std::uint32_t>(n));
    g.values = values;
    return g;
}

struct Settings {
    std::int64_t span = 6000;
    std::vector<std::string> races = chrono_test::six();
    std::optional<std::int64_t> civs;  // nullopt: auto
    std::string intensity = "normal";
    std::string detail = "normal";
    std::vector<std::int64_t> starts;  // empty: P3-03's years
};
inline Settings settings_of(const GCase& c) {
    Settings s;
    s.span = c.span;
    s.races = chrono_test::races_of_text(c.races);
    s.civs = c.civs == "auto" ? std::nullopt : std::optional<std::int64_t>(std::stoll(c.civs));
    s.intensity = c.intensity;
    s.detail = c.detail;
    return s;
}

// HistoryInputs of a geography: chronology rows of `content` for the
// settings at `seed` (P3-03 with the regions' names), layers of the geography.
inline eng::HistoryInputs inputs_of(const Geo& g, std::uint64_t seed, const Settings& s,
                                    const wd::ContentSet& content = shipped()) {
    chrono_api::Params p = chrono_test::params_with(s.span, s.races);
    p.civs = s.civs;
    p.intensity = s.intensity;
    p.detail = s.detail;
    std::vector<std::string> names;
    for (const auto& r : g.regions) {
        names.push_back(r.name);
    }
    const chrono_api::Rows c = chrono_api::stage(content, p, seed, names);
    eng::HistoryInputs in;
    in.seed = seed;
    in.map_size = g.n;
    in.params = chrono_api::to_lib(p);
    in.regions = g.regions;
    in.region_borders = g.borders;
    in.region_biomes = g.region_biomes;
    std::size_t k = 0;
    for (const auto& r : c.races) {
        db::RaceRow row{};
        row.id = r.id;
        row.key = r.key;
        row.name = r.name;
        row.plural = r.plural;
        row.adjective = r.adjective;
        row.camp = r.camp;
        row.lifespan_years = r.lifespan_years;
        row.generation_years = r.generation_years;
        row.culture = r.culture;
        row.present = r.present;
        row.awakening_year = r.awakening_year;
        if (r.present && !s.starts.empty()) {
            row.awakening_year = s.starts.at(k++);
        }
        row.creator_god = r.creator_god;
        in.races.push_back(row);
    }
    for (const auto& r : c.race_biomes) {
        in.race_biomes.push_back(db::RaceBiomeRow{r.race, r.biome, r.weight});
    }
    for (const auto& r : c.cultures) {
        in.cultures.push_back(db::CultureRow{r.id, r.key, r.name, r.race});
    }
    for (const auto& r : c.gods) {
        in.gods.push_back(db::GodRow{r.id, r.name, r.camp});
    }
    for (std::size_t i = 0; i < c.ages.size(); ++i) {
        db::AgeRow a{c.ages[i].id, c.ages[i].race, c.ages[i].start_year, c.ages[i].end_year, c.ages[i].name};
        if (!s.starts.empty()) {
            a.start_year = s.starts[i];
            a.end_year = i + 1 < s.starts.size() ? s.starts[i + 1] - 1 : s.span - 1;
        }
        in.ages.push_back(a);
    }
    in.region_layer = grid_of(db::ElementType::u16, g.n, g.region);
    in.water_layer = grid_of(db::ElementType::u8, g.n, g.water);
    ppl::set_peoples_inputs(in, biome_rows(), grid_of(db::ElementType::u8, g.n, g.hab),
                            grid_of(db::ElementType::u8, g.n, g.river), grid_of(db::ElementType::u8, g.n, g.flow));
    return in;
}

inline eng::HistoryInputs synthetic_inputs(const GCase& c) {
    return inputs_of(make_synthetic(c.seed), c.seed, settings_of(c), c.kind == "stress" ? stress_content() : shipped());
}
inline const wd::ContentSet& content_of(const GCase& c) { return c.kind == "stress" ? stress_content() : shipped(); }

// --- The peoples engine configuration with probes ---------------------------------------------------

struct PeoplesConfig {
    ppl::Peoples peoples;
    std::vector<std::unique_ptr<FnSubsystem>> before, after;  // run before / after peoples
    std::unique_ptr<eng::InterventionSource> interventions = std::make_unique<eng::EmptyInterventionSource>();
    eng::EventCatalogue catalogue;

    explicit PeoplesConfig(const wd::ContentSet& c, bool test_kind = false)
        : catalogue(test_kind ? test_catalogue_of(c) : eng::catalogue_of(c)) {}
    static eng::EventCatalogue test_catalogue_of(const wd::ContentSet& c) {
        eng::EventCatalogue cat = eng::catalogue_of(c);
        eng::KindDef k;
        k.id = static_cast<std::int64_t>(cat.kinds.size()) + 1;
        k.key = "test_change";
        k.caused = false;
        k.region_use = 2;
        k.settlement_use = 2;
        k.roles = {eng::RoleDef{1, "actor", eng::EntityType::civilization, 0, 0}};
        k.salience = 10;
        k.templ = "{actor:all} changed.";
        cat.kinds.push_back(k);
        return cat;
    }
    void add_before(std::vector<eng::Step> steps, FnSubsystem::Fn fn) {
        before.push_back(std::make_unique<FnSubsystem>(std::move(steps), std::move(fn)));
    }
    void add_after(std::vector<eng::Step> steps, FnSubsystem::Fn fn) {
        after.push_back(std::make_unique<FnSubsystem>(std::move(steps), std::move(fn)));
    }
    [[nodiscard]] eng::EngineConfig get() const {
        eng::EngineConfig c{};
        c.awakening = peoples.awakening.get();
        for (const auto& s : before) {
            c.subsystems.push_back(s.get());
        }
        c.subsystems.push_back(peoples.sub.get());
        for (const auto& s : after) {
            c.subsystems.push_back(s.get());
        }
        c.interventions = interventions.get();
        c.catalogue = &catalogue;
        return c;
    }
    [[nodiscard]] const ppl::PeoplesView& view() const { return peoples.sub->view(); }
};

// --- Lines from rows ---------------------------------------------------------------------------------

inline bool is_null(const Cell& c) { return c.type == db::ValueType::null; }

// P3-04 11.1 with P3-05 15.4 appended.
inline std::vector<std::string> report_lines_05(const Tables& t, const eng::HistoryInputs& in) {
    std::vector<std::string> o = report_lines(t, in.params.span);
    if (o.empty()) {
        return o;
    }
    std::map<std::int64_t, std::int64_t> civ_race;
    for (const Row& c : t.civilization) {
        civ_race[at(c, 0)] = at(c, 2);
    }
    auto standing = [](const Row& s) { return at(s, 10) == 1 || at(s, 10) == 2; };
    std::int64_t pop = 0;
    for (const Row& s : t.settlement) {
        pop += standing(s) ? at(s, 11) : 0;
    }
    o.push_back("population = " + std::to_string(pop));
    for (const char* what : {"population_", "settlements_standing_", "regions_held_"}) {
        for (const db::RaceRow& r : in.races) {
            std::int64_t v = 0;
            if (std::string(what) == "regions_held_") {
                for (const Row& rc : t.region_control) {
                    v += (is_null(rc[2]) && civ_race[at(rc, 3)] == r.id) ? 1 : 0;
                }
            } else {
                for (const Row& s : t.settlement) {
                    if (standing(s) && civ_race[at(s, 8)] == r.id) {
                        v += std::string(what) == "population_" ? at(s, 11) : 1;
                    }
                }
            }
            o.push_back(std::string(what) + r.key + " = " + std::to_string(v));
        }
    }
    return o;
}

inline std::vector<std::string> stats_lines(const Tables& t) {
    std::map<std::int64_t, std::int64_t> kind;
    for (const Row& e : t.event) {
        kind[at(e, 0)] = at(e, 3);
    }
    std::int64_t mx = 0, colonies = 0, seceded = 0;
    for (const Row& c : t.civilization) {
        if (is_null(c[8])) {
            std::int64_t n = 0;
            for (const Row& s : t.settlement) {
                n += (at(s, 8) == at(c, 0) && (at(s, 10) == 1 || at(s, 10) == 2)) ? 1 : 0;
            }
            mx = std::max(mx, n);
        }
        seceded += kind[at(c, 7)] == k_civ_seceded ? 1 : 0;
    }
    for (const Row& s : t.settlement) {
        colonies += kind[at(s, 7)] == k_colony_founded ? 1 : 0;
    }
    return {"civ_settlements_max = " + std::to_string(mx), "settlements_colonies = " + std::to_string(colonies),
            "civilizations_seceded = " + std::to_string(seceded)};
}

// "ra<r> y<year> <homes>" per awakening from rows (the golden's homes lines
// without the requested count n).
inline std::vector<std::string> homes_from_rows(const Tables& t) {
    std::map<std::int64_t, std::vector<std::int64_t>> cause_of;
    for (const Row& c : t.event_cause) {
        cause_of[at(c, 0)].push_back(at(c, 1));
    }
    std::vector<std::string> out;
    for (const Row& e : t.event) {
        if (at(e, 3) != k_awakening) {
            continue;
        }
        std::int64_t race = 0;
        for (const Row& p : t.event_participant) {
            if (at(p, 0) == at(e, 0) && at(p, 2) == 1) {
                race = at(p, 4);
            }
        }
        std::string homes;
        for (const Row& c : t.civilization) {
            const auto it = cause_of.find(at(c, 7));
            if (it != cause_of.end() && it->second == std::vector<std::int64_t>{at(e, 0)}) {
                homes += (homes.empty() ? "" : ",") + std::to_string(at(c, 4));
            }
        }
        out.push_back("ra" + std::to_string(race) + " y" + std::to_string(at(e, 1)) + " " + homes);
    }
    return out;
}
inline std::vector<std::string> homes_without_n(const GCase& c) {
    std::vector<std::string> out;
    for (const std::string& h : c.homes) {
        const std::vector<std::string> w = split(h, ' ');
        out.push_back(w[0] + " " + w[1] + " " + w[3]);
    }
    return out;
}

// --- Invariants of 14 from rows (items 1-8; 9 and 10 are tests of their own) --------------------------

inline void check_invariants(const Tables& t, const eng::HistoryInputs& in, const GCase& g, std::int64_t fade_years,
                             std::int64_t abandon_min) {
    (void)abandon_min;
    std::map<std::int64_t, const Row*> ev, st, cv, ph;
    for (const Row& r : t.event) ev[at(r, 0)] = &r;
    for (const Row& r : t.settlement) st[at(r, 0)] = &r;
    for (const Row& r : t.civilization) cv[at(r, 0)] = &r;
    for (const Row& r : t.phase) ph[at(r, 0)] = &r;
    std::map<std::int64_t, std::vector<std::int64_t>> causes;
    for (const Row& c : t.event_cause) causes[at(c, 0)].push_back(at(c, 1));
    auto kind = [&](std::int64_t e) { return at(*ev.at(e), 3); };
    // 1
    for (const db::AgeRow& a : in.ages) {
        std::int64_t n = 0;
        for (const Row& c : t.civilization) {
            if (at(c, 2) == a.race && kind(at(c, 7)) == k_civ_founded) {
                ++n;
                CHECK(at(c, 6) == a.start_year);
                CHECK(kind(causes[at(c, 7)].at(0)) == k_awakening);
            }
        }
        CHECK(n >= 1);
        CHECK(n <= (in.params.civs_per_race.has_value() ? *in.params.civs_per_race : 2));
    }
    // 2
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> cells;
    std::map<std::int64_t, std::int64_t> faded_by;  // settlement -> its ruin_faded event
    for (const Row& e : t.event) {
        if (at(e, 3) == k_ruin_faded) faded_by[at(e, 6)] = at(e, 0);
    }
    const auto& reg = std::get<std::vector<std::uint16_t>>(in.region_layer.values);
    const auto& wat = std::get<std::vector<std::uint8_t>>(in.water_layer.values);
    for (const Row& s : t.settlement) {
        CAPTURE(at(s, 0));
        CHECK((kind(at(s, 7)) == k_settlement_founded || kind(at(s, 7)) == k_colony_founded));
        const auto c = static_cast<std::size_t>(at(s, 3) * in.map_size + at(s, 2));
        CHECK(reg.at(c) == at(s, 4));
        CHECK(wat.at(c) == 0);
        const auto key = std::make_pair(at(s, 2), at(s, 3));
        if (const auto it = cells.find(key); it != cells.end()) {
            // a faded settlement does not block (6.2): see the reference's SPEC_GAPS
            CHECK(faded_by.count(it->second) == 1U);
            if (faded_by.count(it->second) == 1U) CHECK(faded_by[it->second] < at(s, 7));
        }
        cells[key] = at(s, 0);
    }
    // 3: P <= 2 C (the reference's C, golden score lines)
    for (const Row& s : t.settlement) {
        if (at(s, 10) == 1 || at(s, 10) == 2) {
            const std::int64_t race = at(*cv.at(at(s, 8)), 2);
            CHECK(at(s, 11) <= 2 * g.score.at({at(s, 4), race}).second);
        }
    }
    // 4
    for (const Row& rc : t.region_control) {
        if (!is_null(rc[2])) continue;
        bool ok = false;
        for (const Row& s : t.settlement) {
            ok = ok || (at(s, 8) == at(rc, 3) && at(s, 4) == at(rc, 0) && (at(s, 10) == 1 || at(s, 10) == 2));
        }
        CHECK_MESSAGE(ok, "region " << at(rc, 0) << " held by " << at(rc, 3) << " without a settlement");
    }
    // 5
    for (const Row& c : t.civilization) {
        CAPTURE(at(c, 0));
        if (is_null(c[8])) {
            REQUIRE(st.count(at(c, 5)) == 1U);
            CHECK(at(*st.at(at(c, 5)), 8) == at(c, 0));
            CHECK((at(*st.at(at(c, 5)), 10) == 1 || at(*st.at(at(c, 5)), 10) == 2));
        } else {
            for (const Row& s : t.settlement) {
                CHECK_FALSE((at(s, 8) == at(c, 0) && (at(s, 10) == 1 || at(s, 10) == 2)));
            }
            for (const Row& rc : t.region_control) {
                CHECK_FALSE((is_null(rc[2]) && at(rc, 3) == at(c, 0)));
            }
        }
    }
    // 6: a fortress stays a fortress (census)
    std::map<std::int64_t, std::vector<std::int64_t>> kinds;
    for (const Row& r : t.settlement_census) kinds[at(r, 0)].push_back(at(r, 3));
    for (const auto& [s, ks] : kinds) {
        if (ks.front() == fortress) {
            CHECK(std::all_of(ks.begin(), ks.end(), [](std::int64_t k) { return k == fortress; }));
        }
    }
    // 7
    for (const Row& s : t.settlement) {
        if (at(s, 10) == ruined) {
            CHECK(kind(at(s, 13)) == k_settlement_abandoned);
        }
        if (at(s, 10) == faded) {
            REQUIRE(faded_by.count(at(s, 0)) == 1U);
            // lead's ruling 3: fading clears ruined_year and ruin_event; the ruin_faded event's cause is the
            // settlement's ruin event, dated at least ruin_fade_years before it
            const std::int64_t f = faded_by[at(s, 0)];
            CHECK(is_null(s[12]));
            CHECK(at(s, 13) == 0);
            REQUIRE(causes[f].size() == 1U);
            const Row& ruin = *ev.at(causes[f][0]);
            CHECK(at(ruin, 3) == k_settlement_abandoned);
            CHECK(at(ruin, 6) == at(s, 0));
            CHECK(at(*ev.at(f), 1) >= at(ruin, 1) + fade_years);
        }
    }
    // 8: no event of 11 except kinds 6-10, 16-19 in a quiet phase; causes of the caused kinds
    for (const Row& e : t.event) {
        const std::int64_t k = at(e, 3);
        if (k >= 6 && at(*ph.at(at(e, 4)), 2) == 2) {
            CHECK_MESSAGE(!(k >= k_famine && k <= k_civ_seceded), "event " << at(e, 0) << " kind " << k << " in a quiet step");
        }
        const std::vector<std::int64_t>& cs = causes[at(e, 0)];
        if (k == k_became_village || k == k_became_town || k == k_became_city) {
            CHECK(cs == std::vector<std::int64_t>{at(*st.at(at(e, 6)), 7)});
        } else if (k == k_plague_spread) {
            CHECK((cs.size() == 1U && kind(cs[0]) == k_plague));
        } else if (k == k_civ_seceded) {
            CHECK((cs.size() == 1U && kind(cs[0]) == k_succession_crisis));
        } else if (k == k_capital_moved || k == k_civ_ended || k == k_ruin_faded) {
            CHECK((cs.size() == 1U && kind(cs[0]) == k_settlement_abandoned));
        } else if (k == k_settlement_abandoned) {
            REQUIRE(cs.size() == 1U);
            const std::int64_t ck = kind(cs[0]);
            CHECK((ck == k_famine || ck == k_plague || ck == k_plague_spread || ck == k_succession_crisis ||
                   ck == k_settlement_founded || ck == k_colony_founded));
        }
    }
}

}  // namespace peoples_test

#endif  // UNMAPPED_HAVE_PEOPLES
