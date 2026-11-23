// The geography of the peoples (spec P3-05 sections 4 to 6 and 12.1): the
// tuning, races and regions, scores, capacities, borders, mouths and sites.

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/history/peoples.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

constexpr std::int64_t WATER_LAND = 0;   // layer water (P1-02): 0 land, 1 river, 2 lake, 3 ocean
constexpr std::int64_t WATER_RIVER = 1;
constexpr std::int64_t WATER_OCEAN = 3;
constexpr std::int64_t MAX_CAPACITY = 2147483647;

PeoplesByKind by_kind(const data::Record& r) {
    return PeoplesByKind{r.get_int("hamlet"), r.get_int("village"), r.get_int("town"), r.get_int("city"),
                         r.get_int("fortress")};
}

// The cells of a layer of N x N of its catalogue type (P3-05 16.0a, r7:
// the message names that type), widened.
class Cells {
public:
    Cells(const db::LayerGrid& g, std::string_view name, std::int64_t n, db::ElementType type) {
        const auto side = static_cast<std::uint32_t>(n);
        if (g.width == side && g.height == side && g.type() == type) {
            u8_ = g.get_if<std::uint8_t>();
            u16_ = g.get_if<std::uint16_t>();
            i16_ = g.get_if<std::int16_t>();
        }
        if (u8_ == nullptr && u16_ == nullptr && i16_ == nullptr) {
            fail("history: peoples: layer '" + std::string(name) + "' is not " + int_text(n) + " x " + int_text(n) +
                 " " + type_text(type));
        }
    }
    [[nodiscard]] std::int64_t at(std::size_t i) const {
        if (u8_ != nullptr) {
            return static_cast<std::int64_t>((*u8_)[i]);
        }
        if (u16_ != nullptr) {
            return static_cast<std::int64_t>((*u16_)[i]);
        }
        return static_cast<std::int64_t>((*i16_)[i]);
    }

private:
    static std::string type_text(db::ElementType type) {
        switch (type) {
            case db::ElementType::u8: return "u8";
            case db::ElementType::u16: return "u16";
            case db::ElementType::i16: return "i16";
            default: break;
        }
        return "of its catalogue type";
    }
    const std::vector<std::uint8_t>* u8_ = nullptr;
    const std::vector<std::uint16_t>* u16_ = nullptr;
    const std::vector<std::int16_t>* i16_ = nullptr;
};

// Chebyshev dilation by `radius` inside the grid: out(c) = 1 if a set cell
// lies within distance radius of c (P3-05 6.1).
std::vector<std::uint8_t> dilate(const std::vector<std::uint8_t>& in, std::int64_t n, std::int64_t radius) {
    const auto un = static_cast<std::size_t>(n);
    std::vector<std::uint8_t> rows(in.size(), 0);
    std::vector<std::int64_t> prefix(un + 1, 0);
    for (std::size_t y = 0; y < un; ++y) {
        for (std::size_t x = 0; x < un; ++x) {
            prefix[x + 1] = prefix[x] + static_cast<std::int64_t>(in[y * un + x] != 0);
        }
        for (std::int64_t x = 0; x < n; ++x) {
            const std::int64_t lo = std::max<std::int64_t>(0, x - radius);
            const std::int64_t hi = std::min<std::int64_t>(n - 1, x + radius);
            rows[y * un + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(prefix[static_cast<std::size_t>(hi + 1)] > prefix[static_cast<std::size_t>(lo)]);
        }
    }
    std::vector<std::uint8_t> out(in.size(), 0);
    for (std::size_t x = 0; x < un; ++x) {
        for (std::size_t y = 0; y < un; ++y) {
            prefix[y + 1] = prefix[y] + static_cast<std::int64_t>(rows[y * un + x] != 0);
        }
        for (std::int64_t y = 0; y < n; ++y) {
            const std::int64_t lo = std::max<std::int64_t>(0, y - radius);
            const std::int64_t hi = std::min<std::int64_t>(n - 1, y + radius);
            out[static_cast<std::size_t>(y) * un + x] =
                static_cast<std::uint8_t>(prefix[static_cast<std::size_t>(hi + 1)] > prefix[static_cast<std::size_t>(lo)]);
        }
    }
    return out;
}

SettlementKind kind_of_text(std::string_view text) {
    static constexpr std::string_view KINDS[] = {"hamlet", "village", "town", "city", "fortress"};
    for (std::size_t i = 0; i < 5; ++i) {
        if (KINDS[i] == text) {
            return static_cast<SettlementKind>(static_cast<std::int64_t>(i) + 1);
        }
    }
    fail("history: peoples: '" + std::string(text) + "' is not a settlement kind");
}

constexpr SettlementKind LADDER[] = {SettlementKind::hamlet, SettlementKind::village, SettlementKind::town,
                                     SettlementKind::city};

}  // namespace

// --- Tuning ---------------------------------------------------------------------------------

std::int64_t PeoplesByKind::of(SettlementKind kind) const noexcept {
    switch (kind) {
        case SettlementKind::hamlet: return hamlet;
        case SettlementKind::village: return village;
        case SettlementKind::town: return town;
        case SettlementKind::city: return city;
        case SettlementKind::fortress: return fortress;
    }
    return 0;
}

PeoplesTuning peoples_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_PEOPLES_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.peoples");
    }
    const data::Record& r = d->record;
    PeoplesTuning t;
    t.score_site = r.get_int("score_site");
    t.score_biome = r.get_int("score_biome");
    t.score_upland = r.get_int("score_upland");
    t.score_coast = r.get_int("score_coast");
    t.ore_reach_full = r.get_int("ore_reach_full");
    t.coast_full = r.get_int("coast_full");
    t.rough_base = r.get_int("rough_base");
    t.rough_upland = r.get_int("rough_upland");
    t.people_per_kcell = r.get_int("people_per_kcell");
    t.capacity_floor = r.get_int("capacity_floor");
    t.capital_weight = r.get_int("capital_weight");
    t.founding_population = r.get_int("founding_population");
    t.village_min = r.get_int("village_min");
    t.town_min = r.get_int("town_min");
    t.city_min = r.get_int("city_min");
    t.city_max = r.get_int("city_max");
    t.fortress_max = r.get_int("fortress_max");
    t.kind_shrink_permille = r.get_int("kind_shrink_permille");
    t.site_spacing = r.get_int("site_spacing");
    t.region_slots_max = r.get_int("region_slots_max");
    t.mouth_class_min = r.get_int("mouth_class_min");
    t.port_radius = r.get_int("port_radius");
    t.port_mouth_bonus = r.get_int("port_mouth_bonus");
    t.port_coast_bonus = r.get_int("port_coast_bonus");
    t.decline_permille = r.get_int("decline_permille");
    t.recover_permille = r.get_int("recover_permille");
    t.colonist_permille = r.get_int("colonist_permille");
    t.colonists_min = r.get_int("colonists_min");
    t.abandon_min = r.get_int("abandon_min");
    t.abandon_frail_max = r.get_int("abandon_frail_max");
    t.abandon_permille = by_kind(r.get_record("abandon_permille"));
    t.found_pressure_permille = r.get_int("found_pressure_permille");
    t.found_rate_permille = r.get_int("found_rate_permille");
    t.quiet_found_permille = r.get_int("quiet_found_permille");
    t.reach_base = r.get_int("reach_base");
    t.reach_per_expansion = r.get_int("reach_per_expansion");
    t.expand_min_score = r.get_int("expand_min_score");
    t.expand_relative_permille = r.get_int("expand_relative_permille");
    t.expand_min_capacity = r.get_int("expand_min_capacity");
    t.cost_penalty = r.get_int("cost_penalty");
    t.pass_cost = r.get_int("pass_cost");
    t.river_cost = r.get_int("river_cost");
    t.river_border_share_permille = r.get_int("river_border_share_permille");
    t.civs_two_permille = r.get_int("civs_two_permille");
    t.regions_per_civ = r.get_int("regions_per_civ");
    t.regions_per_extra_civ = r.get_int("regions_per_extra_civ");  // r8 (generator 0.18.0)
    t.civs_extra_max = r.get_int("civs_extra_max");
    t.cause_memory_years = r.get_int("cause_memory_years");  // r12 22.2 (generator 0.19.0)
    t.inland_cap = r.get_int("inland_cap");                  // r12 22.5
    t.home_min_hops = r.get_int("home_min_hops");
    t.home_min_capacity = r.get_int("home_min_capacity");
    const data::Record& intensity = r.get_record("intensity");
    t.intensity_calm = intensity.get_int("calm");
    t.intensity_normal = intensity.get_int("normal");
    t.intensity_turbulent = intensity.get_int("turbulent");
    t.famine_crowding_permille = r.get_int("famine_crowding_permille");
    t.famine_ppm = r.get_int("famine_ppm");
    t.famine_toll_min = r.get_int("famine_toll_min");
    t.famine_toll_max = r.get_int("famine_toll_max");
    t.plague_ppm = by_kind(r.get_record("plague_ppm"));
    t.plague_toll_min = r.get_int("plague_toll_min");
    t.plague_toll_max = r.get_int("plague_toll_max");
    t.plague_spread_permille = r.get_int("plague_spread_permille");
    t.plague_spread_toll_permille = r.get_int("plague_spread_toll_permille");
    t.crisis_per_generation_permille = r.get_int("crisis_per_generation_permille");
    t.crisis_loss_permille = r.get_int("crisis_loss_permille");
    t.split_min_settlements = r.get_int("split_min_settlements");
    t.split_min_regions = r.get_int("split_min_regions");
    t.split_permille = r.get_int("split_permille");
    t.ruin_fade_years = r.get_int("ruin_fade_years");
    t.colony_new_region_salience = r.get_int("colony_new_region_salience");
    t.plague_small_salience = r.get_int("plague_small_salience");
    t.plague_major_dead = r.get_int("plague_major_dead");
    t.crisis_small_salience = r.get_int("crisis_small_salience");
    t.ghost_permille = r.get_int("ghost_permille");  // P5-02 4.5
    t.ghost_max_population = r.get_int("ghost_max_population");
    return t;
}

// --- Races ---------------------------------------------------------------------------------

bool PeoplesRace::builds_kind(SettlementKind kind) const noexcept {
    return builds[static_cast<std::size_t>(kind)] != 0;
}

bool PeoplesRace::has_cell_term() const noexcept {
    // P3-01 r11 3.4: the record differs from the neutral one.
    return cell_ore != 0 || cell_height != 0 || cell_inland != 0 || cell_port != 1000 || cell_habitability != 1000;
}

bool PeoplesRace::has_ladder() const noexcept {
    for (const SettlementKind k : LADDER) {
        if (builds_kind(k)) {
            return true;
        }
    }
    return false;
}

PeoplesRace peoples_race(const db::RaceRow& row, std::span<const db::RaceBiomeRow> race_biomes,
                         std::span<const db::CultureRow> cultures, const data::ContentSet& content) {
    PeoplesRace r;
    r.id = row.id;
    r.key = row.key;
    r.culture = row.culture;
    r.generation_years = row.generation_years;
    for (const db::CultureRow& c : cultures) {
        if (c.id == row.culture) {
            r.culture_key = c.key;
        }
    }
    const data::Definition* race_def = content.find(data::RACE_KIND, row.key);
    const data::Definition* culture_def =
        r.culture_key.empty() ? nullptr : content.find(data::CULTURE_KIND, r.culture_key);
    if (race_def == nullptr || culture_def == nullptr) {
        fail("history: peoples: race '" + row.key + "' or culture '" + r.culture_key + "' is not in the content");
    }
    r.naming = culture_naming_from(culture_def->record.get_record("naming"));
    const data::Record& d = race_def->record;
    const data::Record& w = d.get_record("site_weights");
    r.w_fertility = w.get_int("fertility");
    r.w_forage = w.get_int("forage");
    r.w_timber = w.get_int("timber");
    r.w_ore = w.get_int("ore");
    r.w_fresh_water = w.get_int("fresh_water");
    r.upland = d.get_int("upland");
    r.coast = d.get_int("coast");
    r.growth_permille = d.get_int("growth_permille");
    r.expansion = d.get_int("expansion");
    r.density_permille = d.get_int("density_permille");
    r.awakening_population = d.get_int("awakening_population");  // r16 23.2 (generator 0.20.0)
    const data::Record& cw = d.get_record("cell_weights");  // P3-01 r8
    r.cell_ore = cw.get_int("ore");
    r.cell_height = cw.get_int("height");
    r.cell_inland = cw.get_int("inland");  // P3-01 r11 3.4 (generator 0.19.0)
    r.cell_port = cw.get_int("port");
    r.cell_habitability = cw.get_int("habitability");
    for (const data::Value& v : d.get_array("settlement_kinds")) {
        r.builds[static_cast<std::size_t>(kind_of_text(v.as_string()))] = 1;
    }
    for (const db::RaceBiomeRow& b : race_biomes) {
        if (b.race == row.id) {
            r.biomes.push_back(b);
        }
    }
    std::stable_sort(r.biomes.begin(), r.biomes.end(),
                     [](const db::RaceBiomeRow& a, const db::RaceBiomeRow& b) { return a.biome < b.biome; });
    return r;
}

// --- Regions, scores, capacities (4.1, 4.2, 5.1) ------------------------------------------------

PeoplesRegion peoples_region(const db::RegionRow& row, std::span<const db::RegionBiomeRow> of_region,
                             std::span<const db::BiomeRow> biomes, const PeoplesTuning& t,
                             std::int64_t islet_max_dry_cells) {
    PeoplesRegion r;
    r.id = row.id;
    r.cells = row.cells;
    r.dry = row.cells - row.lake_cells;
    r.walk_cells = row.walk_cells;
    r.coast_cells = row.coast_cells;
    r.islet = r.dry < islet_max_dry_cells;
    r.upland = 4 * row.walk_cells < row.cells;
    r.fertility_mean = row.fertility_mean;
    r.forage_mean = row.forage_mean;
    r.timber_mean = row.timber_mean;
    r.fresh_water_mean = row.fresh_water_mean;
    r.elev_max_dm = row.elev_max_dm;
    r.seed_x = row.seed_x;
    r.seed_y = row.seed_y;
    r.x_min = row.x_min;
    r.y_min = row.y_min;
    r.x_max = row.x_max;
    r.y_max = row.y_max;
    const auto is_water = [&biomes](std::int64_t code) {
        for (const db::BiomeRow& b : biomes) {
            if (b.id == code) {
                return b.water != 0;
            }
        }
        return false;
    };
    for (const db::RegionBiomeRow& b : of_region) {
        if (b.region == row.id && !is_water(b.biome)) {
            r.land = checked_add(r.land, b.cells);
        }
    }
    for (const db::RegionBiomeRow& b : of_region) {
        if (b.region == row.id) {
            r.shares.emplace_back(b.biome, r.land > 0 ? floor_div(checked_mul(b.cells, 1000), r.land) : 0);
        }
    }
    std::stable_sort(r.shares.begin(), r.shares.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    r.rough = r.dry > 0 ? floor_div(checked_mul(r.dry - r.walk_cells, 1000), r.dry) : 0;
    r.ore_m = std::min<std::int64_t>(255, floor_div(checked_mul(row.ore_reach_cells, 255), t.ore_reach_full));
    r.coast_m = std::min<std::int64_t>(1000, floor_div(checked_mul(row.coast_cells, 1000), t.coast_full));
    return r;
}

std::int64_t region_biome_term(const PeoplesRegion& g, const PeoplesRace& r) {
    std::int64_t b_num = 0;
    for (const db::RaceBiomeRow& w : r.biomes) {
        for (const auto& [biome, share] : g.shares) {
            if (biome == w.biome) {
                b_num = checked_add(b_num, checked_mul(w.weight, share));
            }
        }
    }
    return floor_div(b_num, 1000);
}

std::int64_t region_score(const PeoplesRegion& g, const PeoplesRace& r, const PeoplesTuning& t) {
    const std::int64_t wsum = r.w_fertility + r.w_forage + r.w_timber + r.w_ore + r.w_fresh_water;
    std::int64_t s_num = 0;
    s_num = checked_add(s_num, checked_mul(r.w_fertility, g.fertility_mean));
    s_num = checked_add(s_num, checked_mul(r.w_forage, g.forage_mean));
    s_num = checked_add(s_num, checked_mul(r.w_timber, g.timber_mean));
    s_num = checked_add(s_num, checked_mul(r.w_ore, g.ore_m));
    s_num = checked_add(s_num, checked_mul(r.w_fresh_water, g.fresh_water_mean));
    // race_site_weights_positive (P3-01) makes wsum > 0.
    const std::int64_t S = wsum > 0 ? floor_div(checked_mul(s_num, 1000), checked_mul(255, wsum)) : 0;
    const std::int64_t B = region_biome_term(g, r);
    const std::int64_t U = floor_div(checked_mul(r.upland, g.rough), 1000);
    const std::int64_t C = floor_div(checked_mul(r.coast, g.coast_m), 1000);
    std::int64_t sum = checked_mul(t.score_site, S);
    sum = checked_add(sum, checked_mul(t.score_biome, B));
    sum = checked_add(sum, checked_mul(t.score_upland, U));
    sum = checked_add(sum, checked_mul(t.score_coast, C));
    return floor_div(sum, 1000);
}

std::int64_t region_fit(std::int64_t score) noexcept {
    return std::min<std::int64_t>(1000, std::max<std::int64_t>(0, score));
}

std::int64_t region_capacity(const PeoplesRegion& g, const PeoplesRace& r, const PeoplesTuning& t) {
    const std::int64_t q = region_fit(region_score(g, r, t));
    const std::int64_t rough_r =
        t.rough_base + floor_div(checked_mul(std::max<std::int64_t>(0, r.upland), t.rough_upland), 1000);
    const std::int64_t area = g.walk_cells + floor_div(checked_mul(g.dry - g.walk_cells, rough_r), 1000);
    const std::int64_t product =
        checked_mul(checked_mul(checked_mul(area, q), r.density_permille), t.people_per_kcell);
    const std::int64_t c = std::max<std::int64_t>(t.capacity_floor, floor_div(product, 1000000000));
    if (c > MAX_CAPACITY) {
        fail("history: bound: capacity of region " + int_text(g.id) + " for race " + int_text(r.id) + " exceeds " +
             int_text(MAX_CAPACITY));
    }
    return c;
}

std::optional<std::int64_t> border_cost(const db::RegionBorderRow& b, const PeoplesTuning& t) {
    std::int64_t cost = 0;
    if (b.walk_pairs > 0) {
        cost = 1;
    } else if (b.pass_id > 0) {
        cost = t.pass_cost;
    } else {
        return std::nullopt;
    }
    if (b.length_m > 0 &&
        checked_mul(checked_mul(b.river_pairs, 16), 1000) >= checked_mul(t.river_border_share_permille, b.length_m)) {
        cost += t.river_cost;
    }
    return cost;
}

// --- Kinds and growth (5.2, 8.1, 9.1) -------------------------------------------------------------

std::int64_t kind_min(SettlementKind kind, const PeoplesTuning& t) noexcept {
    switch (kind) {
        case SettlementKind::hamlet: return 0;
        case SettlementKind::village: return t.village_min;
        case SettlementKind::town: return t.town_min;
        case SettlementKind::city: return t.city_min;
        case SettlementKind::fortress: return 0;
    }
    return 0;
}

SettlementKind ladder_kind(const PeoplesRace& race, std::int64_t population, const PeoplesTuning& t) {
    std::optional<SettlementKind> best;
    std::optional<SettlementKind> lowest;
    for (const SettlementKind k : LADDER) {
        if (!race.builds_kind(k)) {
            continue;
        }
        if (!lowest) {
            lowest = k;
        }
        if (kind_min(k, t) <= population) {
            best = k;
        }
    }
    if (best) {
        return *best;
    }
    return lowest ? *lowest : SettlementKind::fortress;
}

std::int64_t kind_cap(const PeoplesRace& race, SettlementKind kind, const PeoplesTuning& t) {
    if (kind == SettlementKind::fortress) {
        return t.fortress_max;
    }
    std::int64_t cap = t.fortress_max;  // a race with no ladder kind builds only fortresses
    for (const SettlementKind k : LADDER) {
        if (!race.builds_kind(k)) {
            continue;
        }
        switch (k) {
            case SettlementKind::hamlet: cap = t.village_min - 1; break;
            case SettlementKind::village: cap = t.town_min - 1; break;
            case SettlementKind::town: cap = t.city_min - 1; break;
            case SettlementKind::city: cap = t.city_max; break;
            case SettlementKind::fortress: break;
        }
    }
    return cap;
}

std::int64_t grow_population(std::int64_t p, std::int64_t share, std::int64_t g, std::int64_t cap,
                             std::int64_t decline_permille, std::int64_t wild) {
    // r6 (9.1): the target is min(share, cap), so an excess over the kind
    // cap drains like an excess over the share.
    const std::int64_t target = std::min(share, cap);
    if (p < target) {
        std::int64_t d = 0;
        if (wild == 1000) {
            // P5-02 7.1 with G = 1000: the same rational number as below,
            // floored, in 9.1's form.
            const std::int64_t num = checked_add(checked_mul(checked_mul(p, g), share - p), checked_mul(500, share));
            d = floor_div(num, checked_mul(1000, share));
        } else {
            // P5-02 7.1: (P g G (C - P) + 500 x 1000 x C) div (10^6 C).
            const std::int64_t num = checked_add(checked_mul(checked_mul(checked_mul(p, g), wild), share - p),
                                                 checked_mul(500000, share));
            d = floor_div(num, checked_mul(1000000, share));
        }
        if (g > 0) {
            d = std::max<std::int64_t>(1, d);
        }
        return std::min(p + d, cap);
    }
    if (p > target) {
        return p - std::max<std::int64_t>(1, floor_div(checked_mul(p - target, decline_permille), 1000));
    }
    return p;
}

// r12 22.5: sea_d(c), the Chebyshev distance in cells from c to the
// nearest sea cell, capped at `cap`: a multi-source breadth-first search over
// the 8-neighbour grid from every sea cell, in unit steps, stopped at `cap`.
// Every cell farther (and every cell of a world without sea) has `cap`.
namespace {

std::vector<std::uint16_t> capped_sea_distance(const std::vector<std::uint8_t>& sea, std::int64_t n, std::int64_t cap) {
    const auto cells = static_cast<std::size_t>(n * n);
    std::vector<std::uint16_t> d(cells, static_cast<std::uint16_t>(cap));
    std::vector<std::size_t> frontier;
    for (std::size_t i = 0; i < cells; ++i) {
        if (sea[i] != 0) {
            d[i] = 0;
            frontier.push_back(i);
        }
    }
    for (std::int64_t step = 1; step < cap && !frontier.empty(); ++step) {
        std::vector<std::size_t> next;
        for (const std::size_t i : frontier) {
            const std::int64_t x = static_cast<std::int64_t>(i) % n;
            const std::int64_t y = static_cast<std::int64_t>(i) / n;
            for (std::int64_t dy = -1; dy <= 1; ++dy) {
                for (std::int64_t dx = -1; dx <= 1; ++dx) {
                    const std::int64_t nx = x + dx;
                    const std::int64_t ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= n || ny >= n) {
                        continue;
                    }
                    const auto j = static_cast<std::size_t>(ny * n + nx);
                    if (d[j] > step) {
                        d[j] = static_cast<std::uint16_t>(step);
                        next.push_back(j);
                    }
                }
            }
        }
        frontier = std::move(next);
    }
    return d;
}

}  // namespace

// --- PeoplesGeo ---------------------------------------------------------------------------------

PeoplesGeo::PeoplesGeo(const HistoryInputs& in, const data::ContentSet& content, const PeoplesTuning& tuning,
                       std::int64_t islet_max_dry_cells)
    : tuning_(tuning), n_(in.map_size) {
    const std::int64_t n = n_;
    const auto cells = static_cast<std::size_t>(n * n);
    // Layers (the region layer is N x N u16, checked by the engine).
    const Cells region(in.region_layer, "region", n, db::ElementType::u16);
    const Cells water(in.water_layer, "water", n, db::ElementType::u8);
    const Cells hab(in.habitability_layer, "habitability", n, db::ElementType::u8);
    const Cells river(in.river_layer, "river", n, db::ElementType::u8);
    const Cells flow(in.flow_dir_layer, "flow_dir", n, db::ElementType::u8);
    // r6: the cell term of 6.2 and the fortress cell rule of 8.4.
    const Cells elevation(in.elevation_layer, "elevation", n, db::ElementType::i16);
    const Cells ore(in.ore_layer, "ore", n, db::ElementType::u8);
    const Cells walk(in.walk_area_layer, "walk_area", n, db::ElementType::u16);
    region_layer_.resize(cells);
    water_.resize(cells);
    habitability_.resize(cells);
    elevation_.resize(cells);
    ore_.resize(cells);
    walk_area_.resize(cells);
    mouth_.assign(cells, 0);
    std::vector<std::uint8_t> sea(cells, 0);
    for (std::size_t i = 0; i < cells; ++i) {
        region_layer_[i] = static_cast<std::uint16_t>(region.at(i));
        water_[i] = static_cast<std::uint8_t>(std::min<std::int64_t>(255, water.at(i)));
        habitability_[i] = static_cast<std::uint16_t>(hab.at(i));
        elevation_[i] = static_cast<std::int16_t>(elevation.at(i));
        ore_[i] = static_cast<std::uint16_t>(ore.at(i));
        walk_area_[i] = static_cast<std::uint16_t>(walk.at(i));
        sea[i] = static_cast<std::uint8_t>(water.at(i) == WATER_OCEAN);
    }
    // 6.1: mouth cells.
    for (std::int64_t y = 0; y < n; ++y) {
        for (std::int64_t x = 0; x < n; ++x) {
            const auto i = static_cast<std::size_t>(y * n + x);
            const std::int64_t d = flow.at(i);
            if (river.at(i) < tuning_.mouth_class_min || water.at(i) != WATER_RIVER || d < 0 || d > 7) {
                continue;
            }
            const std::int64_t tx = x + geo::D8_DX[static_cast<std::size_t>(d)];
            const std::int64_t ty = y + geo::D8_DY[static_cast<std::size_t>(d)];
            if (tx >= 0 && ty >= 0 && tx < n && ty < n && water.at(static_cast<std::size_t>(ty * n + tx)) == WATER_OCEAN) {
                mouth_[i] = 1;
            }
        }
    }
    near_mouth_ = dilate(mouth_, n, tuning_.port_radius);
    near_sea_ = dilate(sea, n, tuning_.port_radius);

    // Regions (ids 1, 2, ... in order: GeoView checks).
    regions_.reserve(in.regions.size());
    for (const db::RegionRow& row : in.regions) {
        regions_.push_back(peoples_region(row, in.region_biomes, in.biomes, tuning_, islet_max_dry_cells));
    }
    const std::size_t nr = regions_.size();
    cells_of_region_.assign(nr, {});
    for (std::size_t i = 0; i < cells; ++i) {
        const std::int64_t r = region_layer_[i];
        if (r >= 1 && static_cast<std::size_t>(r) <= nr && water_[i] == WATER_LAND) {
            cells_of_region_[static_cast<std::size_t>(r - 1)].push_back(static_cast<std::uint32_t>(i));
        }
    }
    // Races, scores, capacities.
    bool inland = false;
    for (const db::RaceRow& row : in.races) {
        if (row.generation_years < 1) {
            fail("history: peoples: race " + int_text(row.id) + " has no generation_years above 0");
        }
        races_.push_back(peoples_race(row, in.race_biomes, in.cultures, content));
        inland = inland || races_.back().cell_inland > 0;
    }
    // r12 22.5: the capped distance from the sea, only when a present race weighs it.
    if (inland) {
        sea_d_ = capped_sea_distance(sea, n, tuning_.inland_cap);
    }
    score_.resize(races_.size() * nr);
    capacity_.resize(races_.size() * nr);
    biome_term_.resize(races_.size() * nr);
    for (std::size_t k = 0; k < races_.size(); ++k) {
        for (std::size_t j = 0; j < nr; ++j) {
            score_[k * nr + j] = region_score(regions_[j], races_[k], tuning_);
            capacity_[k * nr + j] = region_capacity(regions_[j], races_[k], tuning_);
            biome_term_[k * nr + j] = region_biome_term(regions_[j], races_[k]);
        }
    }
    // Open borders.
    neighbours_.assign(nr, {});
    for (const db::RegionBorderRow& b : in.region_borders) {
        const std::optional<std::int64_t> cost = border_cost(b, tuning_);
        if (!cost || b.a < 1 || b.b < 1 || static_cast<std::size_t>(b.a) > nr || static_cast<std::size_t>(b.b) > nr) {
            continue;
        }
        neighbours_[static_cast<std::size_t>(b.a - 1)].push_back(PeoplesNeighbour{b.b, *cost});
        neighbours_[static_cast<std::size_t>(b.b - 1)].push_back(PeoplesNeighbour{b.a, *cost});
    }
    for (std::vector<PeoplesNeighbour>& list : neighbours_) {
        std::stable_sort(list.begin(), list.end(),
                         [](const PeoplesNeighbour& a, const PeoplesNeighbour& b) { return a.region < b.region; });
    }
    hops_.assign(nr, {});
    candidates_.assign(races_.size() * nr, {});
    candidates_ready_.assign(races_.size() * nr, 0);
}

const PeoplesRegion& PeoplesGeo::region(std::int64_t id) const {
    if (id < 1 || id > region_count()) {
        fail("history: peoples: region " + int_text(id) + " does not exist");
    }
    return regions_[static_cast<std::size_t>(id - 1)];
}

std::size_t PeoplesGeo::race_index(std::int64_t race) const {
    for (std::size_t k = 0; k < races_.size(); ++k) {
        if (races_[k].id == race) {
            return k;
        }
    }
    fail("history: peoples: race " + int_text(race) + " does not exist");
}

const PeoplesRace& PeoplesGeo::race(std::int64_t id) const {
    return races_[race_index(id)];
}

std::int64_t PeoplesGeo::score(std::int64_t region_id, std::int64_t race) const {
    (void)region(region_id);
    return score_[race_index(race) * regions_.size() + static_cast<std::size_t>(region_id - 1)];
}

std::int64_t PeoplesGeo::capacity(std::int64_t region_id, std::int64_t race) const {
    (void)region(region_id);
    return capacity_[race_index(race) * regions_.size() + static_cast<std::size_t>(region_id - 1)];
}

std::int64_t PeoplesGeo::biome_term(std::int64_t region_id, std::int64_t race) const {
    (void)region(region_id);
    return biome_term_[race_index(race) * regions_.size() + static_cast<std::size_t>(region_id - 1)];
}

std::int64_t PeoplesGeo::non_islet_count() const noexcept {
    std::int64_t n = 0;
    for (const PeoplesRegion& r : regions_) {
        n += r.islet ? 0 : 1;
    }
    return n;
}

std::span<const PeoplesNeighbour> PeoplesGeo::neighbours(std::int64_t region_id) const {
    (void)region(region_id);
    return neighbours_[static_cast<std::size_t>(region_id - 1)];
}

const std::vector<std::int64_t>& PeoplesGeo::hops(std::int64_t from) const {
    (void)region(from);
    std::vector<std::int64_t>& d = hops_[static_cast<std::size_t>(from - 1)];
    if (!d.empty()) {
        return d;
    }
    d.assign(regions_.size(), -1);
    std::vector<std::int64_t> queue{from};
    d[static_cast<std::size_t>(from - 1)] = 0;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::int64_t u = queue[head];
        for (const PeoplesNeighbour& v : neighbours_[static_cast<std::size_t>(u - 1)]) {
            std::int64_t& dv = d[static_cast<std::size_t>(v.region - 1)];
            if (dv < 0) {
                dv = d[static_cast<std::size_t>(u - 1)] + 1;
                queue.push_back(v.region);
            }
        }
    }
    return d;
}

std::size_t PeoplesGeo::cell(std::int64_t x, std::int64_t y) const {
    if (x < 0 || y < 0 || x >= n_ || y >= n_) {
        fail("history: peoples: cell (" + int_text(x) + ", " + int_text(y) + ") is outside the map");
    }
    return static_cast<std::size_t>(y * n_ + x);
}

bool PeoplesGeo::is_mouth(std::int64_t x, std::int64_t y) const {
    return mouth_[cell(x, y)] != 0;
}

bool PeoplesGeo::near_mouth(std::int64_t x, std::int64_t y) const {
    return near_mouth_[cell(x, y)] != 0;
}

bool PeoplesGeo::near_sea(std::int64_t x, std::int64_t y) const {
    return near_sea_[cell(x, y)] != 0;
}

bool PeoplesGeo::walkable(std::int64_t x, std::int64_t y) const {
    return walk_area_[cell(x, y)] != 0;
}

std::int64_t PeoplesGeo::site_value(std::int64_t x, std::int64_t y, std::int64_t race_id) const {
    const std::size_t i = cell(x, y);
    const PeoplesRace& r = race(race_id);
    const std::int64_t bonus = near_mouth_[i] != 0 ? tuning_.port_mouth_bonus
                               : near_sea_[i] != 0 ? tuning_.port_coast_bonus
                                                   : 0;
    const std::int64_t port = floor_div(bonus * (1000 + r.coast), 1000);  // 6.1
    // r12 22.5: v = hab x cw_a div 1000 + port x cw_p div 1000
    //            + (cw_o x ore + cw_h x hgt) div 1000
    //            + cw_i x min(cap, sea_d) x 255 div (cap x 1000).
    std::int64_t v = floor_div(static_cast<std::int64_t>(habitability_[i]) * r.cell_habitability, 1000);
    v += floor_div(port * r.cell_port, 1000);
    if (r.cell_ore != 0 || r.cell_height != 0) {
        // r6 (6.2 item 2): the cell's ore and its height in its region.
        const std::int64_t reg = region_layer_[i];
        const std::int64_t elev_max = reg >= 1 && reg <= region_count() ? region(reg).elev_max_dm : 0;
        const std::int64_t hgt = std::min<std::int64_t>(
            255, floor_div(std::max<std::int64_t>(0, elevation_[i]) * 255, std::max<std::int64_t>(1, elev_max)));
        v += floor_div(r.cell_ore * static_cast<std::int64_t>(ore_[i]) + r.cell_height * hgt, 1000);
    }
    if (r.cell_inland != 0) {
        const std::int64_t cap = tuning_.inland_cap;
        const std::int64_t d = std::min<std::int64_t>(cap, sea_d_[i]);
        v += floor_div(r.cell_inland * d * 255, cap * 1000);
    }
    return v;
}

std::int64_t PeoplesGeo::sea_distance(std::int64_t x, std::int64_t y) const {
    const std::size_t i = cell(x, y);
    return sea_d_.empty() ? tuning_.inland_cap : static_cast<std::int64_t>(sea_d_[i]);
}

const std::vector<std::uint32_t>& PeoplesGeo::candidates(std::int64_t region_id, std::int64_t race_id) const {
    (void)region(region_id);
    const std::size_t at = race_index(race_id) * regions_.size() + static_cast<std::size_t>(region_id - 1);
    std::vector<std::uint32_t>& list = candidates_[at];
    if (candidates_ready_[at] != 0) {
        return list;
    }
    const std::vector<std::uint32_t>& cells = cells_of_region_[static_cast<std::size_t>(region_id - 1)];
    std::vector<std::pair<std::int64_t, std::uint32_t>> keyed;
    keyed.reserve(cells.size());
    for (const std::uint32_t c : cells) {
        keyed.emplace_back(site_value(static_cast<std::int64_t>(c) % n_, static_cast<std::int64_t>(c) / n_, race_id), c);
    }
    // Value descending, then cell index ascending (6.2 item 2).
    std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    list.reserve(keyed.size());
    for (const auto& k : keyed) {
        list.push_back(k.second);
    }
    candidates_ready_[at] = 1;
    return list;
}

std::optional<SiteCell> PeoplesGeo::site(std::int64_t region_id, std::int64_t race_id,
                                         std::span<const Settlement> settlements, bool relaxed) const {
    const PeoplesRegion& reg = region(region_id);
    const std::int64_t sp = tuning_.site_spacing;
    const std::int64_t sp2 = sp * sp;
    // Settlements that keep their spacing (statuses 1 to 3) and lie near the
    // region's box; the others cannot be closer than the spacing.
    std::vector<SiteCell> block;
    for (const Settlement& s : settlements) {
        if (s.status == SettlementStatus::faded) {
            continue;
        }
        if (s.x >= reg.x_min - sp && s.x <= reg.x_max + sp && s.y >= reg.y_min - sp && s.y <= reg.y_max + sp) {
            block.push_back(SiteCell{s.x, s.y});
        }
    }
    const auto free = [&block, sp2](std::int64_t x, std::int64_t y) {
        for (const SiteCell& b : block) {
            const std::int64_t dx = b.x - x;
            const std::int64_t dy = b.y - y;
            if (dx * dx + dy * dy < sp2) {
                return false;
            }
        }
        return true;
    };
    // 1. The seed cell, if it is a free candidate; r6: only for a race
    // without a cell term.
    if (!race(race_id).has_cell_term() && reg.seed_x >= 0 && reg.seed_y >= 0 && reg.seed_x < n_ && reg.seed_y < n_) {
        const std::size_t i = cell(reg.seed_x, reg.seed_y);
        if (region_layer_[i] == region_id && water_[i] == WATER_LAND && free(reg.seed_x, reg.seed_y)) {
            return SiteCell{reg.seed_x, reg.seed_y};
        }
    }
    // 2. The best free candidate.
    const std::vector<std::uint32_t>& list = candidates(region_id, race_id);
    for (const std::uint32_t c : list) {
        const std::int64_t x = static_cast<std::int64_t>(c) % n_;
        const std::int64_t y = static_cast<std::int64_t>(c) / n_;
        if (free(x, y)) {
            return SiteCell{x, y};
        }
    }
    // 3. Relaxed: the best candidate no settlement of any status stands on.
    if (relaxed) {
        for (const std::uint32_t c : list) {
            const std::int64_t x = static_cast<std::int64_t>(c) % n_;
            const std::int64_t y = static_cast<std::int64_t>(c) / n_;
            bool taken = false;
            for (const Settlement& s : settlements) {
                taken = taken || (s.x == x && s.y == y);
            }
            if (!taken) {
                return SiteCell{x, y};
            }
        }
    }
    return std::nullopt;
}

}  // namespace worldgen::history
