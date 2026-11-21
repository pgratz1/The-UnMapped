#pragma once

// Stage P1-06 regions (spec docs/specs/p1-06-regions.md; P1-00 sections 3,
// 4, 7.2 and 7.7).
//
// Partitions the land into regions grown from seed cells over a travel cost
// weighted by the height above drainage, merges regions that are too small,
// names them and writes the layer `region` and the tables `region`,
// `region_border` and `region_biome` (schema 5). Steps RG1 to RG14:
// preconditions (RG1), G7 with k = 1 (RG2), the step weight (RG3), one
// candidate per tile (RG4), the greedy seeds with the bank rule (RG5), one
// seed per landmass (RG6), the cost flood (RG7), the merge of small regions
// (RG8), renumbering (RG9), the region rows (RG10), names (RG11), borders and
// passes (RG12), biome counts (RG13) and the write (RG14).
//
// Every step is a library function on plain arrays and any W x H grid (cell
// (x, y) has index y * width + x), so that the tests of P1-06 section 10 can
// call it alone. Every function raises FatalError (P0-01 4.1) on a contract
// violation: an array of the wrong length, threads outside [1, 256], a
// violated precondition, tuning outside its schema bounds. `threads` only
// schedules the G6 row blocks of RG3 (P1-00 D10): no result depends on it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"
#include "worldgen/stages/biomes.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P1-00 3).
inline constexpr std::string_view REGIONS_STAGE = "regions";

// The layer the stage writes (P1-06 3.1; layer id 19 in a full run).
inline constexpr std::string_view REGION_LAYER = "region";

// The largest region id (u16 layer, RG9).
inline constexpr std::int64_t REGION_MAX_ID = 65535;

// The "not reached" cost of the flood (RG7), 2^62.
inline constexpr std::int64_t REGION_COST_INF = std::int64_t{1} << 62;

// The largest height above drainage the step weight accepts (P1-06 7: the
// largest |Delta S| of P1-05 7), decimetres.
inline constexpr std::int64_t REGION_HAND_MAX_DM = 131070;

// --- Tuning (P1-06 4) -----------------------------------------------------------------------

// The words of record `region_names`, in declaration order.
struct RegionNames {
    std::string central;
    std::string north;
    std::string northeast;
    std::string east;
    std::string southeast;
    std::string south;
    std::string southwest;
    std::string west;
    std::string northwest;
    std::string isle;
    std::string upland;  // r2: the noun of an upland region (RG11 3c)

    friend bool operator==(const RegionNames&, const RegionNames&) = default;
};

// The word lists of a placename definition and the slots of a place form,
// in the order of record name_pools (P1-06 4, r3): index 0 nouns ({noun}),
// 1 suffixes ({suffix}), 2 prefixes, 3 adjectives, 4 features, 5
// possessors, 6 places, 7 objects.
inline constexpr std::size_t PLACE_LIST_COUNT = 8;
inline constexpr std::array<std::string_view, PLACE_LIST_COUNT> PLACE_LIST_FIELDS = {
    "nouns", "suffixes", "prefixes", "adjectives", "features", "possessors", "places", "objects"};
inline constexpr std::array<std::string_view, PLACE_LIST_COUNT> PLACE_SLOT_TEXTS = {
    "{noun}", "{suffix}", "{prefix}", "{adjective}", "{feature}", "{possessor}", "{place}", "{object}"};

// Record name_traits (P1-06 4, r3), in declaration order.
struct RegionNameTraits {
    std::int64_t coast_cells = 0;
    std::int64_t river_class = 16;
    std::int64_t lake_permille = 1000;
    std::int64_t high_mean_m = 0;
    std::int64_t ore_rich_cells = 0;
    std::int64_t barren_hab_max = -1;
    std::int64_t fertile_min = 256;

    friend bool operator==(const RegionNameTraits&, const RegionNameTraits&) = default;
};

// The fields of definition `worldgen.regions`, in schema declaration order.
struct RegionTuning {
    std::int64_t seed_tile_m = 0;
    std::int64_t seed_spacing_m = 0;
    std::int64_t hand_ref_m = 0;
    geo::FbmM cost_noise;
    Fx cost_noise_amp;
    Fx lake_cost_factor;
    std::int64_t river_border_class = 0;
    std::int64_t river_cost_m = 0;
    std::int64_t region_min_area_m2 = 0;
    std::int64_t ore_rich_min = 0;
    std::int64_t ore_reach_m = 0;
    Fx name_central_frac;
    RegionNames names;
    // r2: biome keys (definition ids) whose noun an upland region keeps
    // (RG11 3c), at most 16, in data order.
    std::vector<std::string> upland_own_noun;
    // r3: record name_pools, [list][pool] with pools 0 common, 1 land, 2
    // trait (RG11 6), lists in the order of PLACE_LIST_FIELDS.
    std::array<std::array<std::int64_t, 3>, PLACE_LIST_COUNT> name_pool_weights{};
    // r3: record name_traits (RG11 5).
    RegionNameTraits name_traits;

    friend bool operator==(const RegionTuning&, const RegionTuning&) = default;
};

// The tuning held by a validated `worldgen.regions` record. Fatal if a field
// is missing or of another kind.
[[nodiscard]] RegionTuning region_tuning_from(const data::Record& record);
// The tuning of definition worldgen.regions in loaded content. Fatal if the
// content has no such definition.
[[nodiscard]] RegionTuning region_tuning_from(const data::ContentSet& content);

// Fatal ("regions: tuning: <field> outside its schema range", fields in
// declaration order, a word of `names` counting as outside its range when it
// is longer than 32 bytes or fails the word grammar, reported as
// "names.<word>"; upland_own_noun with more than 16 entries or an entry that
// is not an id), then "regions: tuning: seed_tile_m must be at most twice
// seed_spacing_m" (named check of 4). Every seam that takes a tuning calls
// it first.
void check_region_tuning(const RegionTuning& tuning);

// min_cells of RG8: (region_min_area_m2 + 255) div 256.
[[nodiscard]] std::int64_t region_min_cells(const RegionTuning& tuning);

// The sector words of RG11 in the order of region_sector's index: central,
// north, northeast, east, southeast, south, southwest, west, northwest.
[[nodiscard]] std::array<std::string_view, 9> region_sector_words(const RegionNames& names);

// --- RG3: step weight ---------------------------------------------------------------------------

// The cost noise of RG3 at cell (x, y): fbm_m(K_cost, from_int(16 x + 8),
// from_int(16 y + 8), cost_noise), K_cost = stream_key(seed,
// "geo.regions.cost"). Checks the tuning first.
[[nodiscard]] Fx region_cost_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const RegionTuning& tuning);

// RG3: wt = mul(ONE + mul(cost_noise_amp, nz), ONE + from_ratio(hand, 10
// hand_ref_m)). Fatal if nz is outside [-ONE, ONE] or hand outside [0,
// 131070] (REGION_HAND_MAX_DM).
[[nodiscard]] Fx region_step_weight(Fx nz, std::int64_t hand, const RegionTuning& tuning);

// --- RG4, RG5, RG6: seeds -------------------------------------------------------------------------

// A site cell (water 0) with its site key of P1-06 5: (habitability, -S,
// hash_at(K_seed, x, y)), K_seed = stream_key(seed, "geo.regions.seed").
struct RegionSite {
    geo::CellIndex cell = 0;
    std::int64_t habitability = 0;
    std::int64_t surface = 0;  // S, decimetres
    std::uint64_t hash = 0;

    friend bool operator==(const RegionSite&, const RegionSite&) = default;
};

// The site key of cell (x, y): its habitability and surface as given, and
// h = hash_at(stream_key(seed, "geo.regions.seed"), x, y).
[[nodiscard]] RegionSite region_site(geo::Grid grid, geo::CellIndex cell, std::int64_t habitability,
                                     std::int64_t surface, std::uint64_t seed);

// True if site a is better than site b (P1-06 5): key(a) > key(b)
// lexicographically (habitability, then -S, then the hash compared
// unsigned), or the keys are equal and a.cell < b.cell. A strict total order
// on distinct cells.
[[nodiscard]] bool region_site_better(const RegionSite& a, const RegionSite& b) noexcept;

// RG4: the tiles of seed_tile_m div 16 cells, in order (ty, then tx); each
// tile with at least one site cell (water 0) contributes its best site.
// surface is S = elevation + water_depth. Fatal if a size is wrong or the
// tuning fails check_region_tuning.
[[nodiscard]] std::vector<RegionSite> region_candidates(geo::Grid grid, std::span<const std::uint8_t> water,
                                                       std::span<const std::uint8_t> habitability,
                                                       std::span<const std::int64_t> surface, std::uint64_t seed,
                                                       const RegionTuning& tuning);

// The border river cells of P1-06 5: 1 where water = 1 and river >=
// river_border_class, else 0. Fatal if a size is wrong or the class is
// outside [1, 16].
[[nodiscard]] std::vector<std::uint8_t> region_border_rivers(geo::Grid grid, std::span<const std::uint8_t> water,
                                                             std::span<const std::uint8_t> river,
                                                             std::int64_t river_border_class);

// RG5 cross(s, c): the integer segment from s to c meets a border river cell
// (border_river[cell] != 0), including a diagonal slip between two of them.
// Fatal if a size is wrong or a cell is outside the grid.
[[nodiscard]] bool region_crosses(geo::Grid grid, std::span<const std::uint8_t> border_river, geo::CellIndex s,
                                  geo::CellIndex c);

// RG5: the candidates sorted best first (region_site_better), accepted
// greedily unless some accepted seed is near (distance below
// seed_spacing_m) and on the same bank (not region_crosses); returns the
// accepted cells in acceptance order (seed numbers 1, 2, ...). Fatal if a
// size is wrong, a candidate cell is outside the grid or listed twice, or
// the tuning fails.
[[nodiscard]] std::vector<geo::CellIndex> region_seeds(std::span<const RegionSite> candidates, geo::Grid grid,
                                                       std::span<const std::uint8_t> water,
                                                       std::span<const std::uint8_t> river,
                                                       const RegionTuning& tuning);

// G2 on land (water != 3), 8-connected (RG6, the `landmass` labels).
struct RegionLandmasses {
    std::vector<std::uint32_t> label;  // 0 on ocean, else 1 .. count
    std::uint32_t count = 0;
};
[[nodiscard]] RegionLandmasses region_landmasses(geo::Grid grid, std::span<const std::uint8_t> water);

// RG6: for each landmass in label order that holds no seed, appends a seed
// at its best site cell (region_site_better over its cells with water 0),
// or at its smallest-index cell when it has none. Returns the extended list.
// Fatal if a size is wrong or a seed is outside the grid or on ocean.
[[nodiscard]] std::vector<geo::CellIndex> region_landmass_seeds(geo::Grid grid, std::span<const std::uint8_t> water,
                                                                std::span<const std::uint8_t> habitability,
                                                                std::span<const std::int64_t> surface,
                                                                const RegionLandmasses& landmasses,
                                                                std::vector<geo::CellIndex> seeds, std::uint64_t seed);

// --- RG7: flood -------------------------------------------------------------------------------

// The owners and costs of RG7: owner = the seed number 1 .. seeds.size() on
// land, 0 on ocean; cost in decimetres of level walking (REGION_COST_INF
// where not reached, ocean included).
struct RegionFlood {
    std::vector<std::uint32_t> owner;
    std::vector<std::int64_t> cost;
};

// The step cost of RG7 from c to its neighbour m in direction d (both land,
// m = neighbour(c, d)): hz from wt[m] (lake factor on a lake), plus 10
// river_cost_m when m is a border river cell, plus 10 river_cost_m more on a
// diagonal step whose two corner cells are border river cells. Fatal if c
// has no neighbour in direction d or a size is wrong.
[[nodiscard]] std::int64_t region_step_cost(geo::Grid grid, std::span<const Fx> wt,
                                            std::span<const std::uint8_t> water,
                                            std::span<const std::uint8_t> border_river, geo::CellIndex c,
                                            geo::Direction d, const RegionTuning& tuning);

// RG7: the multi-source Dijkstra over land cells, popping by (cost, seq),
// relaxing in the 8-neighbour order, strict improvement only. wt is the RG3
// weight of each land cell (any value elsewhere; must be > 0 on land).
// Fatal if a size is wrong, a value of water or river is out of range, a
// seed is outside the grid, on ocean or listed twice, or wt <= 0 on land.
[[nodiscard]] RegionFlood region_flood(geo::Grid grid, std::span<const Fx> wt, std::span<const std::uint8_t> water,
                                       std::span<const std::uint8_t> river, std::span<const geo::CellIndex> seeds,
                                       const RegionTuning& tuning);

// --- RG8, RG9: merge and renumber ---------------------------------------------------------------

// RG8 on the flood owners (numbers 1 .. count on land, 0 on ocean): small
// regions (fewer than min_cells cells) with a neighbour are merged, smallest
// (cells, number) first, into the neighbour with the largest (nr, n), ties
// to the smaller number. Returns the owners after merging (each cell holds
// the number of the region that absorbed it). Fatal if a size is wrong, a
// land cell has owner 0 or above count, or an ocean cell a non-zero owner.
[[nodiscard]] std::vector<std::uint32_t> region_merge(geo::Grid grid, std::span<const std::uint32_t> owner,
                                                      std::span<const std::uint8_t> water,
                                                      std::span<const std::uint8_t> river, std::uint32_t count,
                                                      std::int64_t min_cells, const RegionTuning& tuning);

// RG9: ids 1, 2, ... in order of each region's smallest cell index; 0 stays
// 0. `old_of_id[k - 1]` is the owner number that became id k. Fatal
// ("regions: more than 65535 regions") above REGION_MAX_ID.
struct RegionNumbering {
    std::vector<std::uint16_t> region;
    std::vector<std::uint32_t> old_of_id;
};
[[nodiscard]] RegionNumbering region_renumber(std::span<const std::uint32_t> owner);

// --- RG11: names -----------------------------------------------------------------------------

// The sector index of RG11 for a seed offset (dx, dy) from the land centroid
// (y grows southward) and central radius r_c: 0 central, 1 north, 2
// northeast, 3 east, 4 southeast, 5 south, 6 southwest, 7 west, 8 northwest.
// Fatal if |dx| or |dy| is 2^24 or more, or r_c < 0.
[[nodiscard]] std::uint32_t region_sector(std::int64_t dx, std::int64_t dy, std::int64_t r_c);

// The Roman numeral of k (standard subtractive form; thousands as repeated
// M). Fatal if k < 1 or k > 100000. Forwards to worldgen::roman_numeral
// (core/roman.hpp), which the history library shares.
[[nodiscard]] std::string roman(std::int64_t k);

// --- RG11 r3: the placename data (P1-06 4.1) -------------------------------------------------

enum class PlaceRole : std::uint8_t { common, land, trait };
enum class PlaceSpecial : std::uint8_t { none, isle, lake, upland };

// The eight traits of RG11 5, in trait order (the order of the `trait`
// enumeration of 4.1; index 0 to 7).
inline constexpr std::size_t REGION_TRAIT_COUNT = 8;
inline constexpr std::array<std::string_view, REGION_TRAIT_COUNT> REGION_TRAIT_NAMES = {
    "coast", "river", "lake", "high", "ore", "pass", "barren", "fertile"};

struct PlaceForm {
    std::string form;
    std::int64_t weight = 1;

    friend bool operator==(const PlaceForm&, const PlaceForm&) = default;
};

// One placename definition.
struct PlacenameDef {
    std::string id;
    PlaceRole role = PlaceRole::common;
    std::vector<std::string> biomes;  // biome keys (role land)
    PlaceSpecial special = PlaceSpecial::none;
    std::int64_t trait = -1;  // -1 none, else the index in REGION_TRAIT_NAMES
    std::vector<PlaceForm> forms;
    std::array<std::vector<std::string>, PLACE_LIST_COUNT> lists;  // PLACE_LIST_FIELDS order
    std::vector<std::string> forbidden;
    std::vector<std::string> excluded;

    friend bool operator==(const PlacenameDef&, const PlacenameDef&) = default;
};

// Every definition of kind placename, in canonical (ascending id) order.
struct PlacenameData {
    std::vector<PlacenameDef> definitions;

    friend bool operator==(const PlacenameData&, const PlacenameData&) = default;
};

// The placename definitions of loaded content (validated by the loader), in
// ascending id. An empty list when the kind has no definition.
[[nodiscard]] PlacenameData placename_data_from(const data::ContentSet& content);

// Section 2 check 4a (r3), each fatal with the prefix "regions: placename: ":
// a common definition exists ("there must be exactly one common
// definition"); (i) every biome with water = false, ascending code, is in
// the biomes of a land definition ("biome '<key>' has no land definition");
// (ii) a land definition for isle, lake, upland ("no land definition for
// '<special>'"); (iii) every slot of every form of every land definition
// (ascending id; form index from 0) can be filled without traits ("'<id>'
// form <i> slot '<slot>' has no words", the slot with its braces).
void check_placename_data(const PlacenameData& data, const BiomeDefinitions& definitions, const RegionTuning& tuning);

// --- RG11 r3: one region's name -------------------------------------------------------------------

// What RG11 needs of each region, in id order.
struct RegionNameInput {
    std::int64_t seed_x = 0;
    std::int64_t seed_y = 0;
    std::uint8_t biome = 0;          // dominant biome code
    std::uint32_t landmass = 0;      // G2 label
    // r2 (RG11 3b, 3c): the region's cells, lake cells and walkable cells.
    // The defaults (one walkable dry cell) select neither Mere nor upland.
    std::int64_t cells = 1;
    std::int64_t lake_cells = 0;
    std::int64_t walk_cells = 1;
    // r3 (RG11 5): the figures the traits test, and whether a border row
    // with passes > 0 touches the region.
    std::int64_t coast_cells = 0;
    std::int64_t river_class_max = 0;
    std::int64_t elev_mean_dm = 0;
    std::int64_t ore_rich_cells = 0;
    std::int64_t habitability_mean = 0;
    std::int64_t fertility_mean = 0;
    bool has_pass = false;
};

// RG11 5: the traits a region holds, indexed by REGION_TRAIT_NAMES.
using RegionTraits = std::array<bool, REGION_TRAIT_COUNT>;
[[nodiscard]] RegionTraits region_traits(const RegionNameInput& region, const RegionTuning& tuning);

// The slot values of one filled form, in the order the slots appear in the
// form (left to right): (list index of PLACE_LIST_FIELDS, value).
using PlaceSlotValues = std::vector<std::pair<std::size_t, std::string>>;

// The form with each slot replaced by its value. Fatal if a slot of the
// form has no value or the form is not of the place_form grammar.
[[nodiscard]] std::string region_fill_form(std::string_view form, const PlaceSlotValues& values);

// RG11 7: V1 to V6 for a form filled with `values` (one per slot, in slot
// order) against the common definition's forbidden and excluded lists.
[[nodiscard]] bool region_name_valid(std::string_view form, const PlaceSlotValues& values,
                                     const PlacenameDef& common);

// One attempt of RG11 6 and 7: the name of attempt t for region id r, and
// whether it is valid (V1 to V6). An invalid attempt's name is not
// specified (P3-02 4.2's convention). `traits` are the region's trait
// definitions in trait order. k_name = substream(stream_key(seed,
// "geo.regions.name"), N).
struct RegionNameAttempt {
    bool valid = false;
    std::string name;
    PlaceSlotValues values;  // r6 15.4: the slot values, in the form's order
};
[[nodiscard]] RegionNameAttempt region_name_attempt(std::uint64_t k_name, std::int64_t region, std::int64_t attempt,
                                                    const PlacenameDef& land, const PlacenameDef& common,
                                                    std::span<const PlacenameDef* const> traits,
                                                    const RegionTuning& tuning);

// r6 15.4 (V7, generator 0.19.0): the stems of the slot values other than
// {noun} and {suffix}: each value lower-cased, without a leading "the ",
// cut to its first five bytes.
[[nodiscard]] std::vector<std::string> region_name_stems(const PlaceSlotValues& values);

// RG11 8 fallback: the first compound P[i] + S[j] (P = common prefixes then
// the land's, S = the land's suffixes then the common ones, i outer) that
// passes V1 to V6 as {prefix}{suffix} and is not in `used`; std::nullopt
// when there is none.
[[nodiscard]] std::optional<std::string> region_name_fallback(const PlacenameDef& land, const PlacenameDef& common,
                                                              const std::set<std::string, std::less<>>& used);

// How a region got its name (golden lines of P1-06 10.5): kept its base
// name, the accepted attempt t, or the fallback.
struct RegionNameChoice {
    std::string name;
    bool keeper = false;
    bool fallback = false;
    std::int64_t attempt = 0;
};

// RG11 1 to 4 without the draws: the base name (sector word, one space,
// noun) of every region, in id order. Fatal if a region's biome has no
// definition, a landmass is out of range, the tuning fails, land_cells is 0,
// or an upland_own_noun key names no loaded biome ("regions: tuning:
// upland_own_noun '<key>' is not a loaded biome").
[[nodiscard]] std::vector<std::string> region_base_names(std::span<const RegionNameInput> regions,
                                                         std::span<const std::uint64_t> landmass_cells,
                                                         std::uint64_t land_cells, std::uint64_t sum_x,
                                                         std::uint64_t sum_y, std::int64_t n,
                                                         const BiomeDefinitions& definitions,
                                                         const RegionTuning& tuning);

// RG11 r3 for regions in id order: base names and lands (1 to 3), keepers
// (4), traits (5), the draws keyed by seed and n (6, 7), uniqueness and the
// fallback (8). land_cells, sum_x and sum_y are over the land cells of the
// map; landmass_cells[k - 1] is the size of landmass k; n is the map side.
// Fatal as region_base_names, check_placename_data, and "regions: no free
// name for region <id>".
[[nodiscard]] std::vector<RegionNameChoice> region_name_choices(
    std::span<const RegionNameInput> regions, std::span<const std::uint64_t> landmass_cells, std::uint64_t land_cells,
    std::uint64_t sum_x, std::uint64_t sum_y, std::int64_t n, const BiomeDefinitions& definitions,
    const PlacenameData& placenames, const RegionTuning& tuning, std::uint64_t seed);

// The names of region_name_choices.
[[nodiscard]] std::vector<std::string> region_names(std::span<const RegionNameInput> regions,
                                                    std::span<const std::uint64_t> landmass_cells,
                                                    std::uint64_t land_cells, std::uint64_t sum_x,
                                                    std::uint64_t sum_y, std::int64_t n,
                                                    const BiomeDefinitions& definitions,
                                                    const PlacenameData& placenames, const RegionTuning& tuning,
                                                    std::uint64_t seed);

// --- RG12, RG13: borders, passes, biome counts --------------------------------------------------

// RG12 without the pass columns: one row per pair of regions sharing a
// 4-adjacent land cell pair, in (a, b) order. region is the RG9 layer;
// surface S; walk_area of P1-05. Fatal if a size is wrong.
[[nodiscard]] std::vector<db::RegionBorderRow> region_borders(geo::Grid grid, std::span<const std::uint16_t> region,
                                                              std::span<const std::int64_t> surface,
                                                              std::span<const std::uint8_t> water,
                                                              std::span<const std::uint8_t> river,
                                                              std::span<const std::uint16_t> walk_area,
                                                              const RegionTuning& tuning);

// RG12 passes (r2): each pass row is attached to at most one border, the
// crossing of its path nearest its col (q = first position of the col cell
// in the path; a crossing inside step j has d = min(|j - q|, |j + 1 - q|);
// smallest (d, j), then the first in sequence order); none if its path
// crosses no border. Sets pass_id (smallest attached id) and passes (count)
// of the rows. Fatal if a size is wrong, a path is not a chain of cells
// ("regions: input precondition: pass <id>: path is not a chain of cells",
// every pass first), a col cell is outside the grid or not on its path
// ("... pass <id>: col cell is not on its path", then every pass), or a
// crossing has no border row.
void region_pass_crossings(geo::Grid grid, std::span<const std::uint16_t> region,
                           std::span<const std::uint8_t> water, std::span<const db::PassRow> passes,
                           std::vector<db::RegionBorderRow>& borders);

// --- The whole stage ---------------------------------------------------------------------------

// The thirteen input layers of P1-06 section 2 on a W x H grid, each
// grid.cells() values, catalogue types, in the order of the inputs table.
struct RegionInputs {
    geo::Grid grid;
    std::span<const std::int16_t> elevation;
    std::span<const std::uint8_t> water;
    std::span<const std::uint16_t> water_depth;
    std::span<const std::uint8_t> flow_dir;
    std::span<const std::uint8_t> river;
    std::span<const std::uint8_t> biome;
    std::span<const std::uint8_t> fertility;
    std::span<const std::uint8_t> ore;
    std::span<const std::uint8_t> timber;
    std::span<const std::uint8_t> fresh_water;
    std::span<const std::uint8_t> forage;
    std::span<const std::uint8_t> habitability;
    std::span<const std::uint16_t> walk_area;
};

// The stage's outputs (P1-06 3) and the intermediate values the golden
// vectors of 10.4 hash.
struct RegionResult {
    std::vector<std::uint16_t> region;              // layer `region`
    std::vector<db::RegionRow> regions;             // table region, id order
    std::vector<db::RegionBorderRow> borders;       // table region_border, (a, b) order
    std::vector<db::RegionBiomeRow> biomes;         // table region_biome, (region, biome) order
    // Intermediates (10.4).
    std::vector<std::int64_t> hand;                 // RG2, dm, every cell
    std::vector<Fx> weight;                         // RG3, every land cell, 0 elsewhere
    std::vector<std::uint32_t> flood_owner;         // RG7 owners before merging
    std::vector<geo::CellIndex> seeds;              // RG5 + RG6, seed order
    std::vector<RegionNameChoice> names;            // RG11 r3, id order (10.5 golden lines)
};

// RG1 to RG13 on a W x H grid (n = W, the map side of RG11, taken as the
// grid width). Fatal, in the order of section 2 items 2 to 5: threads
// outside [1, 256] ("regions: thread count outside [1, 256]"); the tuning
// (check_region_tuning); a biome region_noun that is not one word
// ("regions: biome '<key>' has a region_noun that is not one word", ascending
// code); r3: check_placename_data (check 4a); then with the prefix
// "regions: input precondition: ": the grid
// checks; "<name> length is not width * height" (first input of the wrong
// length, order of RegionInputs); "water value <v> at (<x>, <y>)"; "river
// value <v> at (<x>, <y>)"; "biome value <v> at (<x>, <y>) has no
// definition"; "pass <id>: path is not a chain of cells" (passes in the
// order given); G7 ("G7: ...").
[[nodiscard]] RegionResult regions_stage(const RegionInputs& inputs, const BiomeDefinitions& definitions,
                                         const PlacenameData& placenames, std::span<const db::PassRow> passes,
                                         const RegionTuning& tuning, std::uint64_t seed, std::uint32_t threads = 1);

// The stage entry point (P1-06 RG14, P1-00 3): reads the biome definitions,
// worldgen.regions, the thirteen stored input layers (section 2 item 1:
// present, then catalogue type, then size, each over all inputs in turn) and
// table pass, runs regions_stage with meta.seed, and writes the layer
// `region` (codec 1) and the rows of the three tables.
void run_regions(StageContext& context);

}  // namespace worldgen::stages
