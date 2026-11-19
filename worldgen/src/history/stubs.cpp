// The stub subsystems of P3-04 (spec section 7).

#include "worldgen/history/stubs.hpp"

#include <string>
#include <tuple>

#include "worldgen/core/error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

const db::RaceRow& race_row(const HistoryInputs& in, std::int64_t race) {
    for (const db::RaceRow& r : in.races) {
        if (r.id == race) {
            return r;
        }
    }
    fail("history: stub: race " + int_text(race) + " has no row");
}

const db::CultureRow& culture_row(const HistoryInputs& in, std::int64_t culture) {
    for (const db::CultureRow& c : in.cultures) {
        if (c.id == culture) {
            return c;
        }
    }
    fail("history: stub: culture " + int_text(culture) + " has no row");
}

SettlementKind settlement_kind_of(std::string_view text) {
    static constexpr std::string_view KINDS[] = {"hamlet", "village", "town", "city", "fortress"};
    for (std::size_t i = 0; i < 5; ++i) {
        if (KINDS[i] == text) {
            return static_cast<SettlementKind>(static_cast<std::int64_t>(i) + 1);
        }
    }
    fail("history: stub: '" + std::string(text) + "' is not a settlement kind");
}

bool settlement_at(const State& state, std::int64_t x, std::int64_t y) {
    for (const Settlement& s : state.settlements()) {
        if (s.x == x && s.y == y) {
            return true;
        }
    }
    return false;
}

// The value of a u16 (region) or u8 (water) layer at (x, y), or -1 when the
// layer does not cover the cell or has another element type.
std::int64_t cell_value(const db::LayerGrid& g, std::int64_t x, std::int64_t y) {
    if (x < 0 || y < 0 || x >= static_cast<std::int64_t>(g.width) || y >= static_cast<std::int64_t>(g.height)) {
        return -1;
    }
    const auto i = static_cast<std::size_t>(y * static_cast<std::int64_t>(g.width) + x);
    if (const std::vector<std::uint16_t>* v = g.get_if<std::uint16_t>()) {
        return (*v)[i];
    }
    if (const std::vector<std::uint8_t>* v = g.get_if<std::uint8_t>()) {
        return (*v)[i];
    }
    return -1;
}

constexpr std::int64_t WATER_LAKE = 2;  // layer water: 2 = lake (P1-02)

}  // namespace

StubAwakening::StubAwakening(const data::ContentSet& content, const HistoryInputs& inputs,
                             std::int64_t islet_max_dry_cells)
    : content_(content), islet_max_dry_cells_(islet_max_dry_cells) {
    // P3-03 8.2: the registry from the stored names, regions, ages, gods.
    for (const db::RegionRow& r : inputs.regions) {
        registry_.add(r.name);
    }
    for (const db::AgeRow& a : inputs.ages) {
        registry_.add(a.name);
    }
    for (const db::GodRow& g : inputs.gods) {
        registry_.add(g.name);
    }
}

std::int64_t StubAwakening::awakening_place(const StepContext& context, std::int64_t race) {
    (void)race;  // the stub ignores race preferences (7.1)
    const std::vector<db::RegionRow>& regions = context.inputs().regions;
    if (regions.empty()) {
        fail("history: stub: no region");
    }
    const auto non_islet = [this](const db::RegionRow& r) { return r.cells - r.lake_cells >= islet_max_dry_cells_; };
    const auto free = [&context](const db::RegionRow& r) { return context.state().region(r.id).owner == 0; };
    // (habitability_mean, walk_cells) larger first, then smaller id.
    const auto better = [](const db::RegionRow& a, const db::RegionRow& b) {
        return std::make_tuple(a.habitability_mean, a.walk_cells, -a.id) >
               std::make_tuple(b.habitability_mean, b.walk_cells, -b.id);
    };
    for (int set = 1; set <= 3; ++set) {
        const db::RegionRow* best = nullptr;
        for (const db::RegionRow& r : regions) {
            const bool in_set = set == 1 ? non_islet(r) && free(r) : set == 2 ? non_islet(r) : true;
            if (in_set && (best == nullptr || better(r, *best))) {
                best = &r;
            }
        }
        if (best != nullptr) {
            return best->id;
        }
    }
    fail("history: stub: no region");
}

void StubAwakening::awaken(StepContext& context, std::int64_t race, std::int64_t awakening_event) {
    const HistoryInputs& in = context.inputs();
    State& state = context.state();
    const std::int64_t home = awakening_place(context, race);  // the state has not changed since the event
    const db::RaceRow& r = race_row(in, race);
    const db::CultureRow& culture = culture_row(in, r.culture);
    const data::Definition* race_def = content_.find(data::RACE_KIND, r.key);
    const data::Definition* culture_def = content_.find(data::CULTURE_KIND, culture.key);
    if (race_def == nullptr || culture_def == nullptr) {
        fail("history: stub: race '" + r.key + "' or culture '" + culture.key + "' is not in the content");
    }
    const CultureNaming naming = culture_naming_from(culture_def->record.get_record("naming"));
    const std::uint64_t seed = in.seed;
    const auto culture_number = static_cast<std::uint64_t>(culture.id);

    // 1. The civilization.
    const auto c = static_cast<std::int64_t>(state.civs().size()) + 1;
    std::string civ_name = make_name(naming, culture.key, NameKind::civilization, seed, culture_number,
                                     static_cast<std::uint64_t>(c), registry_);
    EventSpec founded;
    founded.kind = "civ_founded";
    founded.year = context.year();
    founded.end_year = context.year();
    founded.region = home;
    founded.participants = {Participant{"civilization", EntityType::civilization, c},
                            Participant{"race", EntityType::race, race}};
    founded.causes = {awakening_event};
    const std::int64_t civ_event = context.append(founded);
    if (state.found_civilization(race, culture.id, home, std::move(civ_name), civ_event) != c) {
        fail("history: stub: the civilization id is not the expected one");
    }
    if (state.region(home).owner == 0) {
        state.set_region_owner(home, c, civ_event);
    }

    // 2. The site: the seed cell, else the first free dry cell of the region
    // in row-major order of its bounding box.
    const db::RegionRow& region = in.regions[static_cast<std::size_t>(home - 1)];
    std::int64_t x = region.seed_x;
    std::int64_t y = region.seed_y;
    if (settlement_at(state, x, y)) {
        bool found = false;
        for (std::int64_t yy = region.y_min; yy <= region.y_max && !found; ++yy) {
            for (std::int64_t xx = region.x_min; xx <= region.x_max && !found; ++xx) {
                if (cell_value(in.region_layer, xx, yy) == home && cell_value(in.water_layer, xx, yy) != WATER_LAKE &&
                    !settlement_at(state, xx, yy)) {
                    x = xx;
                    y = yy;
                    found = true;
                }
            }
        }
        if (!found) {
            fail("history: stub: no settlement site in region " + int_text(home));
        }
    }

    // 3. The settlement.
    const auto s = static_cast<std::int64_t>(state.settlements().size()) + 1;
    std::string settlement_name = make_name(naming, culture.key, NameKind::settlement, seed, culture_number,
                                            static_cast<std::uint64_t>(s), registry_);
    EventSpec settled;
    settled.kind = "settlement_founded";
    settled.year = context.year();
    settled.end_year = context.year();
    settled.region = home;
    settled.settlement = s;
    settled.participants = {Participant{"settlement", EntityType::settlement, s},
                            Participant{"founder", EntityType::civilization, c}};
    settled.values = {STUB_POPULATION};
    settled.causes = {civ_event};
    const std::int64_t settlement_event = context.append(settled);
    const std::vector<data::Value>& kinds = race_def->record.get_array("settlement_kinds");
    if (kinds.empty()) {
        fail("history: stub: race '" + r.key + "' has no settlement kind");
    }
    if (state.found_settlement(x, y, home, c, settlement_kind_of(kinds.front().as_string()), STUB_POPULATION,
                               std::move(settlement_name), settlement_event) != s) {
        fail("history: stub: the settlement id is not the expected one");
    }
    state.set_capital(c, s);
}

namespace testing {

ForcedTension::ForcedTension(std::int64_t period, std::int64_t value, std::int64_t trigger)
    : period_(period), value_(value), trigger_(trigger) {
    if (period < 1) {
        fail("ForcedTension: period below 1");
    }
}

TensionReport ForcedTension::tension(const StepContext& context) {
    const std::int64_t end = context.year() + context.years();
    return TensionReport{end % period_ == 0 ? value_ : 0, trigger_};
}

}  // namespace testing

}  // namespace worldgen::history
