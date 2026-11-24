// The monsters subsystem (spec docs/specs/p5-02-beasts-monsters.md sections
// 4 to 9): beasts, monsters, sites and the second tension source.
//
// Rules of P3-04 6.9 as P5-02 6.7 states them: no founding, no relation;
// every loop over settlements, civilizations, regions, monsters or sites
// runs over ids up to a count taken before it and re-fetches by id after
// any change; members hold ids. Draws are keyed by ids and the step's year.

#include "worldgen/history/monsters.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

// The twelve simulation streams of P5-02 3 (the name streams are the name
// generator's), in registration order.
enum Stream : std::size_t {
    BOLD = 0,
    WAKE,
    RISE,
    SLEEP,
    REWAKE,
    RAID,
    RAID_TOLL,
    OVERRUN,
    HUNT,
    HUNT_OUTCOME,
    LAIR_NAME,
    OUTPOST,
    RAID_TARGET,
    BOLD_KIND,  // P5-03 13
    FEAR,       // P5-03 9.4
};
constexpr std::array<std::string_view, 15> STREAM_NAMES = {
    streams::hist_beasts_bold,     streams::hist_monsters_wake,      streams::hist_monsters_rise,
    streams::hist_monsters_sleep,  streams::hist_monsters_rewake,    streams::hist_monsters_raid,
    streams::hist_monsters_raid_toll, streams::hist_monsters_overrun, streams::hist_monsters_hunt,
    streams::hist_monsters_hunt_outcome, streams::hist_monsters_lair_name, streams::hist_sites_outpost,
    streams::hist_monsters_raid_target, streams::hist_beasts_bold_kind, streams::hist_monsters_fear,
};

constexpr std::array<Step, 5> STEPS = {Step::t1, Step::t4, Step::t6, Step::q1, Step::q4};

constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
constexpr std::int64_t PPB = 1000000000;
constexpr std::int64_t WATER_LAND = 0;  // layer water (P1-02): 0 land
constexpr std::int64_t CIV_TYPE = static_cast<std::int64_t>(EntityType::civilization);
constexpr std::string_view ROOT_SLOT = "{root}";
constexpr std::string_view PLACE_SLOT = "{place}";  // P5-03 12.2

// P5-03 12.1: the epithet of a name form, the form with {root} removed and
// outer spaces trimmed.
std::string epithet_of(std::string_view form) {
    std::string out(form);
    const std::size_t at = out.find(ROOT_SLOT);
    if (at != std::string::npos) {
        out.erase(at, ROOT_SLOT.size());
    }
    const std::size_t a = out.find_first_not_of(' ');
    if (a == std::string::npos) {
        return std::string();
    }
    const std::size_t b = out.find_last_not_of(' ');
    return out.substr(a, b - a + 1);
}

// The form a name was built with (P5-02 9.2's matching: the first form in
// list order whose parts around {root} enclose it), or nullptr.
const std::string* matched_form(std::string_view name, const std::vector<std::string>& forms) {
    for (const std::string& form : forms) {
        const std::size_t at = form.find(ROOT_SLOT);
        if (at == std::string::npos) {
            continue;
        }
        const std::string_view f = form;
        const std::string_view a = f.substr(0, at);
        const std::string_view b = f.substr(at + ROOT_SLOT.size());
        if (name.size() > a.size() + b.size() && name.substr(0, a.size()) == a &&
            name.substr(name.size() - b.size()) == b) {
            return &form;
        }
    }
    return nullptr;
}

bool chance_scaled(std::uint64_t u, std::int64_t p, std::int64_t scale) {
    const std::int64_t raw = std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), scale));
    return chance(u, Fx::from_raw(raw));
}
bool chance_pm(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, 1000);
}
bool chance_ppm(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, 1000000);
}

bool standing(const Settlement& s) noexcept {
    return s.status == SettlementStatus::thriving || s.status == SettlementStatus::declining;
}

bool quiet(const StepContext& ctx) noexcept {
    return ctx.step() == Step::q1 || ctx.step() == Step::q4;
}

std::int64_t settlement_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.settlements().size());
}

std::int64_t civ_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.civs().size());
}

EventSpec point_event(const StepContext& ctx, std::string_view kind, std::int64_t region, std::int64_t settlement) {
    EventSpec e;
    e.kind = kind;
    e.year = ctx.year();
    e.end_year = ctx.year();
    e.region = region;
    e.settlement = settlement;
    return e;
}

std::vector<std::string> string_list(const data::Record& record, std::string_view field) {
    std::vector<std::string> out;
    for (const data::Value& v : record.get_array(field)) {
        out.push_back(v.as_string());
    }
    return out;
}

std::vector<HabitatWeight> habitat_list(const data::Record& record, std::string_view field) {
    std::vector<HabitatWeight> out;
    for (const data::Value& v : record.get_array(field)) {
        const data::Record& r = v.as_record();
        out.push_back(HabitatWeight{r.get_string("biome"), r.get_int("weight")});
    }
    return out;
}

SettlementKind kind_of(std::string_view text) {
    if (text == "hamlet") return SettlementKind::hamlet;
    if (text == "village") return SettlementKind::village;
    if (text == "town") return SettlementKind::town;
    if (text == "city") return SettlementKind::city;
    return SettlementKind::fortress;
}

// The weight of a biome key in a habitat list, 0 when unlisted.
std::int64_t weight_of(const std::vector<HabitatWeight>& list, std::string_view key) {
    for (const HabitatWeight& h : list) {
        if (h.biome == key) {
            return h.weight;
        }
    }
    return 0;
}

bool relation_open(const State& state, std::int64_t a, std::int64_t b, std::string_view kind) {
    return state.relations().count(RelationKey{CIV_TYPE, a, CIV_TYPE, b, std::string(kind)}) != 0;
}

// An open war between a and b, either order.
bool at_war(const State& state, std::int64_t a, std::int64_t b) {
    return relation_open(state, a, b, "war") || relation_open(state, b, a, "war");
}

// An open war (either order) or enmity (a < b) between a and b.
bool hostile(const State& state, std::int64_t a, std::int64_t b) {
    return at_war(state, a, b) || relation_open(state, std::min(a, b), std::max(a, b), "enmity");
}

}  // namespace

// --- Data -------------------------------------------------------------------------------------------

MonstersTuning monsters_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_MONSTERS_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.monsters");
    }
    const data::Record& r = d->record;
    MonstersTuning t;
    const data::Record& p = r.get_record("pressure");
    t.pressure_low = p.get_int("low");
    t.pressure_normal = p.get_int("normal");
    t.pressure_high = p.get_int("high");
    t.living_permille = r.get_int("living_permille");
    t.undead_min_dead = r.get_int("undead_min_dead");
    t.undead_window_years = r.get_int("undead_window_years");
    t.hunt_hops = r.get_int("hunt_hops");
    t.hunt_levy_permille = r.get_int("hunt_levy_permille");
    t.hunt_permille = r.get_int("hunt_permille");
    t.hunt_memory_years = r.get_int("hunt_memory_years");
    t.slay_max_permille = r.get_int("slay_max_permille");
    t.drive_permille = r.get_int("drive_permille");
    t.drive_years = r.get_int("drive_years");
    t.displace_per_kpeople = r.get_int("displace_per_kpeople");
    t.displace_max_permille = r.get_int("displace_max_permille");
    t.beast_decline_permille = r.get_int("beast_decline_permille");
    t.people_floor = r.get_int("people_floor");
    t.drag_k = r.get_int("drag_k");
    t.drag_max_permille = r.get_int("drag_max_permille");
    t.feed_k = r.get_int("feed_k");
    t.feed_max_permille = r.get_int("feed_max_permille");
    t.bold_on_permille = r.get_int("bold_on_permille");
    t.bold_off_permille = r.get_int("bold_off_permille");
    t.bold_permille = r.get_int("bold_permille");
    t.bold_toll_permille = r.get_int("bold_toll_permille");
    t.outpost_permille = r.get_int("outpost_permille");
    t.outpost_max = r.get_int("outpost_max");
    t.outpost_quiet_years = r.get_int("outpost_quiet_years");
    const data::Record& w = r.get_record("prey_weight");
    t.prey_weight[1] = w.get_int("hamlet");
    t.prey_weight[2] = w.get_int("village");
    t.prey_weight[3] = w.get_int("town");
    t.prey_weight[4] = w.get_int("city");
    t.prey_weight[5] = w.get_int("fortress");
    t.prey_capital_permille = r.get_int("prey_capital_permille");
    t.prey_hop_permille = r.get_int("prey_hop_permille");
    t.prey_might_ref = r.get_int("prey_might_ref");
    t.prey_might_max_permille = r.get_int("prey_might_max_permille");
    // P5-03 9.1 to 9.4, 14.1.
    t.raid_chain_years = r.get_int("raid_chain_years");
    t.toll_over_permille = r.get_int("toll_over_permille");
    t.hunt_raid_permille = r.get_int("hunt_raid_permille");
    t.hunt_max_permille = r.get_int("hunt_max_permille");
    t.fear_raids = r.get_int("fear_raids");
    t.fear_years = r.get_int("fear_years");
    t.fear_permille = r.get_int("fear_permille");
    t.overrun_people_ref = r.get_int("overrun_people_ref");
    t.undead_dead_permille = r.get_int("undead_dead_permille");
    return t;
}

std::vector<BeastDef> beast_defs_from(const data::ContentSet& content) {
    std::vector<BeastDef> out;
    const data::KindContent* kind = content.find_kind(data::BEAST_KIND);
    if (kind == nullptr) {
        return out;
    }
    for (const data::Definition& d : kind->definitions) {  // canonical id order (P0-03 7.4)
        BeastDef b;
        b.id = static_cast<std::int64_t>(out.size()) + 1;
        b.key = d.id;
        b.name = d.record.get_string("name");
        b.plural = d.record.get_string("plural");
        b.habitat = habitat_list(d.record, "habitat");
        b.density = d.record.get_int("density");
        b.growth_permille = d.record.get_int("growth_permille");
        b.menace = d.record.get_int("menace");
        b.food = d.record.get_int("food");
        if (d.record.find("bold_phrases") != nullptr) {  // P5-03 13
            b.bold_phrases = string_list(d.record, "bold_phrases");
        }
        out.push_back(std::move(b));
    }
    return out;
}

std::vector<MonsterKindDef> monster_kind_defs_from(const data::ContentSet& content) {
    std::vector<MonsterKindDef> out;
    const data::KindContent* kind = content.find_kind(data::MONSTER_KIND);
    if (kind == nullptr) {
        return out;
    }
    for (const data::Definition& d : kind->definitions) {
        const data::Record& r = d.record;
        MonsterKindDef k;
        k.id = static_cast<std::int64_t>(out.size()) + 1;
        k.key = d.id;
        k.name = r.get_string("name");
        k.noun = r.get_string("noun");
        k.grouped = r.get_bool("grouped");
        k.wake = r.get_string("wake") == "dead" ? 2 : 1;
        k.biomes = habitat_list(r, "biomes");
        k.wake_rate = r.get_int("wake_rate");
        k.naming_culture = r.get_string("naming_culture");
        k.name_forms = string_list(r, "name_forms");
        k.lair_forms = string_list(r, "lair_forms");
        k.might = r.get_int("might");
        k.range_hops = r.get_int("range_hops");
        k.raid_permille = r.get_int("raid_permille");
        k.toll_min_permille = r.get_int("toll_min_permille");
        k.toll_max_permille = r.get_int("toll_max_permille");
        k.kill_max = r.get_int("kill_max");
        for (const std::string& o : string_list(r, "overruns")) {
            k.overruns.push_back(kind_of(o));
        }
        k.overrun_permille = r.get_int("overrun_permille");
        k.active_years = r.get_int("active_years");
        k.rewake_ppm = r.get_int("rewake_ppm");
        k.lifespan_years = r.get_int("lifespan_years");
        k.max_alive = r.get_int("max_alive");
        k.flare = r.get_bool("flare");
        k.hoard_permille = r.get_int("hoard_permille");
        if (r.find("raid_phrases") != nullptr) {  // P5-03 9.5
            k.raid_phrases = string_list(r, "raid_phrases");
        }
        out.push_back(std::move(k));
    }
    return out;
}

bool chance_ppb(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, PPB);
}

std::int64_t beast_year(std::int64_t b, std::int64_t k, std::int64_t growth_permille, std::int64_t decline_permille) {
    if (b < k) {
        const std::int64_t next =
            b == 0 ? 1
                   : b + std::max<std::int64_t>(1, floor_div(checked_mul(checked_mul(b, growth_permille), k - b),
                                                            checked_mul(1000, k)));
        return std::min(next, k);
    }
    if (b > k) {
        return b - std::max<std::int64_t>(1, floor_div(checked_mul(b - k, decline_permille), 1000));
    }
    return b;
}

// --- Setup ------------------------------------------------------------------------------------------

MonstersSubsystem::MonstersSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content)
    : MonstersSubsystem(peoples, monsters_tuning_from(content), beast_defs_from(content),
                        monster_kind_defs_from(content)) {}

MonstersSubsystem::MonstersSubsystem(PeoplesSubsystem& peoples, MonstersTuning tuning, std::vector<BeastDef> beasts,
                                     std::vector<MonsterKindDef> kinds)
    : peoples_(peoples), tuning_(std::move(tuning)), beasts_(std::move(beasts)), kinds_(std::move(kinds)) {}

std::span<const Step> MonstersSubsystem::steps() const {
    return STEPS;
}

void MonstersSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    const data::ContentSet& content = ctx.content();
    for (std::size_t i = 0; i < STREAM_NAMES.size(); ++i) {
        keys_[i] = stream_key(in.seed, STREAM_NAMES[i]);
    }
    name_key_ = stream_key(in.seed, streams::hist_names_monster);
    switch (in.params.monster_pressure) {
        case data::MonsterPressure::low: pressure_ = tuning_.pressure_low; break;
        case data::MonsterPressure::normal: pressure_ = tuning_.pressure_normal; break;
        case data::MonsterPressure::high: pressure_ = tuning_.pressure_high; break;
    }
    threshold_ = schedule_lengths(in.params.span, engine_tuning_from(content), in.params.detail).flare_threshold;
    const std::span<const GeoView::Region> regions = ctx.geo().regions();
    region_count_ = static_cast<std::int64_t>(regions.size());
    islet_.assign(regions.size(), 0);
    for (std::size_t i = 0; i < regions.size(); ++i) {
        islet_[i] = regions[i].islet ? 1 : 0;
        neighbours_.push_back(regions[i].neighbours);
    }
    living_cap_ = std::max<std::int64_t>(1, floor_div(checked_mul(region_count_, tuning_.living_permille), 1000));
    // Layers (the region layer is N x N u16, checked by the engine).
    map_size_ = in.map_size;
    const auto cells = static_cast<std::size_t>(map_size_ * map_size_);
    const std::vector<std::uint16_t>* region = in.region_layer.get_if<std::uint16_t>();
    const std::vector<std::uint8_t>* water = in.water_layer.get_if<std::uint8_t>();
    const std::vector<std::uint8_t>* hab = in.habitability_layer.get_if<std::uint8_t>();
    const std::vector<std::int16_t>* elev = in.elevation_layer.get_if<std::int16_t>();
    if (region == nullptr || water == nullptr || hab == nullptr || elev == nullptr || region->size() != cells ||
        water->size() != cells || hab->size() != cells || elev->size() != cells) {
        fail("history: monsters: the layers region, water, habitability and elevation are not " +
             int_text(map_size_) + " x " + int_text(map_size_) + " u16, u8, u8 and i16");
    }
    habitability_.resize(cells);
    elevation_ = *elev;
    land_cells_.assign(regions.size(), {});
    for (std::size_t i = 0; i < cells; ++i) {
        habitability_[i] = (*hab)[i];
        const std::int64_t r = (*region)[i];
        if (r >= 1 && r <= region_count_ && (*water)[i] == WATER_LAND) {
            land_cells_[static_cast<std::size_t>(r - 1)].push_back(static_cast<std::uint32_t>(i));
        }
    }
    // 6.2.1: habitat and capacity; the monster kinds' habitat likewise.
    std::vector<std::pair<std::int64_t, std::string>> biome_keys;  // (code, key)
    for (const db::BiomeRow& b : in.biomes) {
        biome_keys.emplace_back(b.id, b.key);
    }
    const auto key_of = [&biome_keys](std::int64_t code) -> std::string_view {
        for (const auto& [id, key] : biome_keys) {
            if (id == code) {
                return key;
            }
        }
        return {};
    };
    const std::size_t ns = beasts_.size();
    const std::size_t nk = kinds_.size();
    count_.assign(regions.size() * ns, 0);
    capacity0_.assign(regions.size() * ns, 0);
    last_row_.assign(regions.size() * ns, 0);
    habitat_kind_.assign(regions.size() * nk, 0);
    std::vector<std::int64_t> habitat_sum(regions.size() * ns, 0);
    for (const db::RegionBiomeRow& rb : in.region_biomes) {
        if (rb.region < 1 || rb.region > region_count_) {
            continue;
        }
        const std::string_view key = key_of(rb.biome);
        const auto base = static_cast<std::size_t>(rb.region - 1);
        for (std::size_t s = 0; s < ns; ++s) {
            habitat_sum[base * ns + s] =
                checked_add(habitat_sum[base * ns + s], checked_mul(rb.cells, weight_of(beasts_[s].habitat, key)));
        }
        for (std::size_t k = 0; k < nk; ++k) {
            habitat_kind_[base * nk + k] =
                checked_add(habitat_kind_[base * nk + k], checked_mul(rb.cells, weight_of(kinds_[k].biomes, key)));
        }
    }
    for (std::int64_t& h : habitat_kind_) {
        h = floor_div(h, 1000);
    }
    for (std::size_t r = 0; r < regions.size(); ++r) {
        for (std::size_t s = 0; s < ns; ++s) {
            const std::size_t i = r * ns + s;
            const std::int64_t h = floor_div(habitat_sum[i], 1000);
            const std::int64_t f = beasts_[s].menace > 0 ? pressure_ : 1000;
            const std::int64_t k0 = floor_div(checked_mul(checked_mul(h, beasts_[s].density), f), 10000 * 1000);
            if (k0 > MAX_BEAST_COUNT) {
                fail("history: bound: beast count exceeds 1000000");
            }
            capacity0_[i] = k0;
            count_[i] = k0;
        }
    }
    displaced_.assign(regions.size(), 0);
    growth_.assign(regions.size(), 1000);
    menace_.assign(regions.size(), 0);
    bold_.assign(regions.size(), 0);
    windows_.assign(regions.size(), {});
    lair_count_.assign(regions.size(), 0);
    shunned_count_.assign(regions.size(), 0);
    // Hunts: prowess by race.
    for (const db::RaceRow& r : in.races) {
        const data::Definition* d = content.find(data::RACE_KIND, r.key);
        prowess_.emplace_back(r.id, d != nullptr ? d->record.get_int("prowess_permille") : 1000);
    }
    // 9.1: the naming of each kind's culture with its name forms, and of each culture.
    for (const MonsterKindDef& k : kinds_) {
        const data::Definition* d = content.find(data::CULTURE_KIND, k.naming_culture);
        if (d == nullptr) {
            fail("history: monsters: culture '" + k.naming_culture + "' of monster '" + k.key +
                 "' is not in the content");
        }
        CultureNaming naming = culture_naming_from(d->record.get_record("naming"));
        naming.monster_forms = k.name_forms;
        kind_naming_.push_back(std::move(naming));
        std::int64_t culture_id = 0;
        for (const db::CultureRow& c : in.cultures) {
            culture_id = c.key == k.naming_culture ? c.id : culture_id;
        }
        kind_culture_id_.push_back(culture_id);
    }
    for (const db::CultureRow& c : in.cultures) {  // id order
        const data::Definition* d = content.find(data::CULTURE_KIND, c.key);
        if (d == nullptr) {
            fail("history: monsters: culture '" + c.key + "' is not in the content");
        }
        culture_naming_.emplace_back(c.id, culture_naming_from(d->record.get_record("naming")));
        culture_keys_.emplace_back(c.id, c.key);
    }
    placenames_ = stages::placename_data_from(content);
    prepared_ = true;
}

void MonstersSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    switch (ctx.step()) {
        case Step::t1: beasts(ctx, 1); break;
        case Step::t4: {
            read_log(ctx);
            beasts_bold(ctx);
            monsters_year(ctx);
            if (!rise(ctx)) {
                (void)wild_appearance(ctx);
            }
            break;
        }
        case Step::t6:
            outposts_left(ctx);
            outposts_raised(ctx);
            break;
        case Step::q1: beasts(ctx, ctx.years()); break;
        case Step::q4:
            pending_flare_ = 0;
            deaths_of_age_quiet(ctx);
            (void)wild_appearance(ctx);
            outposts_left(ctx);
            break;
        default: break;
    }
}

TensionReport MonstersSubsystem::tension(const StepContext&) {
    if (pending_flare_ == 0) {
        return TensionReport{};
    }
    return TensionReport{threshold_, pending_flare_};
}

std::int64_t MonstersSubsystem::growth_permille(std::int64_t region) const {
    if (region < 1 || static_cast<std::size_t>(region) > growth_.size()) {
        return 1000;
    }
    return growth_[static_cast<std::size_t>(region - 1)];
}

bool MonstersSubsystem::holds_site(std::int64_t settlement) const {
    return std::any_of(sites_.begin(), sites_.end(),
                       [settlement](const Site& t) { return t.kind != 3 && !t.ended_year && t.settlement == settlement; });
}

void MonstersSubsystem::open_site(const SiteSpec& spec) {
    if (spec.id != next_site_id() || spec.kind != 3) {
        fail("history: monsters: open_site: site " + int_text(spec.id) + " of kind " + int_text(spec.kind) +
             " is not the next site of kind 3");
    }
    Site t;
    t.id = spec.id;
    t.kind = spec.kind;
    t.name = spec.name;
    t.region = spec.region;
    t.x = spec.x;
    t.y = spec.y;
    t.settlement = spec.settlement;
    t.founder = spec.founder_civ;
    t.monster = spec.monster;
    t.origin_year = spec.origin_year;
    t.origin_event = spec.origin_event;
    sites_.push_back(std::move(t));
}

void MonstersSubsystem::end_site_seam(std::int64_t id, std::int64_t year, std::int64_t event, std::int64_t ending) {
    if (site(id).kind != 3 || site(id).ended_year) {
        fail("history: monsters: end_site: site " + int_text(id) + " is not an open mine");
    }
    end_site(id, year, event, ending);
}

bool MonstersSubsystem::shunned(std::int64_t region) const {
    if (region < 1 || static_cast<std::size_t>(region) > shunned_count_.size()) {
        return false;
    }
    return shunned_count_[static_cast<std::size_t>(region - 1)] > 0;
}

MonstersSubsystem::Monster& MonstersSubsystem::monster(std::int64_t id) {
    if (id < 1 || static_cast<std::size_t>(id) > monsters_.size()) {
        fail("history: monsters: monster " + int_text(id) + " does not exist");
    }
    return monsters_[static_cast<std::size_t>(id - 1)];
}

const MonstersSubsystem::Monster& MonstersSubsystem::monster(std::int64_t id) const {
    if (id < 1 || static_cast<std::size_t>(id) > monsters_.size()) {
        fail("history: monsters: monster " + int_text(id) + " does not exist");
    }
    return monsters_[static_cast<std::size_t>(id - 1)];
}

MonstersSubsystem::Site& MonstersSubsystem::site(std::int64_t id) {
    if (id < 1 || static_cast<std::size_t>(id) > sites_.size()) {
        fail("history: monsters: site " + int_text(id) + " does not exist");
    }
    return sites_[static_cast<std::size_t>(id - 1)];
}

std::string MonstersSubsystem::monster_root(std::int64_t id) const {
    const Monster& m = monster(id);
    return m.placed ? std::string() : m.root;  // P5-03 12.2: a host's root is its place, not a name
}

const std::string& MonstersSubsystem::monster_name(std::int64_t id) const {
    return monster(id).name;
}

std::int64_t MonstersSubsystem::lair_region(const Monster& m) const {
    return sites_[static_cast<std::size_t>(m.lair - 1)].region;
}

std::vector<std::int64_t> MonstersSubsystem::hops_from(std::int64_t region) const {
    // Breadth-first over the GeoView neighbours, ascending (P5-02 15).
    std::vector<std::int64_t> d(static_cast<std::size_t>(region_count_), -1);
    if (region < 1 || region > region_count_) {
        return d;
    }
    std::vector<std::int64_t> frontier{region};
    d[static_cast<std::size_t>(region - 1)] = 0;
    for (std::size_t i = 0; i < frontier.size(); ++i) {
        const std::int64_t r = frontier[i];
        for (const GeoView::Neighbour& nb : neighbours_[static_cast<std::size_t>(r - 1)]) {
            std::int64_t& dn = d[static_cast<std::size_t>(nb.region - 1)];
            if (dn < 0) {
                dn = d[static_cast<std::size_t>(r - 1)] + 1;
                frontier.push_back(nb.region);
            }
        }
    }
    return d;
}

// --- Beasts (6.2) -------------------------------------------------------------------------------

void MonstersSubsystem::write_beast_rows(std::int64_t year) {
    const std::size_t ns = beasts_.size();
    for (std::size_t r = 0; r < static_cast<std::size_t>(region_count_); ++r) {
        for (std::size_t s = 0; s < ns; ++s) {
            const std::size_t i = r * ns + s;
            if (count_[i] != last_row_[i]) {
                population_rows_.push_back(db::BeastPopulationRow{static_cast<std::int64_t>(r) + 1,
                                                                  static_cast<std::int64_t>(s) + 1, year, count_[i]});
                last_row_[i] = count_[i];
            }
        }
    }
}

void MonstersSubsystem::beasts(StepContext& ctx, std::int64_t years) {
    // Rows at a phase change: the counts at the end of the previous update.
    if (beasts_updated_ && ctx.phase_id() != beast_phase_) {
        write_beast_rows(beast_last_year_);
    }
    const State& state = ctx.state();
    const std::size_t nr = static_cast<std::size_t>(region_count_);
    std::vector<std::int64_t> people(nr, 0);
    for (std::int64_t id = 1; id <= settlement_count(state); ++id) {
        const Settlement& s = state.settlement(id);
        if (standing(s) && s.region >= 1 && s.region <= region_count_) {
            std::int64_t& p = people[static_cast<std::size_t>(s.region - 1)];
            p = checked_add(p, s.population);
        }
    }
    const std::size_t ns = beasts_.size();
    for (std::size_t r = 0; r < nr; ++r) {
        const std::int64_t d = std::min(tuning_.displace_max_permille,
                                        floor_div(checked_mul(people[r], tuning_.displace_per_kpeople), 1000));
        displaced_[r] = d;
        std::int64_t pred = 0;
        std::int64_t game = 0;
        for (std::size_t s = 0; s < ns; ++s) {
            const std::size_t i = r * ns + s;
            const std::int64_t k = floor_div(checked_mul(capacity0_[i], 1000 - d), 1000);
            std::int64_t b = count_[i];
            for (std::int64_t y = 0; y < years; ++y) {
                b = beast_year(b, k, beasts_[s].growth_permille, tuning_.beast_decline_permille);
            }
            if (b > MAX_BEAST_COUNT) {
                fail("history: bound: beast count exceeds 1000000");
            }
            count_[i] = b;
            pred = checked_add(pred, checked_mul(b, beasts_[s].menace));
            game = checked_add(game, checked_mul(b, beasts_[s].food));
        }
        const std::int64_t pd = std::max(people[r], tuning_.people_floor);
        const std::int64_t drag = std::min(tuning_.drag_max_permille, floor_div(checked_mul(pred, tuning_.drag_k), pd));
        const std::int64_t feed = std::min(tuning_.feed_max_permille, floor_div(checked_mul(game, tuning_.feed_k), pd));
        growth_[r] = 1000 - drag + feed;
        menace_[r] = floor_div(checked_mul(pred, 1000), pd);
    }
    beast_phase_ = ctx.phase_id();
    beast_last_year_ = ctx.year() + years - 1;
    if (!beasts_updated_) {
        write_beast_rows(beast_last_year_);  // after the very first update
        beasts_updated_ = true;
    }
}

void MonstersSubsystem::finish(std::int64_t end_year) {
    if (finished_ || !prepared_) {
        return;
    }
    write_beast_rows(end_year - 1);
    finished_ = true;
}

void MonstersSubsystem::beasts_bold(StepContext& ctx) {
    State& state = ctx.state();
    const std::int64_t n = settlement_count(state);
    // The standing visible settlement of the largest population (then the
    // smaller id) of each region.
    std::vector<std::int64_t> largest(static_cast<std::size_t>(region_count_), 0);
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        if (!standing(s) || !ctx.visible(s) || s.region < 1 || s.region > region_count_) {
            continue;
        }
        std::int64_t& t = largest[static_cast<std::size_t>(s.region - 1)];
        if (t == 0 || s.population > state.settlement(t).population) {
            t = id;
        }
    }
    const std::size_t ns = beasts_.size();
    for (std::int64_t r = 1; r <= region_count_; ++r) {
        const auto ri = static_cast<std::size_t>(r - 1);
        const std::int64_t t = largest[ri];
        if (t == 0) {
            continue;
        }
        if (bold_[ri] != 0) {
            if (menace_[ri] < tuning_.bold_off_permille) {
                bold_[ri] = 0;
            }
            continue;
        }
        if (menace_[ri] < tuning_.bold_on_permille ||
            !chance_pm(ctx.draw(key(BOLD), word(r), 0, 0), tuning_.bold_permille)) {
            continue;
        }
        const Settlement& s = state.settlement(t);
        const std::int64_t p = s.population;
        const std::int64_t dead = std::max<std::int64_t>(
            0, std::min(p - 1, std::max<std::int64_t>(1, floor_div(checked_mul(p, tuning_.bold_toll_permille), 1000))));
        // P5-03 13: the species by a weighted draw of count x menace.
        std::vector<std::int64_t> weights(ns, 0);
        for (std::size_t sp = 0; sp < ns; ++sp) {
            weights[sp] = checked_mul(count_[ri * ns + sp], beasts_[sp].menace);
        }
        const std::size_t best = weighted(ctx.draw(key(BOLD_KIND), word(r), 0, 0), weights);
        const std::int64_t owner = s.owner_civ;
        EventSpec e = point_event(ctx, "beasts_bold", r, t);
        e.participants = {Participant{"settlement", EntityType::settlement, t},
                          Participant{"civilization", EntityType::civilization, owner}};
        e.values = {beasts_[best].id, dead};
        const std::int64_t ev = ctx.append(e);
        if (dead > 0) {
            state.set_population(t, p - dead);
        }
        peoples_.note_reduction(t, ev);
        bold_[ri] = 1;
        ++stats_.bold;
    }
}

// --- Appearances (6.3) ------------------------------------------------------------------------------

std::int64_t MonstersSubsystem::living() const {
    std::int64_t n = 0;
    for (const Monster& m : monsters_) {
        n += (m.status == MonsterStatus::awake || m.status == MonsterStatus::asleep) ? 1 : 0;
    }
    return n;
}

std::int64_t MonstersSubsystem::living_of(std::size_t kind) const {
    std::int64_t n = 0;
    for (const Monster& m : monsters_) {
        n += (m.kind == kind && (m.status == MonsterStatus::awake || m.status == MonsterStatus::asleep)) ? 1 : 0;
    }
    return n;
}

bool MonstersSubsystem::has_lair(std::int64_t region) const {
    return lair_count_[static_cast<std::size_t>(region - 1)] > 0;
}

bool MonstersSubsystem::caps_allow(std::size_t kind, std::int64_t region) const {
    return living() < living_cap_ && living_of(kind) < kinds_[kind].max_alive && !has_lair(region);
}

void MonstersSubsystem::read_log(const StepContext& ctx) {
    const std::int64_t last = ctx.last_event_id();
    for (std::int64_t e = cursor_; e <= last; ++e) {
        const std::string_view kind = ctx.event_kind(e);
        const std::int64_t r = ctx.event_region(e);
        if (r < 1 || r > region_count_) {
            continue;
        }
        std::int64_t dead = -1;
        if (kind == "battle" || kind == "battle_at_pass") {
            dead = checked_add(ctx.event_value(e, "winner_dead"), ctx.event_value(e, "loser_dead"));
        } else if (kind == "sacked") {
            dead = ctx.event_value(e, "dead");
        }
        if (dead >= 0) {
            windows_[static_cast<std::size_t>(r - 1)].push_back(WindowEntry{ctx.event_year(e), dead, e});
        }
    }
    cursor_ = last + 1;
    const std::int64_t oldest = ctx.year() - tuning_.undead_window_years;  // entries with year <= oldest drop
    for (std::vector<WindowEntry>& w : windows_) {
        std::size_t drop = 0;
        while (drop < w.size() && w[drop].year <= oldest) {
            ++drop;
        }
        w.erase(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(drop));
    }
}

bool MonstersSubsystem::rise(StepContext& ctx) {
    // P5-03a 6: T = max(undead_min_dead, P_W x undead_dead_permille div
    // 1000), P_W the people of the standing settlements as the test is made.
    std::int64_t world_people = 0;
    for (const Settlement& s : ctx.state().settlements()) {
        if (standing(s)) {
            world_people = checked_add(world_people, s.population);
        }
    }
    const std::int64_t threshold = std::max(
        tuning_.undead_min_dead, floor_div(checked_mul(world_people, tuning_.undead_dead_permille), 1000));
    const std::int64_t need = floor_div(checked_mul(threshold, 1000), std::max<std::int64_t>(1, pressure_));
    for (std::int64_t r = 1; r <= region_count_; ++r) {
        const std::vector<WindowEntry>& w = windows_[static_cast<std::size_t>(r - 1)];
        std::int64_t sum = 0;
        for (const WindowEntry& x : w) {
            sum = checked_add(sum, x.dead);
        }
        // P5-03 14.2: the threshold falls with the pressure.
        if (sum < need) {
            continue;
        }
        for (std::size_t k = 0; k < kinds_.size(); ++k) {
            if (kinds_[k].wake != 2 || !caps_allow(k, r)) {
                continue;
            }
            if (chance_ppm(ctx.draw(key(RISE), word(r), word(kinds_[k].id), 0),
                           floor_div(checked_mul(kinds_[k].wake_rate, pressure_), 1000))) {
                create(ctx, k, r, true);
                return true;
            }
        }
    }
    return false;
}

bool MonstersSubsystem::wild_appearance(StepContext& ctx) {
    if (ctx.step() == Step::q4 && ctx.quiet_events_left(0) <= 0) {
        return false;
    }
    const std::int64_t years = ctx.years();
    const std::size_t nk = kinds_.size();
    for (std::size_t k = 0; k < nk; ++k) {
        const MonsterKindDef& def = kinds_[k];
        if (def.wake != 1) {
            continue;
        }
        for (std::int64_t r = 1; r <= region_count_; ++r) {
            const auto ri = static_cast<std::size_t>(r - 1);
            const std::int64_t h = habitat_kind_[ri * nk + k];
            if (islet_[ri] != 0 || h <= 0 || !caps_allow(k, r)) {
                continue;
            }
            std::int64_t p = floor_div(checked_mul(def.wake_rate, h), 10);
            p = floor_div(checked_mul(p, pressure_), 1000);
            p = floor_div(checked_mul(p, 1000 - displaced_[ri]), 1000);
            p = checked_mul(p, years);
            if (chance_ppb(ctx.draw(key(WAKE), word(r), word(def.id), 0), std::min(p, PPB))) {
                create(ctx, k, r, false);
                return true;
            }
        }
    }
    return false;
}

void MonstersSubsystem::create(StepContext& ctx, std::size_t k, std::int64_t region, bool risen) {
    const MonsterKindDef& def = kinds_[k];
    const State& state = ctx.state();
    const auto m = static_cast<std::int64_t>(monsters_.size()) + 1;
    const auto s = static_cast<std::int64_t>(sites_.size()) + 1;
    if (m > MAX_MONSTERS) {
        fail("history: bound: monsters exceed 100000");
    }
    if (s > MAX_SITES) {
        fail("history: bound: sites exceed 1000000");
    }
    // 9.1: the name, then the root of 9.2.
    Monster mon;
    mon.id = m;
    mon.kind = k;
    // The lair's cell (6.3.2: at a ruined sacked settlement, else the wild cell).
    std::int64_t lair_settlement = 0;
    std::vector<std::int64_t> causes;
    if (risen) {
        std::vector<WindowEntry> w = windows_[static_cast<std::size_t>(region - 1)];
        std::stable_sort(w.begin(), w.end(), [](const WindowEntry& a, const WindowEntry& b) {
            return a.dead != b.dead ? a.dead > b.dead : a.event < b.event;
        });
        if (!w.empty() && ctx.event_kind(w.front().event) == "sacked") {
            const std::int64_t x = ctx.event_settlement(w.front().event);
            const bool dead_place = x != 0 && (state.settlement(x).status == SettlementStatus::ruined ||
                                               state.settlement(x).status == SettlementStatus::faded);
            const bool occupied = std::any_of(sites_.begin(), sites_.end(), [x](const Site& t) {
                return t.kind != 3 && !t.ended_year && t.settlement == x;
            });
            if (dead_place && !occupied) {
                lair_settlement = x;
            }
        }
        for (std::size_t i = 0; i < w.size() && i < 8; ++i) {
            causes.push_back(w[i].event);
        }
        std::stable_sort(causes.begin(), causes.end());
    }
    // 9.1: the name, then the root of 9.2 (P5-03 12.2: a dead kind is named
    // for its place, after the lair's settlement is known).
    if (def.wake == 2) {
        mon.name = place_name(ctx, k, m, region, lair_settlement, mon.root);
        mon.placed = true;
    } else {
        mon.name = wild_name(ctx, k, m);
        mon.root = realm_root(mon.name, def.name_forms);
    }
    Cell cell;
    if (lair_settlement != 0) {
        cell = Cell{state.settlement(lair_settlement).x, state.settlement(lair_settlement).y};
    } else {
        cell = *wild_cell(ctx, region, false);
    }
    std::string lair = lair_name(ctx, s, k, mon.root);
    EventSpec e = point_event(ctx, risen ? "undead_rose" : "monster_appeared", region, lair_settlement);
    e.participants = {Participant{"monster", EntityType::monster, m}, Participant{"lair", EntityType::site, s}};
    e.causes = std::move(causes);
    const std::int64_t ev = ctx.append(e);
    Site site_row;
    site_row.id = s;
    site_row.kind = 1;
    site_row.name = std::move(lair);
    site_row.region = region;
    site_row.x = cell.x;
    site_row.y = cell.y;
    site_row.settlement = lair_settlement;
    site_row.monster = m;
    site_row.origin_year = ctx.year();
    site_row.origin_event = ev;
    sites_.push_back(std::move(site_row));
    ++lair_count_[static_cast<std::size_t>(region - 1)];
    if (lair_settlement != 0) {
        ++shunned_count_[static_cast<std::size_t>(region - 1)];
    }
    mon.status = MonsterStatus::awake;
    mon.born = ctx.year();
    mon.born_event = ev;
    mon.lair = s;
    mon.hops = hops_from(region);
    mon.waking_event = ev;
    monsters_.push_back(std::move(mon));
    if (risen) {
        windows_[static_cast<std::size_t>(region - 1)].clear();
    }
    if (ctx.step() == Step::q4 && def.flare) {
        pending_flare_ = ev;
    }
}

// --- The monsters' year (6.4) -------------------------------------------------------------------------

void MonstersSubsystem::end_site(std::int64_t id, std::int64_t year, std::int64_t event, std::int64_t ending) {
    Site& t = site(id);
    t.ended_year = year;
    t.ending_event = event;
    t.ending = ending;
    const auto ri = static_cast<std::size_t>(t.region - 1);
    if (t.kind == 1) {
        --lair_count_[ri];
        if (t.settlement != 0) {
            --shunned_count_[ri];
        }
    }
}

void MonstersSubsystem::die_of_age(StepContext& ctx, std::int64_t id) {
    Monster& m0 = monster(id);
    EventSpec e = point_event(ctx, "monster_died", lair_region(m0), 0);
    e.participants = {Participant{"monster", EntityType::monster, id}};
    e.causes = {m0.born_event};
    const std::int64_t ev = ctx.append(e);
    Monster& m = monster(id);
    m.status = MonsterStatus::died;
    m.died = ctx.year();
    m.death_event = ev;
    end_site(m.lair, ctx.year(), ev, 2);
}

void MonstersSubsystem::monsters_year(StepContext& ctx) {
    const auto n = static_cast<std::int64_t>(monsters_.size());
    const std::int64_t y = ctx.year();
    for (std::int64_t id = 1; id <= n; ++id) {
        const Monster& m = monster(id);
        if (m.status == MonsterStatus::slain || m.status == MonsterStatus::died) {
            continue;
        }
        const MonsterKindDef& def = kinds_[m.kind];
        // 1. Age.
        if (def.lifespan_years > 0 && y >= checked_add(m.born, def.lifespan_years)) {
            die_of_age(ctx, id);
            continue;
        }
        // 2. Asleep.
        if (m.status == MonsterStatus::asleep) {
            if (y >= m.drive_until &&
                chance_ppm(ctx.draw(key(REWAKE), word(id), 0, 0),
                           floor_div(checked_mul(def.rewake_ppm, pressure_), 1000))) {
                EventSpec e = point_event(ctx, "monster_woke", lair_region(m), 0);
                e.participants = {Participant{"monster", EntityType::monster, id}};
                e.causes = {m.sleeping_event};
                const std::int64_t ev = ctx.append(e);
                Monster& w = monster(id);
                w.status = MonsterStatus::awake;
                w.waking_event = ev;
            }
            continue;
        }
        // 3. Sleep.
        if (chance_pm(ctx.draw(key(SLEEP), word(id), 0, 0), std::max<std::int64_t>(1, 1000 / def.active_years))) {
            EventSpec e = point_event(ctx, "monster_slept", lair_region(m), 0);
            e.participants = {Participant{"monster", EntityType::monster, id},
                              Participant{"lair", EntityType::site, m.lair}};
            e.causes = {m.waking_event};
            const std::int64_t ev = ctx.append(e);
            Monster& w = monster(id);
            w.status = MonsterStatus::asleep;
            w.sleeping_event = ev;
            continue;
        }
        // 4. Raid.
        if (chance_pm(ctx.draw(key(RAID), word(id), 0, 0),
                      std::min<std::int64_t>(1000, floor_div(checked_mul(def.raid_permille, pressure_), 1000)))) {
            raid(ctx, id);
        }
        // 5. Hunts.
        if (monster(id).status == MonsterStatus::awake) {
            hunts(ctx, id);
        }
    }
}

// 6.4.1 (r1): the weight of a candidate at h hops, before the capital factor.
std::int64_t MonstersSubsystem::prey_weight(SettlementKind kind, std::int64_t might, std::int64_t hops) const {
    const auto code = static_cast<std::size_t>(kind);
    if (code < 1 || code >= tuning_.prey_weight.size()) {
        fail("history: monsters: a settlement of unknown kind");
    }
    std::int64_t base = tuning_.prey_weight[code];
    if (kind == SettlementKind::town || kind == SettlementKind::city || kind == SettlementKind::fortress) {
        const std::int64_t might_f = std::min(tuning_.prey_might_max_permille,
                                              floor_div(checked_mul(might, 1000), tuning_.prey_might_ref));
        base = floor_div(checked_mul(base, might_f), 1000);
    }
    std::int64_t hop_f = 1000;
    for (std::int64_t h = 0; h < hops; ++h) {
        hop_f = floor_div(checked_mul(hop_f, tuning_.prey_hop_permille), 1000);
    }
    return floor_div(checked_mul(base, hop_f), 1000);
}

// 6.4.1 (r1): a keyed weighted draw over the candidates in ascending id; 0
// (and no draw) when there is no candidate or every weight is 0.
std::int64_t MonstersSubsystem::raid_target(StepContext& ctx, std::int64_t id) const {
    const State& state = ctx.state();
    const MonsterKindDef& def = kinds_[monster(id).kind];
    std::vector<std::int64_t> ids;
    std::vector<std::int64_t> weights;
    std::int64_t total = 0;
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t sid = 1; sid <= ns; ++sid) {
        const Settlement& s = state.settlement(sid);
        if (!standing(s) || !ctx.visible(s) || s.owner_civ < 1 || s.region < 1 || s.region > region_count_) {
            continue;
        }
        const std::int64_t h = monster(id).hops[static_cast<std::size_t>(s.region - 1)];
        if (h < 0 || h > def.range_hops) {
            continue;
        }
        std::int64_t w = prey_weight(s.kind, def.might, h);
        if (state.civ(s.owner_civ).capital == sid) {
            w = floor_div(checked_mul(w, tuning_.prey_capital_permille), 1000);
        }
        ids.push_back(sid);
        weights.push_back(w);
        total = checked_add(total, w);
    }
    if (ids.empty() || total == 0) {
        return 0;
    }
    const std::size_t i = weighted(ctx.draw(key(RAID_TARGET), word(id), 0, 0), weights);
    return ids[i];
}

std::vector<MonstersSubsystem::PlaceRaid> MonstersSubsystem::place_raids(std::int64_t id,
                                                                         std::int64_t settlement) const {
    const Monster& m = monster(id);
    const auto it = std::lower_bound(m.places.begin(), m.places.end(), settlement,
                                     [](const PlaceMemory& p, std::int64_t s) { return p.settlement < s; });
    if (it == m.places.end() || it->settlement != settlement) {
        return {};
    }
    return it->raids;
}

void MonstersSubsystem::raid(StepContext& ctx, std::int64_t id) {
    State& state = ctx.state();
    const MonsterKindDef& def = kinds_[monster(id).kind];
    const std::int64_t target = raid_target(ctx, id);
    if (target == 0) {
        return;
    }
    const std::int64_t y = ctx.year();
    const Settlement& t = state.settlement(target);
    const std::int64_t p = t.population;
    const std::int64_t c = t.owner_civ;
    const std::int64_t tau =
        def.toll_min_permille +
        static_cast<std::int64_t>(pick(ctx.draw(key(RAID_TOLL), word(id), 0, 0),
                                       word(def.toll_max_permille - def.toll_min_permille + 1)));
    // P5-03 9.2: above kill_max the toll grows at toll_over_permille.
    const std::int64_t raw = floor_div(checked_mul(p, tau), 1000);
    std::int64_t dead = raw;
    if (raw > def.kill_max) {
        dead = checked_add(def.kill_max, floor_div(checked_mul(raw - def.kill_max, tuning_.toll_over_permille), 1000));
    }
    dead = std::max<std::int64_t>(0, std::min(dead, p - 1));
    const std::int64_t plunder = floor_div(checked_mul(p, def.hoard_permille), 1000);
    // P5-03 9.1: the waking event and the last raid on t within raid_chain_years.
    std::vector<std::int64_t> causes{monster(id).waking_event};
    const std::vector<PlaceRaid> before = place_raids(id, target);
    if (!before.empty() && y - before.back().year < tuning_.raid_chain_years &&
        before.back().event != causes.front()) {
        causes.push_back(before.back().event);
        std::stable_sort(causes.begin(), causes.end());
    }
    EventSpec e = point_event(ctx, "monster_raid", t.region, target);
    e.participants = {Participant{"monster", EntityType::monster, id},
                      Participant{"civilization", EntityType::civilization, c}};
    e.values = {dead, plunder};
    e.causes = std::move(causes);
    const std::int64_t ev = ctx.append(e);
    if (dead > 0) {
        state.set_population(target, p - dead);
    }
    peoples_.note_reduction(target, ev);
    Monster& m = monster(id);
    m.hoard = checked_add(m.hoard, plunder);
    // The raid memory per civilization (6.4.2, with P5-03 9.3's years) and
    // per settlement (P5-03 9.1, 9.4); entries older than the memories drop.
    auto it = std::lower_bound(m.raids.begin(), m.raids.end(), c,
                               [](const RaidMemory& r, std::int64_t civ) { return r.civ < civ; });
    if (it == m.raids.end() || it->civ != c) {
        it = m.raids.insert(it, RaidMemory{c, 0, 0, {}});
    }
    it->year = y;
    it->event = ev;
    it->years.push_back(y);
    std::size_t old = 0;
    while (old < it->years.size() && y - it->years[old] > tuning_.hunt_memory_years) {
        ++old;
    }
    it->years.erase(it->years.begin(), it->years.begin() + static_cast<std::ptrdiff_t>(old));
    const std::int64_t keep = std::max(tuning_.raid_chain_years, tuning_.fear_years);
    auto pl = std::lower_bound(m.places.begin(), m.places.end(), target,
                               [](const PlaceMemory& q, std::int64_t s) { return q.settlement < s; });
    if (pl == m.places.end() || pl->settlement != target) {
        pl = m.places.insert(pl, PlaceMemory{target, {}});
    }
    std::vector<PlaceRaid>& raids = pl->raids;
    raids.push_back(PlaceRaid{y, ev});
    std::size_t drop = 0;
    while (drop < raids.size() && y - raids[drop].year >= keep) {
        ++drop;
    }
    raids.erase(raids.begin(), raids.begin() + static_cast<std::ptrdiff_t>(drop));
    ++stats_.raids;
    overrun(ctx, id, target, ev);
    // P5-03 9.4: a place still standing may be left for fear.
    const Settlement& after = state.settlement(target);
    if (standing(after)) {
        flight(ctx, id, target);
    }
}

void MonstersSubsystem::flight(StepContext& ctx, std::int64_t id, std::int64_t target) {
    const State& state = ctx.state();
    const Settlement& t = state.settlement(target);
    if (t.kind != SettlementKind::hamlet && t.kind != SettlementKind::village) {
        return;
    }
    const std::int64_t c = t.owner_civ;
    if (c < 1 || state.civ(c).capital == target) {
        return;
    }
    const std::int64_t y = ctx.year();
    std::vector<std::int64_t> recent;  // the raids on t within fear_years, ascending
    for (const PlaceRaid& r : place_raids(id, target)) {
        if (y - r.year < tuning_.fear_years) {
            recent.push_back(r.event);
        }
    }
    if (static_cast<std::int64_t>(recent.size()) < tuning_.fear_raids) {
        return;
    }
    bool other = false;
    for (std::int64_t sid = 1; sid <= settlement_count(state) && !other; ++sid) {
        const Settlement& u = state.settlement(sid);
        other = sid != target && standing(u) && u.owner_civ == c;
    }
    if (!other || !chance_pm(ctx.draw(key(FEAR), word(id), word(target), 0), tuning_.fear_permille)) {
        return;
    }
    EventSpec e = point_event(ctx, "settlement_fled", t.region, target);
    e.participants = {Participant{"monster", EntityType::monster, id},
                      Participant{"civilization", EntityType::civilization, c}};
    const std::size_t from = recent.size() > 4U ? recent.size() - 4U : 0U;
    e.causes.assign(recent.begin() + static_cast<std::ptrdiff_t>(from), recent.end());
    const std::int64_t ev = ctx.append(e);
    peoples_.abandon(ctx, target, ev);
    ++stats_.fled;
}

void MonstersSubsystem::overrun(StepContext& ctx, std::int64_t id, std::int64_t target, std::int64_t raid_event) {
    const State& state = ctx.state();
    const Monster& m = monster(id);
    const MonsterKindDef& def = kinds_[m.kind];
    const Settlement& t = state.settlement(target);
    if (m.holds_settlement || std::find(def.overruns.begin(), def.overruns.end(), t.kind) == def.overruns.end()) {
        return;
    }
    const std::int64_t c = t.owner_civ;
    if (state.civ(c).capital == target) {
        return;
    }
    bool other = false;
    for (std::int64_t sid = 1; sid <= settlement_count(state) && !other; ++sid) {
        const Settlement& u = state.settlement(sid);
        other = sid != target && standing(u) && u.owner_civ == c;
    }
    if (!other) {
        return;
    }
    const std::int64_t region = t.region;
    const bool other_lair = std::any_of(sites_.begin(), sites_.end(), [region, id](const Site& x) {
        return x.kind == 1 && !x.ended_year && x.region == region && x.monster != id;
    });
    // P5-03 14.1, 14.2: the chance scales with the pressure and falls for
    // a place of more than overrun_people_ref people (after the raid).
    const std::int64_t base = std::min<std::int64_t>(1000, floor_div(checked_mul(def.overrun_permille, pressure_), 1000));
    const std::int64_t f_p = std::min<std::int64_t>(
        1000, floor_div(checked_mul(tuning_.overrun_people_ref, 1000), std::max<std::int64_t>(1, t.population)));
    if (other_lair ||
        !chance_pm(ctx.draw(key(OVERRUN), word(id), word(target), 0), floor_div(checked_mul(base, f_p), 1000))) {
        return;
    }
    // 1. The new lair site.
    const auto s = static_cast<std::int64_t>(sites_.size()) + 1;
    if (s > MAX_SITES) {
        fail("history: bound: sites exceed 1000000");
    }
    std::string lair = lair_name(ctx, s, m.kind, m.root);
    const std::int64_t tx = t.x;
    const std::int64_t ty = t.y;
    const std::int64_t survivors = t.population;
    // 2. The event.
    EventSpec e = point_event(ctx, "settlement_overrun", region, target);
    e.participants = {Participant{"monster", EntityType::monster, id},
                      Participant{"civilization", EntityType::civilization, c},
                      Participant{"lair", EntityType::site, s}};
    e.values = {survivors};
    e.causes = {raid_event};
    const std::int64_t ev = ctx.append(e);
    Site x;
    x.id = s;
    x.kind = 1;
    x.name = std::move(lair);
    x.region = region;
    x.x = tx;
    x.y = ty;
    x.settlement = target;
    x.monster = id;
    x.origin_year = ctx.year();
    x.origin_event = ev;
    sites_.push_back(std::move(x));
    ++lair_count_[static_cast<std::size_t>(region - 1)];
    ++shunned_count_[static_cast<std::size_t>(region - 1)];
    // 3. The ruin (P3-05 8.5 items 2 to 5).
    peoples_.ruin(ctx, target, ev);
    // 4. The old lair ends; the monster holds the settlement.
    Monster& w = monster(id);
    end_site(w.lair, ctx.year(), ev, 3);
    w.lair = s;
    w.holds_settlement = true;
    w.hops = hops_from(region);
    ++stats_.overruns;
}

void MonstersSubsystem::hunts(StepContext& ctx, std::int64_t id) {
    State& state = ctx.state();
    const std::vector<RaidMemory> raids = monster(id).raids;  // a copy: the loop may end the monster
    const MonsterKindDef& def = kinds_[monster(id).kind];
    for (const RaidMemory& r : raids) {
        const std::int64_t c = r.civ;
        if (ctx.year() - r.year > tuning_.hunt_memory_years || c < 1 || c > civ_count(state) ||
            state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        // P5-03 9.3: the raids on c still remembered (the loop's window, so
        // at least one), and the chance they raise.
        std::int64_t r_c = 0;
        for (const std::int64_t year : r.years) {
            r_c += ctx.year() - year <= tuning_.hunt_memory_years ? 1 : 0;
        }
        const std::int64_t p_hunt = std::min(
            tuning_.hunt_max_permille,
            checked_add(tuning_.hunt_permille, checked_mul(std::max<std::int64_t>(0, r_c - 1), tuning_.hunt_raid_permille)));
        std::int64_t hunters = 0;
        for (std::int64_t sid = 1; sid <= settlement_count(state); ++sid) {
            const Settlement& u = state.settlement(sid);
            if (!standing(u) || !ctx.visible(u) || u.owner_civ != c || u.region < 1 || u.region > region_count_) {
                continue;
            }
            const std::int64_t h = monster(id).hops[static_cast<std::size_t>(u.region - 1)];
            if (h >= 0 && h <= tuning_.hunt_hops) {
                hunters = checked_add(hunters, floor_div(checked_mul(u.population, tuning_.hunt_levy_permille), 1000));
            }
        }
        std::int64_t prowess = 1000;
        for (const auto& [race, value] : prowess_) {
            prowess = race == state.civ(c).race ? value : prowess;
        }
        const std::int64_t strength = floor_div(checked_mul(hunters, prowess), 1000);
        if (strength == 0) {
            continue;
        }
        if (!chance_pm(ctx.draw(key(HUNT), word(id), word(c), 0), p_hunt)) {
            continue;
        }
        const std::int64_t odds = std::min(tuning_.slay_max_permille,
                                           floor_div(checked_mul(strength, 1000), checked_add(strength, def.might)));
        const auto u = static_cast<std::int64_t>(pick(ctx.draw(key(HUNT_OUTCOME), word(id), word(c), 0), 1000));
        const std::int64_t region = lair_region(monster(id));
        if (u < odds) {
            EventSpec e = point_event(ctx, "monster_slain", region, 0);
            e.participants = {Participant{"monster", EntityType::monster, id},
                              Participant{"slayer", EntityType::civilization, c}};
            e.values = {hunters};
            e.causes = {r.event};
            const std::int64_t ev = ctx.append(e);
            Monster& m = monster(id);
            m.status = MonsterStatus::slain;
            m.died = ctx.year();
            m.death_event = ev;
            m.slayer = c;
            end_site(m.lair, ctx.year(), ev, 1);
            return;
        }
        if (u < odds + floor_div(checked_mul(1000 - odds, tuning_.drive_permille), 1000)) {
            EventSpec e = point_event(ctx, "monster_driven_off", region, 0);
            e.participants = {Participant{"monster", EntityType::monster, id},
                              Participant{"civilization", EntityType::civilization, c}};
            e.causes = {r.event};
            const std::int64_t ev = ctx.append(e);
            Monster& m = monster(id);
            m.status = MonsterStatus::asleep;
            m.sleeping_event = ev;
            m.drive_until = checked_add(ctx.year(), tuning_.drive_years);
            return;
        }
    }
}

void MonstersSubsystem::deaths_of_age_quiet(StepContext& ctx) {
    const auto n = static_cast<std::int64_t>(monsters_.size());
    const std::int64_t last = ctx.year() + ctx.years() - 1;
    for (std::int64_t id = 1; id <= n; ++id) {
        const Monster& m = monster(id);
        if (m.status == MonsterStatus::slain || m.status == MonsterStatus::died) {
            continue;
        }
        const MonsterKindDef& def = kinds_[m.kind];
        if (def.lifespan_years <= 0 || checked_add(m.born, def.lifespan_years) > last) {
            continue;
        }
        if (ctx.quiet_events_left(0) <= 0) {
            continue;  // it waits for the next step
        }
        die_of_age(ctx, id);
    }
}

// --- Sites (6.6) ----------------------------------------------------------------------------------------

std::optional<MonstersSubsystem::Cell> MonstersSubsystem::wild_cell(const StepContext& ctx, std::int64_t region,
                                                                    bool height) const {
    const State& state = ctx.state();
    const std::int64_t n = map_size_;
    std::vector<std::uint32_t> taken;
    for (std::int64_t id = 1; id <= settlement_count(state); ++id) {
        const Settlement& s = state.settlement(id);
        if (s.region == region) {
            taken.push_back(static_cast<std::uint32_t>(s.y * n + s.x));
        }
    }
    for (const Site& t : sites_) {
        if (!t.ended_year && t.region == region) {
            taken.push_back(static_cast<std::uint32_t>(t.y * n + t.x));
        }
    }
    std::stable_sort(taken.begin(), taken.end());
    std::optional<std::uint32_t> best;
    for (const std::uint32_t c : land_cells_[static_cast<std::size_t>(region - 1)]) {  // row-major: y, then x
        if (std::binary_search(taken.begin(), taken.end(), c)) {
            continue;
        }
        if (!best) {
            best = c;
            continue;
        }
        const std::int64_t he = elevation_[c];
        const std::int64_t hb = elevation_[*best];
        if (height) {
            if (he > hb) {
                best = c;  // ties keep the earlier cell (smaller y, then x)
            }
        } else {
            const std::int64_t ae = habitability_[c];
            const std::int64_t ab = habitability_[*best];
            if (ae < ab || (ae == ab && he > hb)) {
                best = c;
            }
        }
    }
    if (!best) {
        if (height) {
            return std::nullopt;
        }
        const db::RegionRow& row = ctx.inputs().regions[static_cast<std::size_t>(region - 1)];
        return Cell{row.seed_x, row.seed_y};
    }
    return Cell{static_cast<std::int64_t>(*best) % n, static_cast<std::int64_t>(*best) / n};
}

void MonstersSubsystem::outposts_left(StepContext& ctx) {
    const State& state = ctx.state();
    const bool in_quiet = quiet(ctx);
    const auto n = static_cast<std::int64_t>(sites_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Site& t = site(id);
        if (t.kind != 2 || t.ended_year) {
            continue;
        }
        const std::int64_t c = t.founder;
        const std::int64_t region = t.region;
        for (const GeoView::Neighbour& nb : neighbours_[static_cast<std::size_t>(region - 1)]) {
            const std::int64_t o = state.region(nb.region).owner;
            if (o != 0 && o != c && hostile(state, c, o)) {
                site(id).hostile_year = ctx.year();
                break;
            }
        }
        std::int64_t reason = -1;
        std::int64_t cause = 0;
        std::int64_t ending = 0;
        if (state.civ(c).ended_year) {
            reason = 2;
            cause = state.civ(c).ending_event;
            ending = 5;
        } else if (state.region(region).owner != c) {
            reason = 1;
            cause = state.region(region).owner_event;
            ending = 4;
        } else if (ctx.year() - site(id).hostile_year >= tuning_.outpost_quiet_years) {
            reason = 0;
            ending = 6;
        }
        if (reason < 0) {
            continue;
        }
        if (in_quiet && ctx.quiet_events_left(c) <= 0) {
            continue;  // it waits (W9)
        }
        EventSpec e = point_event(ctx, "outpost_abandoned", region, 0);
        e.participants = {Participant{"civilization", EntityType::civilization, c},
                          Participant{"site", EntityType::site, id}};
        e.values = {reason};
        if (cause != 0) {
            e.causes = {cause};
        }
        const std::int64_t ev = ctx.append(e);
        end_site(id, ctx.year(), ev, ending);
    }
}

void MonstersSubsystem::outposts_raised(StepContext& ctx) {
    const State& state = ctx.state();
    const std::int64_t nc = civ_count(state);
    // The open outposts per region and per founder, kept up to date below.
    std::vector<std::uint8_t> outpost_in(static_cast<std::size_t>(region_count_), 0);
    std::vector<std::int64_t> open_of(static_cast<std::size_t>(nc) + 1, 0);
    for (const Site& t : sites_) {
        if (t.kind == 2 && !t.ended_year) {
            outpost_in[static_cast<std::size_t>(t.region - 1)] = 1;
            if (t.founder >= 1 && t.founder <= nc) {
                ++open_of[static_cast<std::size_t>(t.founder)];
            }
        }
    }
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        if (open_of[static_cast<std::size_t>(c)] >= tuning_.outpost_max) {
            continue;
        }
        std::vector<std::int64_t> enemies;  // ascending
        for (std::int64_t e = 1; e <= civ_count(state); ++e) {
            if (e != c && !state.civ(e).ended_year && at_war(state, c, e)) {
                enemies.push_back(e);
            }
        }
        if (enemies.empty()) {
            continue;
        }
        for (std::int64_t r = 1; r <= region_count_; ++r) {
            const auto ri = static_cast<std::size_t>(r - 1);
            if (state.region(r).owner != c || islet_[ri] != 0) {
                continue;
            }
            if (outpost_in[ri] != 0) {
                continue;
            }
            std::int64_t enemy = 0;
            for (const GeoView::Neighbour& nb : neighbours_[ri]) {
                const std::int64_t o = state.region(nb.region).owner;
                if (o != 0 && std::binary_search(enemies.begin(), enemies.end(), o) && (enemy == 0 || o < enemy)) {
                    enemy = o;
                }
            }
            if (enemy == 0) {
                continue;
            }
            if (!chance_pm(ctx.draw(key(OUTPOST), word(c), word(r), 0), tuning_.outpost_permille)) {
                continue;
            }
            const std::optional<Cell> cell = wild_cell(ctx, r, true);
            if (!cell) {
                continue;
            }
            // Raise it (one per civilization per year).
            const auto s = static_cast<std::int64_t>(sites_.size()) + 1;
            if (s > MAX_SITES) {
                fail("history: bound: sites exceed 1000000");
            }
            const std::int64_t culture = state.civ(c).culture;
            const CultureNaming* naming = nullptr;
            std::string_view culture_key;
            for (const auto& [cid, nm] : culture_naming_) {
                naming = cid == culture ? &nm : naming;
            }
            for (const auto& [cid, k] : culture_keys_) {
                culture_key = cid == culture ? std::string_view(k) : culture_key;
            }
            if (naming == nullptr) {
                fail("history: monsters: culture " + int_text(culture) + " has no naming");
            }
            std::string name = choose_name(*naming, peoples_.name_filter(), culture_key, NameKind::site,
                                           ctx.inputs().seed, word(culture), word(s), peoples_.mutable_name_registry())
                                   .name;
            std::int64_t since = 0;
            for (const auto& [a, b] : {std::pair(c, enemy), std::pair(enemy, c)}) {
                const auto it = state.relations().find(RelationKey{CIV_TYPE, a, CIV_TYPE, b, "war"});
                if (since == 0 && it != state.relations().end()) {
                    since = it->second.event;
                }
            }
            EventSpec e = point_event(ctx, "outpost_built", r, 0);
            e.participants = {Participant{"civilization", EntityType::civilization, c},
                              Participant{"enemy", EntityType::civilization, enemy},
                              Participant{"site", EntityType::site, s}};
            e.causes = {since};
            const std::int64_t ev = ctx.append(e);
            Site t;
            t.id = s;
            t.kind = 2;
            t.name = std::move(name);
            t.region = r;
            t.x = cell->x;
            t.y = cell->y;
            t.founder = c;
            t.origin_year = ctx.year();
            t.origin_event = ev;
            t.hostile_year = ctx.year();
            sites_.push_back(std::move(t));
            outpost_in[ri] = 1;
            ++open_of[static_cast<std::size_t>(c)];
            ++stats_.outposts;
            break;
        }
    }
}

// --- Names (9.2) --------------------------------------------------------------------------------------

// P5-03 12.1: a wild monster's name, drawn from the kind's forms without
// those whose epithet an earlier monster of the world bears.
std::string MonstersSubsystem::wild_name(const StepContext& ctx, std::size_t k, std::int64_t m) {
    const MonsterKindDef& def = kinds_[k];
    std::vector<std::string> allowed;
    for (const std::string& form : def.name_forms) {
        const std::string ep = epithet_of(form);
        if (ep.empty() || std::find(epithets_.begin(), epithets_.end(), ep) == epithets_.end()) {
            allowed.push_back(form);
        }
    }
    if (allowed.empty()) {
        if (std::find(def.name_forms.begin(), def.name_forms.end(), std::string(ROOT_SLOT)) != def.name_forms.end()) {
            allowed.push_back(std::string(ROOT_SLOT));
        } else {
            allowed = def.name_forms;
        }
    }
    CultureNaming naming = kind_naming_[k];
    naming.monster_forms = std::move(allowed);
    std::string name = choose_name(naming, peoples_.name_filter(), def.naming_culture, NameKind::monster,
                                   ctx.inputs().seed, word(kind_culture_id_[k]), word(m),
                                   peoples_.mutable_name_registry())
                           .name;
    if (const std::string* form = matched_form(name, def.name_forms)) {
        if (std::string ep = epithet_of(*form); !ep.empty()) {
            epithets_.push_back(std::move(ep));
        }
    }
    return name;
}

// P5-03 12.2: a host of the dead is named for the settlement at whose ruin
// it rose, else for its region's name in force; the place is its root.
std::string MonstersSubsystem::place_name(const StepContext& ctx, std::size_t k, std::int64_t m, std::int64_t region,
                                          std::int64_t settlement, std::string& root) {
    std::string place;
    if (settlement != 0) {
        place = ctx.state().settlement(settlement).name;
    } else if (renaming_ != nullptr) {
        place = renaming_->name_now(ctx, region);
    } else {
        place = ctx.inputs().regions[static_cast<std::size_t>(region - 1)].name;
    }
    constexpr std::string_view THE = "The ";
    if (place.size() >= THE.size() && place.compare(0, THE.size(), THE) == 0) {
        place[0] = 't';
    }
    root = place;
    const std::vector<std::string>& forms = kinds_[k].name_forms;
    if (forms.empty()) {
        fail("history: monsters: monster '" + kinds_[k].key + "' has no name form");
    }
    const auto fill = [&place](const std::string& form) {
        std::string out = form;
        const std::size_t at = out.find(PLACE_SLOT);
        if (at != std::string::npos) {
            out.replace(at, PLACE_SLOT.size(), place);
        }
        return out;
    };
    const std::uint64_t n = forms.size();
    const std::uint64_t start = pick(ctx.draw(name_key_, word(m), 0, 0), n);
    NameRegistry& registry = peoples_.mutable_name_registry();
    for (std::uint64_t j = 0; j < n; ++j) {
        std::string name = fill(forms[static_cast<std::size_t>((start + j) % n)]);
        if (name.size() <= NAME_ATTEMPT_MAX_BYTES && !registry.contains(name)) {
            registry.add(name);
            return name;
        }
    }
    const std::string base = fill(forms[static_cast<std::size_t>(start)]);
    for (std::uint64_t r = 2;; ++r) {
        std::string name = base + " " + roman(r);
        if (name.size() > NAME_MAX_BYTES) {
            fail("names: a name longer than 64 bytes");
        }
        if (!registry.contains(name)) {
            registry.add(name);
            return name;
        }
    }
}

std::string MonstersSubsystem::lair_name(const StepContext& ctx, std::int64_t s, std::size_t kind,
                                         const std::string& root) {
    const std::vector<std::string>& forms = kinds_[kind].lair_forms;
    const std::uint64_t n = forms.size();
    const std::uint64_t start = pick(ctx.draw(key(LAIR_NAME), word(s), 0, 0), n);
    NameRegistry& registry = peoples_.mutable_name_registry();
    const std::array<SlotValue, 1> slots = {SlotValue{ROOT_SLOT, root}};
    for (std::uint64_t j = 0; j < n; ++j) {
        RenamedName filled = fill_slots(forms[static_cast<std::size_t>((start + j) % n)], slots);
        if (stages::renamed_region_name_valid(filled.name, filled.joins, placenames_) &&
            !registry.contains(filled.name)) {
            registry.add(filled.name);
            return std::move(filled.name);
        }
    }
    const std::string base = fill_slots(forms[static_cast<std::size_t>(start)], slots).name;
    for (std::uint64_t k = 2;; ++k) {
        std::string name = base + " " + roman(k);
        if (name.size() > NAME_MAX_BYTES) {
            fail("names: a name longer than 64 bytes");
        }
        if (!registry.contains(name)) {
            registry.add(name);
            return name;
        }
    }
}

// --- Rows (11.2) ------------------------------------------------------------------------------------------

std::vector<db::BeastRow> MonstersSubsystem::beast_rows() const {
    std::vector<db::BeastRow> out;
    for (const BeastDef& b : beasts_) {
        std::string phrases;  // P5-03 16.1: joined with '|'
        for (const std::string& x : b.bold_phrases) {
            phrases += phrases.empty() ? x : "|" + x;
        }
        out.push_back(db::BeastRow{b.id, b.key, b.name, b.plural, b.menace, b.food, std::move(phrases)});
    }
    return out;
}

std::vector<db::MonsterKindRow> MonstersSubsystem::monster_kind_rows() const {
    std::vector<db::MonsterKindRow> out;
    for (const MonsterKindDef& k : kinds_) {
        std::string phrases;  // P5-03 16.1: joined with '|'
        for (const std::string& x : k.raid_phrases) {
            phrases += phrases.empty() ? x : "|" + x;
        }
        out.push_back(db::MonsterKindRow{k.id, k.key, k.name, k.noun, k.grouped ? 1 : 0, k.wake, k.might,
                                         std::move(phrases)});
    }
    return out;
}

std::vector<db::BeastPopulationRow> MonstersSubsystem::beast_population_rows() const {
    std::vector<db::BeastPopulationRow> out = population_rows_;
    std::stable_sort(out.begin(), out.end(), [](const db::BeastPopulationRow& a, const db::BeastPopulationRow& b) {
        return std::tie(a.region, a.beast, a.year) < std::tie(b.region, b.beast, b.year);
    });
    return out;
}

std::vector<db::MonsterRow> MonstersSubsystem::monster_rows() const {
    std::vector<db::MonsterRow> out;
    for (const Monster& m : monsters_) {
        db::MonsterRow r;
        r.id = m.id;
        r.name = m.name;
        r.kind = kinds_[m.kind].id;
        r.lair_site = m.lair;
        r.born = m.born;
        r.born_event = m.born_event;
        r.died = m.died;
        r.death_event = m.death_event;
        r.slain_by = m.slayer;
        r.status = static_cast<std::int64_t>(m.status);
        r.hoard = m.hoard;
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<db::SiteRow> MonstersSubsystem::site_rows() const {
    std::vector<db::SiteRow> out;
    for (const Site& t : sites_) {
        db::SiteRow r;
        r.id = t.id;
        r.name = t.name;
        r.kind = t.kind;
        r.region = t.region;
        r.x = t.x;
        r.y = t.y;
        r.settlement = t.settlement;
        r.founder_civ = t.founder;
        r.monster = t.monster;
        r.origin_year = t.origin_year;
        r.origin_event = t.origin_event;
        r.ended_year = t.ended_year;
        r.ending_event = t.ending_event;
        r.ending = t.ending;
        out.push_back(std::move(r));
    }
    return out;
}

}  // namespace worldgen::history
