#pragma once

// Registry of every stream name used anywhere (spec P0-01 section 7.3).
//
// Every name passed to stream_key() must be a constant here, and must also
// appear in all_stream_names. A test checks that every entry matches the
// stream-name grammar (is_valid_stream_name) and that no name is listed
// twice. Convention: <area>.<stage>[.<purpose>].

#include <array>
#include <string_view>

namespace worldgen::streams {

// Test pattern hash layer (P0-04).
inline constexpr std::string_view test_hash = "test.hash";
// Test pattern noise layer (P0-04).
inline constexpr std::string_view test_noise = "test.noise";
// Test-only stream for comparing wide-arithmetic paths (P0-01 13.3).
inline constexpr std::string_view test_wide = "test.wide";
// Test-only stream for the random hydrology fixtures (P1-02 8.2).
inline constexpr std::string_view test_hydro = "test.hydro";

// Keys of stage elevation (P1-00 section 3, P1-01 section 2), in the order
// of P1-01 section 2. Each is used only as a noise key (noise2 / fbm2
// through the compositions of P1-01 4.1) or a location-hash key (hash_at:
// continent for the centre jitter, erosion for the random receivers); the
// stage draws from no Rng.
inline constexpr std::string_view geo_elevation_continent_warp_x = "geo.elevation.continent_warp_x";
inline constexpr std::string_view geo_elevation_continent_warp_y = "geo.elevation.continent_warp_y";
inline constexpr std::string_view geo_elevation_continent = "geo.elevation.continent";
inline constexpr std::string_view geo_elevation_belt = "geo.elevation.belt";
inline constexpr std::string_view geo_elevation_coast = "geo.elevation.coast";
inline constexpr std::string_view geo_elevation_segment = "geo.elevation.segment";
inline constexpr std::string_view geo_elevation_relief_warp_x = "geo.elevation.relief_warp_x";
inline constexpr std::string_view geo_elevation_relief_warp_y = "geo.elevation.relief_warp_y";
inline constexpr std::string_view geo_elevation_ridge = "geo.elevation.ridge";
inline constexpr std::string_view geo_elevation_hills = "geo.elevation.hills";
inline constexpr std::string_view geo_elevation_roughness = "geo.elevation.roughness";
inline constexpr std::string_view geo_elevation_basin = "geo.elevation.basin";
inline constexpr std::string_view geo_elevation_detail = "geo.elevation.detail";
inline constexpr std::string_view geo_elevation_erosion = "geo.elevation.erosion";
inline constexpr std::string_view geo_elevation_fill = "geo.elevation.fill";
// Pass layout (P1-01 E6.5): hash_at for the col choice and the jitters,
// noise2 of substream 3 for the band width. Registered after
// geo.hydrology.flat (P1-01 section 2).
inline constexpr std::string_view geo_elevation_pass = "geo.elevation.pass";

// Key of stage hydrology (P1-00 section 3, P1-02 section 2): the noise key
// of the H2 tie-break on flats (fbm_m only, no Rng).
inline constexpr std::string_view geo_hydrology_flat = "geo.hydrology.flat";

// Keys of stage climate (P1-00 section 3, P1-03 section 2): the prevailing
// wind (one Rng, one weighted_index call, sequential code only, D9) and the
// noise keys of the temperature and rainfall variation (fbm_m only).
inline constexpr std::string_view geo_climate_wind = "geo.climate.wind";
inline constexpr std::string_view geo_climate_temperature = "geo.climate.temperature";
inline constexpr std::string_view geo_climate_rainfall = "geo.climate.rainfall";
// The per-world temperature offset of C2b (P1-03, generator 0.9.0): one
// Rng, at most one uniform_i64 call, sequential code only (D9).
inline constexpr std::string_view geo_climate_temp_offset = "geo.climate.temp_offset";

// Keys of stage biomes (P1-00 section 3, P1-04 section 2): the noise keys of
// the temperature and rainfall jitter the rules see (fbm_m only, no Rng).
inline constexpr std::string_view geo_biomes_temp_jitter = "geo.biomes.temp_jitter";
inline constexpr std::string_view geo_biomes_rain_jitter = "geo.biomes.rain_jitter";

// Key of stage resources (P1-00 section 3, P1-05 section 2): the noise key
// of the ore deposits (fbm_m only, no Rng).
inline constexpr std::string_view geo_resources_ore = "geo.resources.ore";

// Keys of stage regions (P1-00 section 3, P1-06 section 2): the hash_at key
// of the seed-site tie-break and the noise key of the step cost (no Rng).
inline constexpr std::string_view geo_regions_seed = "geo.regions.seed";
inline constexpr std::string_view geo_regions_cost = "geo.regions.cost";
// P1-06 r3 RG11: the region name draws (P0-01 amendment 20, P3-08).
inline constexpr std::string_view geo_regions_name = "geo.regions.name";

// Keys of the name generator (P3-02 4.5): the stream keys of the keyed
// draws history::draw(stream_key, attempt, culture, entity, j), one per name
// kind. No Rng.
inline constexpr std::string_view hist_names_god = "hist.names.god";
inline constexpr std::string_view hist_names_civilization = "hist.names.civilization";
inline constexpr std::string_view hist_names_settlement = "hist.names.settlement";

// Key of stage chronology (P3-03 3, C3): the keyed draw of each present
// race's awakening shift, history::draw(stream_key, 0, race id, 0, 0). No
// Rng.
inline constexpr std::string_view hist_chronology_awakening = "hist.chronology.awakening";

// Keys of the peoples subsystem of stage history (P3-05 3.3): the keyed
// draws history::draw(stream_key, year, a, b, k), one stream per decision
// kind. No Rng.
inline constexpr std::string_view hist_peoples_home = "hist.peoples.home";
inline constexpr std::string_view hist_peoples_civs = "hist.peoples.civs";
inline constexpr std::string_view hist_peoples_found = "hist.peoples.found";
inline constexpr std::string_view hist_peoples_outward = "hist.peoples.outward";
inline constexpr std::string_view hist_peoples_famine = "hist.peoples.famine";
inline constexpr std::string_view hist_peoples_famine_toll = "hist.peoples.famine_toll";
inline constexpr std::string_view hist_peoples_plague = "hist.peoples.plague";
inline constexpr std::string_view hist_peoples_plague_toll = "hist.peoples.plague_toll";
inline constexpr std::string_view hist_peoples_plague_spread = "hist.peoples.plague_spread";
inline constexpr std::string_view hist_peoples_abandon = "hist.peoples.abandon";
inline constexpr std::string_view hist_peoples_crisis = "hist.peoples.crisis";
inline constexpr std::string_view hist_peoples_split = "hist.peoples.split";
// P3-06 3.3: the conflict streams (P0-01 section 19).
inline constexpr std::string_view hist_conflict_declare = "hist.conflict.declare";
inline constexpr std::string_view hist_conflict_battle = "hist.conflict.battle";
inline constexpr std::string_view hist_conflict_battle_win = "hist.conflict.battle_win";
inline constexpr std::string_view hist_conflict_siege = "hist.conflict.siege";
inline constexpr std::string_view hist_conflict_sack = "hist.conflict.sack";
// P3-06 r10 20.17 (P3-09 r3): the search for a concealed people.
inline constexpr std::string_view hist_conflict_search = "hist.conflict.search";
// P3-06 r22 24.5 (P5-01, generator 0.21.0): the keyed factor of a battle toll.
inline constexpr std::string_view hist_conflict_toll = "hist.conflict.toll";
// P5-12 3 (generator 0.22.0): the renaming subsystem's chance and name draws.
inline constexpr std::string_view hist_renaming_chance = "hist.renaming.chance";
inline constexpr std::string_view hist_renaming_name = "hist.renaming.name";
// P5-02 3 (generator 0.23.0): beasts, monsters, sites, the ghost rule and
// the names of monsters and sites.
inline constexpr std::string_view hist_beasts_bold = "hist.beasts.bold";
inline constexpr std::string_view hist_monsters_wake = "hist.monsters.wake";
inline constexpr std::string_view hist_monsters_rise = "hist.monsters.rise";
inline constexpr std::string_view hist_monsters_sleep = "hist.monsters.sleep";
inline constexpr std::string_view hist_monsters_rewake = "hist.monsters.rewake";
inline constexpr std::string_view hist_monsters_raid = "hist.monsters.raid";
inline constexpr std::string_view hist_monsters_raid_toll = "hist.monsters.raid_toll";
inline constexpr std::string_view hist_monsters_overrun = "hist.monsters.overrun";
inline constexpr std::string_view hist_monsters_hunt = "hist.monsters.hunt";
inline constexpr std::string_view hist_monsters_hunt_outcome = "hist.monsters.hunt_outcome";
inline constexpr std::string_view hist_monsters_lair_name = "hist.monsters.lair_name";
inline constexpr std::string_view hist_sites_outpost = "hist.sites.outpost";
inline constexpr std::string_view hist_peoples_ghost = "hist.peoples.ghost";
inline constexpr std::string_view hist_names_monster = "hist.names.monster";
inline constexpr std::string_view hist_names_site = "hist.names.site";
// P5-02 r1 (6.4.1): the target of a monster's raid.
inline constexpr std::string_view hist_monsters_raid_target = "hist.monsters.raid_target";
// P5-03 3: the species of bold beasts (13), the flight for fear (9.4), the
// losses on the road of an abandonment (11.1), expulsions (6.5).
inline constexpr std::string_view hist_beasts_bold_kind = "hist.beasts.bold_kind";
inline constexpr std::string_view hist_monsters_fear = "hist.monsters.fear";
inline constexpr std::string_view hist_peoples_abandon_loss = "hist.peoples.abandon_loss";
inline constexpr std::string_view hist_relations_expel = "hist.relations.expel";
inline constexpr std::string_view hist_relations_settlers = "hist.relations.settlers";  // P5-03a 5.1
// P5-04 3 (generator 0.26.0).
inline constexpr std::string_view hist_alliances_form = "hist.alliances.form";
inline constexpr std::string_view hist_alliances_join = "hist.alliances.join";
inline constexpr std::string_view hist_alliances_trade = "hist.alliances.trade";
inline constexpr std::string_view hist_alliances_spoils = "hist.alliances.spoils";
inline constexpr std::string_view hist_alliances_mine = "hist.alliances.mine";
// P5-06 3 (generator 0.27.0).
inline constexpr std::string_view hist_figures_reign = "hist.figures.reign";
inline constexpr std::string_view hist_figures_regnal = "hist.figures.regnal";
inline constexpr std::string_view hist_figures_life = "hist.figures.life";
inline constexpr std::string_view hist_figures_appoint = "hist.figures.appoint";
inline constexpr std::string_view hist_figures_skill = "hist.figures.skill";
inline constexpr std::string_view hist_figures_fall = "hist.figures.fall";
inline constexpr std::string_view hist_figures_exile = "hist.figures.exile";
inline constexpr std::string_view hist_figures_hero = "hist.figures.hero";
inline constexpr std::string_view hist_figures_caster = "hist.figures.caster";
inline constexpr std::string_view hist_figures_book = "hist.figures.book";
inline constexpr std::string_view hist_figures_blight = "hist.figures.blight";
inline constexpr std::string_view hist_figures_tower_name = "hist.figures.tower_name";
inline constexpr std::string_view hist_names_person = "hist.names.person";
inline constexpr std::string_view hist_names_book = "hist.names.book";
inline constexpr std::string_view hist_monsters_hero_fall = "hist.monsters.hero_fall";
// P5-04a 2 (generator 0.28.0): the 98th stream, the soured renewal draw.
inline constexpr std::string_view hist_alliances_renew = "hist.alliances.renew";
// P5-07 3 (generator 0.29.0): the five streams of the mountain depths.
inline constexpr std::string_view hist_deep_delve = "hist.deep.delve";
inline constexpr std::string_view hist_deep_wake = "hist.deep.wake";
inline constexpr std::string_view hist_deep_kind = "hist.deep.kind";
inline constexpr std::string_view hist_deep_gate = "hist.deep.gate";
inline constexpr std::string_view hist_deep_take = "hist.deep.take";
// P5-07 r2 3: the allies' aid (6.11) and the goblin deep warrens (6.10).
inline constexpr std::string_view hist_deep_aid = "hist.deep.aid";
inline constexpr std::string_view hist_deep_warren_delve = "hist.deep.warren_delve";
inline constexpr std::string_view hist_deep_warren_wake = "hist.deep.warren_wake";
inline constexpr std::string_view hist_deep_warren_kind = "hist.deep.warren_kind";
// P5-08 3 (generator 0.30.0): the pantheon and cults, and the deep's stir (12.2).
inline constexpr std::string_view hist_pantheon_act = "hist.pantheon.act";
inline constexpr std::string_view hist_pantheon_kind = "hist.pantheon.kind";
inline constexpr std::string_view hist_pantheon_flare = "hist.pantheon.flare";
inline constexpr std::string_view hist_pantheon_monster_kind = "hist.pantheon.monster_kind";
inline constexpr std::string_view hist_pantheon_site_name = "hist.pantheon.site_name";
inline constexpr std::string_view hist_pantheon_cult_found = "hist.pantheon.cult_found";
inline constexpr std::string_view hist_pantheon_cult_name = "hist.pantheon.cult_name";
inline constexpr std::string_view hist_pantheon_cult_spread = "hist.pantheon.cult_spread";
inline constexpr std::string_view hist_pantheon_adopt = "hist.pantheon.adopt";
inline constexpr std::string_view hist_pantheon_suppress = "hist.pantheon.suppress";
inline constexpr std::string_view hist_pantheon_hide = "hist.pantheon.hide";
inline constexpr std::string_view hist_deep_stir = "hist.deep.stir";
inline constexpr std::string_view hist_deep_warren_stir = "hist.deep.warren_stir";
// P5-11 3 (generator 0.31.0): the artifacts and their names.
inline constexpr std::string_view hist_artifacts_make = "hist.artifacts.make";
inline constexpr std::string_view hist_artifacts_class = "hist.artifacts.class";
inline constexpr std::string_view hist_artifacts_kind = "hist.artifacts.kind";
inline constexpr std::string_view hist_artifacts_take = "hist.artifacts.take";
inline constexpr std::string_view hist_artifacts_bury = "hist.artifacts.bury";
inline constexpr std::string_view hist_artifacts_raid = "hist.artifacts.raid";
inline constexpr std::string_view hist_names_artifact = "hist.names.artifact";
// P5-11 r2 3 (stream 128): corruption by an evil artifact (6.11).
inline constexpr std::string_view hist_artifacts_corrupt = "hist.artifacts.corrupt";
// Every registered name, in registration order.
inline constexpr std::array<std::string_view, 128> all_stream_names = {
    test_hash,
    test_noise,
    test_wide,
    geo_elevation_continent_warp_x,
    geo_elevation_continent_warp_y,
    geo_elevation_continent,
    geo_elevation_belt,
    geo_elevation_coast,
    geo_elevation_segment,
    geo_elevation_relief_warp_x,
    geo_elevation_relief_warp_y,
    geo_elevation_ridge,
    geo_elevation_hills,
    geo_elevation_roughness,
    geo_elevation_basin,
    geo_elevation_detail,
    geo_elevation_erosion,
    geo_elevation_fill,
    test_hydro,
    geo_hydrology_flat,
    geo_elevation_pass,
    geo_climate_wind,
    geo_climate_temperature,
    geo_climate_rainfall,
    geo_biomes_temp_jitter,
    geo_biomes_rain_jitter,
    geo_climate_temp_offset,
    geo_resources_ore,
    geo_regions_seed,
    geo_regions_cost,
    geo_regions_name,
    hist_names_god,
    hist_names_civilization,
    hist_names_settlement,
    hist_chronology_awakening,
    hist_peoples_home,
    hist_peoples_civs,
    hist_peoples_found,
    hist_peoples_outward,
    hist_peoples_famine,
    hist_peoples_famine_toll,
    hist_peoples_plague,
    hist_peoples_plague_toll,
    hist_peoples_plague_spread,
    hist_peoples_abandon,
    hist_peoples_crisis,
    hist_peoples_split,
    hist_conflict_declare,
    hist_conflict_battle,
    hist_conflict_battle_win,
    hist_conflict_siege,
    hist_conflict_sack,
    hist_conflict_search,
    hist_conflict_toll,
    hist_renaming_chance,
    hist_renaming_name,
    hist_beasts_bold,
    hist_monsters_wake,
    hist_monsters_rise,
    hist_monsters_sleep,
    hist_monsters_rewake,
    hist_monsters_raid,
    hist_monsters_raid_toll,
    hist_monsters_overrun,
    hist_monsters_hunt,
    hist_monsters_hunt_outcome,
    hist_monsters_lair_name,
    hist_sites_outpost,
    hist_peoples_ghost,
    hist_names_monster,
    hist_names_site,
    hist_monsters_raid_target,
    hist_beasts_bold_kind,
    hist_monsters_fear,
    hist_peoples_abandon_loss,
    hist_relations_expel,
    hist_relations_settlers,
    hist_alliances_form,
    hist_alliances_join,
    hist_alliances_trade,
    hist_alliances_spoils,
    hist_alliances_mine,
    hist_figures_reign,
    hist_figures_regnal,
    hist_figures_life,
    hist_figures_appoint,
    hist_figures_skill,
    hist_figures_fall,
    hist_figures_exile,
    hist_figures_hero,
    hist_figures_caster,
    hist_figures_book,
    hist_figures_blight,
    hist_figures_tower_name,
    hist_names_person,
    hist_names_book,
    hist_monsters_hero_fall,
    hist_alliances_renew,
    hist_deep_delve,
    hist_deep_wake,
    hist_deep_kind,
    hist_deep_gate,
    hist_deep_take,
    hist_deep_aid,
    hist_deep_warren_delve,
    hist_deep_warren_wake,
    hist_deep_warren_kind,
    hist_pantheon_act,
    hist_pantheon_kind,
    hist_pantheon_flare,
    hist_pantheon_monster_kind,
    hist_pantheon_site_name,
    hist_pantheon_cult_found,
    hist_pantheon_cult_name,
    hist_pantheon_cult_spread,
    hist_pantheon_adopt,
    hist_pantheon_suppress,
    hist_pantheon_hide,
    hist_deep_stir,
    hist_deep_warren_stir,
    hist_artifacts_make,
    hist_artifacts_class,
    hist_artifacts_kind,
    hist_artifacts_take,
    hist_artifacts_bury,
    hist_artifacts_raid,
    hist_names_artifact,
    hist_artifacts_corrupt,
};

}  // namespace worldgen::streams
