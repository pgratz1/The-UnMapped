#pragma once

// Markers for settlements, ruins and sites in the 3D terrain preview (spec
// V3D-05 sections 3 to 9).
//
// Plain C++, no Godot types: the records read through worldgen's public
// history and marker API (3), their placement on the height buffer (4),
// their placeholder models (5), the beacon transforms (6), the choice of
// labels (7), the HUD and status texts, the jump order and view and the
// option parsing (8). TerrainPreview (terrain_preview_markers.cpp) turns
// them into nodes; the unit tests (gdext/tests/markers3d_test.cpp) use this
// file without Godot.
//
// Floating point is allowed here: nothing computed in this file reaches a
// world file (the preview opens world.db read-only). The records, the
// placement cells, the house slots and the house hash are integers.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "terrain/detail.hpp"
#include "terrain/fly_state.hpp"
#include "terrain/placeholder_meshes.hpp"
#include "terrain/terrain_data.hpp"
#include "worldgen/db/reader.hpp"

namespace unmapped::terrain {

// --- Records (3) -------------------------------------------------------------------

enum class MarkerKind : std::uint8_t { settlement = 1, ruin, old_ruin, lair, outpost, mine, tower };

struct MarkerRecord {
    MarkerKind kind = MarkerKind::settlement;
    std::int64_t id = 0;  // settlement id (settlement, ruin, old ruin) or site id
    int x = 0;
    int y = 0;
    int settlement_kind = 0;  // 1 to 5 for settlement entries, 0 for sites
    std::int64_t population = 0;
    bool capital = false;
    bool declining = false;
    bool library = false;
    std::uint32_t colour = 0;  // 0xRRGGBB
    std::string name;
    std::string kind_word;
    std::string text;
};

enum class MarkerStatus { none, loaded, failed };

struct MarkerLoad {
    MarkerStatus status = MarkerStatus::none;
    std::string code;
    std::string message;
    std::int64_t end_year = -1;
    std::int64_t year = -1;
    std::int64_t asked_year = -1;
    bool year_clamped = false;
    std::vector<MarkerRecord> records;
    // Bench (13): load_history, and the rest of section 3.
    std::uint64_t history_usec = 0;
    std::uint64_t records_usec = 0;
    // V3D-06 7 (Paul, Q-V3D-06-4): with pads, every place that ever stood:
    // the union over the years 0 to E, one entry per (settlement or site,
    // id, model cell), in that key's order, with the largest r0 it had.
    std::vector<PadPlace> pad_places;
    std::int64_t pad_years = 0;
    std::uint64_t places_usec = 0;
};

// Section 3: the records at the history's end year, or at min(year, E).
// With pads, also the pad places of V3D-06 7 for a map of map_n cells.
// Never throws.
[[nodiscard]] MarkerLoad load_marker_records(const worldgen::db::WorldReader& reader, std::optional<std::int64_t> year,
                                             bool pads = false, int map_n = 0);

// 3.1's colours (0xRRGGBB).
inline constexpr std::uint32_t MARKER_NO_OWNER = 0x808080;  // (128, 128, 128)
inline constexpr std::uint32_t RUIN_STONE = 0x78726A;       // (120, 114, 106)
inline constexpr std::uint32_t OLD_RUIN_STONE = 0x8C8880;   // (140, 136, 128)
inline constexpr std::uint32_t LAIR_ROCK = 0x584E42;        // (88, 78, 66)
inline constexpr std::uint32_t MARKER_MINE = 0x3C3C3C;      // (60, 60, 60): the headframe beam without founder
inline constexpr std::uint32_t MINE_BEACON = 0xD6A840;      // (214, 168, 64): V3D-06 12
inline constexpr std::uint32_t MARKER_TOWER = 0x783CB4;     // (120, 60, 180)
// Section 5's colours.
inline constexpr std::uint32_t HOUSE_WALL = 0xB4A48C;   // (180, 164, 140)
inline constexpr std::uint32_t WALL_STONE = 0x968F87;   // (150, 144, 135)
inline constexpr std::uint32_t LAIR_MOUTH = 0x0C0A0A;   // (12, 10, 10)
inline constexpr std::uint32_t WOOD = 0x705234;         // (112, 82, 52)
inline constexpr std::uint32_t SPOIL = 0x605C56;        // (96, 92, 86)
inline constexpr std::uint32_t TOWER_STONE = 0x968E84;  // (150, 142, 132)
// Section 6's colours.
inline constexpr std::uint32_t RUIN_BEACON = 0x8C8478;  // (140, 132, 120)
inline constexpr std::uint32_t LAIR_BEACON = 0x96281E;  // (150, 40, 30)

// The 3.2 text of a settlement entry and of each site kind, from the words
// the library gives (the scene never builds these; load_marker_records does,
// and the unit tests call them on hand-made values).
struct SettlementTextParts {
    std::string name;
    std::int64_t kind = 0;
    std::int64_t status = 0;
    bool capital = false;
    std::string owner;  // empty: no owner
    std::int64_t population = 0, since = 0, founded = 0;
};
[[nodiscard]] std::string standing_text(const SettlementTextParts& parts);
// word: "ruin of a", "old ruin of a" or "faded ruin of a"; cause: the
// rendered ruin event, or empty when there is none ("cause not recorded").
[[nodiscard]] std::string ruin_text(const std::string& name, std::string_view word, std::int64_t kind,
                                    std::int64_t ruined_year, const std::string& cause);
[[nodiscard]] std::string mine_text(const std::string& name, const std::string& founder, std::int64_t since,
                                    std::int64_t depth);
[[nodiscard]] std::string tower_text(const std::string& name, bool library, const std::string& figure,
                                     std::int64_t since);
// monster empty: "(lair, empty)".
[[nodiscard]] std::string lair_text(const std::string& name, const std::string& monster, const std::string& monster_kind,
                                    bool asleep, std::int64_t since);
[[nodiscard]] std::string outpost_text(const std::string& name, const std::string& founder, std::int64_t since);

// --- Placement (4) -------------------------------------------------------------------

inline constexpr int MARKER_CLEAR_CELLS = 4;
inline constexpr int MARKER_MAX_MOVES = 3;
inline constexpr int MARKER_MOVE_CELLS = 7;

struct Marker3D {
    MarkerRecord rec;
    int cell_x = 0;
    int cell_y = 0;
    int moves = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float top = 0.0f;
    bool beacon = false;
    std::uint32_t beacon_colour = 0;
};

// Steps 1 and 2 of section 4: each record's model cell and moves (cells only).
struct ModelCell {
    int x = 0;
    int y = 0;
    int moves = 0;
};
[[nodiscard]] std::vector<ModelCell> marker_cells(std::span<const MarkerRecord> records, int n);

// Section 4; throws std::invalid_argument when heights is not N^2 long.
[[nodiscard]] std::vector<Marker3D> place_markers(std::span<const MarkerRecord> records, const Layout& layout,
                                                  std::span<const float> heights);
// V3D-06 10: on a field (layout the coarse one): Y the field at the anchor
// vertex, meshes on ground_height.
[[nodiscard]] std::vector<Marker3D> place_markers(std::span<const MarkerRecord> records, const Layout& layout,
                                                  const GroundField& field);
// V3D-05 5's slot extent e (settlements of their houses, ruins of their
// stubs; 0 for sites) and V3D-06 7's pad radius r0 (m).
[[nodiscard]] int marker_extent(const MarkerRecord& rec);
[[nodiscard]] std::int32_t pad_radius(const MarkerRecord& rec);

// --- Models (5) ----------------------------------------------------------------------

inline constexpr float SINK = 0.5f;
inline constexpr std::uint32_t HOUSE_KEY = 0x484F5553;
inline constexpr std::array<int, 6> MIN_HOUSES = {0, 2, 4, 8, 16, 6};  // index: settlement kind 1 to 5
inline constexpr int MAX_HOUSES = 64;
inline constexpr float MODEL_FAR = 8000.0f;
inline constexpr float OLD_RUIN_FAR = 3000.0f;
inline constexpr float MODEL_FADE_MARGIN = 500.0f;

struct HouseSlot {
    int i = 0;
    int j = 0;
};
// The 289 pairs with |i|, |j| <= 8 sorted by (i^2 + j^2, j, i).
[[nodiscard]] const std::array<HouseSlot, 289>& house_slots();
// min(64, max(MIN_HOUSES[kind], isqrt(population / 8))); kind outside 1..5
// counts as 1.
[[nodiscard]] int house_count(int kind, std::int64_t population);
// The draw distance of a kind's model.
[[nodiscard]] float model_far(MarkerKind kind);

// Section 5: vertices relative to the anchor (X, Y, Z); the same bytes for
// the same inputs.
[[nodiscard]] MeshData build_marker_mesh(const Marker3D& marker, const Layout& layout, std::span<const float> heights);

// One box or gable part as built, for the tests of the ground rule: its
// footprint corners relative to the anchor and its lowest y.
struct GroundedPart {
    std::array<float, 8> corners{};  // x0, z0, ..., x3, z3
    float bottom = 0.0f;
};
// The parts of build_marker_mesh that stand on the ground (boxes, and
// ellipsoids by their bounding square), in build order.
[[nodiscard]] std::vector<GroundedPart> grounded_parts(const Marker3D& marker, const Layout& layout,
                                                       std::span<const float> heights);

// --- Beacons (6) ---------------------------------------------------------------------

inline constexpr float BEACON_MIN_W = 2.0f;

struct BeaconStyle {
    float h = 0.0f;
    float px_w = 0.0f;
    float px_h = 0.0f;
    float fade = 0.0f;
    float far = 0.0f;
};
// The row of 6 for a record (an old ruin has h 0 and far 0: no beacon).
[[nodiscard]] BeaconStyle beacon_style(const MarkerRecord& rec);
// 2 tan(fov / 2) / viewport_h, radians per pixel.
[[nodiscard]] float pixel_angle(float fov_deg, int viewport_h);
// Basis columns then origin (Godot's Transform3D order).
[[nodiscard]] std::array<float, 12> beacon_transform(const Marker3D& marker, float cx, float cy, float cz, float a);

// --- Labels (7) ----------------------------------------------------------------------

inline constexpr float LABEL_RANGE = 6000.0f;
inline constexpr float LABEL_SLACK = 500.0f;
inline constexpr std::size_t LABEL_MAX = 12;
inline constexpr std::size_t LABEL_KEEP = 16;
inline constexpr float LABEL_LIFT = 2.0f;  // B is 2 m above the model's top (4)

[[nodiscard]] std::vector<std::size_t> select_labels(std::span<const Marker3D> markers, float cx, float cy, float cz,
                                                     std::span<const std::size_t> previous);
// Declutter (built beyond the spec, V3D-05 r1 has no rule for it; see the
// README): a label's screen box, centre (x, y) in pixels, and whether its
// point is in front of the camera.
struct LabelBox {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool on_screen = false;
};
inline constexpr bool LABEL_DECLUTTER = true;
inline constexpr float LABEL_CHAR_EM = 0.55f;  // mean glyph width in ems
inline constexpr float LABEL_LINE_EM = 1.25f;  // line height in ems
// The box of a label whose point projects to (sx, sy): em_px is the font's
// em in screen pixels, offset_px the label's offset above the point.
[[nodiscard]] LabelBox label_box(const MarkerRecord& rec, float sx, float sy, float em_px, float offset_px, bool on_screen);
// In shown order (nearest first): false for a label whose box overlaps the
// box of a nearer drawn label by more than half the smaller box's area.
[[nodiscard]] std::vector<bool> declutter_labels(std::span<const LabelBox> boxes);
// "<name>\n<kind_word>".
[[nodiscard]] std::string label_text(const MarkerRecord& rec);
// The label's modulate as 0xRRGGBB: white, ruins (200, 200, 200), old ruins (170, 170, 170).
[[nodiscard]] std::uint32_t label_modulate(MarkerKind kind);

// --- HUD, status, jump, options (8) -----------------------------------------------------

inline constexpr float HUD_TEXT_RADIUS = 2000.0f;
inline constexpr std::size_t HUD_LINE_MAX = 200;
inline constexpr float JUMP_DIST = 500.0f;   // spec 1,200 (tuned, see the README)
inline constexpr float JUMP_ABOVE = 150.0f;  // spec 350 (tuned, see the README)
inline constexpr const char* MARKER_KEYS = ", M markers, N/Shift+N next/previous settlement";

// The nearest marker by horizontal distance to (X, Z); ties the smaller index.
[[nodiscard]] std::optional<std::size_t> nearest_marker(std::span<const Marker3D> markers, float X, float Z);
// One of N, NE, E, SE, S, SW, W, NW for a direction (dx, dz) from the camera
// (north is -Z).
[[nodiscard]] std::string bearing_word(float dx, float dz);
// "<int> m" below 1,000 m (rounded down), else "<one decimal> km".
[[nodiscard]] std::string distance_text(float m);
// Cut to at most `max` bytes at a UTF-8 character boundary; when cut, the
// result ends with "..." and is still at most `max` bytes.
[[nodiscard]] std::string cut_utf8(const std::string& text, std::size_t max);

enum class MarkersHud { on, off, none, failed, disabled };
// HUD line 3 (8.2).
[[nodiscard]] std::string markers_hud_line(MarkersHud state, std::span<const Marker3D> markers, std::int64_t year,
                                           float cam_x, float cam_z);

[[nodiscard]] std::vector<std::size_t> jump_order(std::span<const Marker3D> markers);
[[nodiscard]] FlyState jump_view(const Marker3D& marker, const Layout& layout, std::span<const float> heights);

// 8.1: settlement:<id>, site:<id>, rank:<k>, else a name (exact, then ASCII
// case-folded).
[[nodiscard]] std::optional<std::size_t> find_marker(std::span<const Marker3D> markers,
                                                     std::span<const std::size_t> order, std::string_view v);

// A whole number >= 0 (digits only, at most 18).
[[nodiscard]] std::optional<std::int64_t> parse_year(std::string_view text);
// The status line(s) of 8.4 (two lines, joined by '\n', when the year was clamped).
[[nodiscard]] std::string markers_status_line(const MarkerLoad& load);
inline constexpr const char* MARKERS_OFF_STATUS = "markers: off (--no-markers)";
// 8.1's refusals.
inline constexpr const char* AT_MARKER_REFUSAL =
    "--at-marker cannot be combined with --at, --at-forest, --at-peak or --no-markers";
[[nodiscard]] std::string year_refusal(std::string_view v);
[[nodiscard]] std::string at_marker_miss(std::string_view v, std::int64_t year);

}  // namespace unmapped::terrain
