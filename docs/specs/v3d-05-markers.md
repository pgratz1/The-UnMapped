# V3D-05 Markers for settlements and sites

Status: **r2, 2026-11-28, as built; interim: built on the recommendations, pending Paul's review.** Paul is away overnight and authorised building this stage on the recommendations of section 16 (Q-V3D-05-1 to 4); each is a constant or one function, so it reverses cheaply. r2 records what was built where it departs from r1 (two tuned jump constants, a label declutter, the ruin ring radius, the HUD cut) and adds Q-V3D-05-5 to 8 and an open list (16). Fifth stage of the 3D terrain viewer track (`docs/handoff-3d-terrain-viewer.md`), on top of V3D-01 r3, V3D-02 r2, V3D-03 r2 and V3D-04 r2 ("V3D-0n m" refers to their sections). Art stays generated placeholders; no asset file is added.

**No world change.** The stage reads `world.db` through `worldgen`'s public API and writes nothing to it or to the source tree. `GENERATOR_VERSION`, the schema, every golden file, `world-hashes.sh` and its cases are unchanged; nothing under `worldgen/`, `tools/worldgen-cli/`, `game/data/` or `extern/` changes. No new dependency. **Vegetation, ground and look are unchanged bit for bit** (Q-V3D-05-1). Owner: viewer engineer.

**Not in this stage:** site plans or real buildings; roads; a year bar or history playback in 3D; territory borders on the ground (a candidate for a later stage: the territory image of `render_territory` as a second colour map); browser panes; clicking markers; event places and passes; carved rivers; detail noise; VR.

## 1. Purpose

The preview shows land only. This stage shows where the world's settlements, ruins and sites are at the history's end year E (or `--year Y`), as placeholder models standing on the ground, with a beacon above each that is visible from far, labels for the nearest, a HUD line for the nearest marker, and keys to tour the settlements, so that a flight over a world means something.

## 2. Parts

| Area | Files | Contents |
| --- | --- | --- |
| markers (plain C++, no Godot types) | new `gdext/src/terrain/markers3d.hpp`, `.cpp` | records from the library (3), placement (4), meshes (5), beacons (6), labels (7), HUD text, jump order, option parsing (8); section 9 |
| scene | `terrain_preview.hpp`, `.cpp`, new `terrain_preview_markers.cpp` | nodes, frame update, keys, options, status, bench (10, 8, 13) |
| check | new `terrain_check_markers.cpp`; `terrain_check.cmake` | T31 to T40 (12.2) |
| unit tests | new `gdext/tests/markers3d_test.cpp` | U40 to U52 (12.1) |
| docs | `gdext/README.md`, "Terrain preview" | options, keys, HUD, status, checks, screenshots, bench |

`markers3d.cpp` joins `unmapped_terrain_core` (which already links `worldgen::worldgen`), `terrain_preview_markers.cpp` and `terrain_check_markers.cpp` join `unmapped_gdext`, `markers3d_test.cpp` joins `gdext-terrain-tests`. The 2D viewer's files are not touched.

## 3. Data (through the library only)

`WorldFile::reader()` gives the `db::WorldReader` of the open world. `load_marker_records(reader, year)` (9):

| Step | Call | On failure |
| --- | --- | --- |
| 1 | `history::load_history(reader)` | `history_missing`: status none (a geography-only world). Any other error: failed, with worldgen's code name and message |
| 2 | `inspect::civilization_colours(h.tables)` | FatalError: failed |
| 3 | `inspect::MarkerIndex::load(reader, h)` | failed |
| 4 | Y = E = `h.timeline.end_year()` when no `--year`; else min(year, E), `year_clamped` when year > E | — |
| 5 | `index.markers_at(h, Y, MarkerOptions{faded = true, events = false, passes = false, sites = true})` | FatalError: failed |
| 6 | for s in `h.timeline.sites_existing(Y)`: `st = h.timeline.site_at(s, Y)`, kept when `st.open` and `st.kind` is 1 (lair) or 2 (outpost) | FatalError: failed |

No SQL, nothing from `worldgen/src`, no event kind tested by id. Every exception is caught in `load_marker_records`; it never throws.

### 3.1 Records, in this order

Settlement entries of step 5 ascending settlement id, then the sites of steps 5 (kinds 3, 4) and 6 (kinds 1, 2) merged ascending site id. Names: `h.view.name(EntityType::settlement | site | civilization | monster, id)`, `"<type> <id>"` when null or empty.

| Kind (`MarkerKind`) | From | `settlement_kind` | Colour (roof or accent) | `kind_word` |
| --- | --- | --- | --- | --- |
| `settlement` 1 | status 1 or 2 | kind 1 to 5 | `colours[owner − 1]`; owner 0: `MARKER_NO_OWNER` (128, 128, 128) | `settlement_kind_word(kind)`, then `, capital` when `capital` |
| `ruin` 2 | `settlement_marker_type` = ruin | kind | `RUIN_STONE` (120, 114, 106) | `ruin of a <kind word>` |
| `old_ruin` 3 | `settlement_marker_type` = faded (an old ruin, or status 4) | kind | `OLD_RUIN_STONE` (140, 136, 128) | `old ruin of a <kind word>` (status 4: `faded ruin of a`) |
| `lair` 4 | step 6, kind 1 | 0 | `LAIR_ROCK` (88, 78, 66) | `lair` |
| `outpost` 5 | step 6, kind 2 | 0 | `colours[founder − 1]`; 0: `MARKER_NO_OWNER` | `outpost` |
| `mine` 6 | step 5 site kind 3 | 0 | `colours[founder − 1]`; 0: `MARKER_MINE` | `mine` |
| `tower` 7 | step 5 site kind 4 | 0 | `MARKER_TOWER` (120, 60, 180) | `library` when `settlement` ≠ 0, else `tower` |

`declining` = status 2; `population` = the entry's population (sites 0); `library` = tower with `settlement` ≠ 0.

### 3.2 Text (the HUD's hover text)

The 2D viewer's text (P4-04 8.5, P5-04 11.3, P5-06 11.4) is built in `viewer_model.cpp`, which has Godot types, so it is restated here and built from the same worldgen calls (`settlement_kind_word`, `settlement_status_word`, `render_event`, `figure_display_name`, `mine_depth_word`, `Timeline::mine_depth_at`):

| Kind | Text |
| --- | --- |
| settlement | `<name>, <kind word>[ (capital)][ of <owner name>], <status word>, population <p> recorded in <since>, founded in <founded>` |
| ruin, old ruin | `<name>, <ruin of a / old ruin of a / faded ruin of a> <kind word>, ruined in <R>: <render_event(ruin_event)>`, or `: cause not recorded` when `ruin_event` is 0; a FatalError of `render_event` gives `(<what>)` as in 2D |
| mine | `<name> (mine[ of <founder name>]), since <origin_year>[, delved <mine_depth_word(d)>]`, d = `mine_depth_at(site, Y)` ≥ 1 |
| tower | `<name> (<tower / library>[ of <figure display name>]), since <origin_year>`; the figure is the first participant of entity type figure of the site's `origin_event` |
| lair | `<name> (lair[ of <monster name>, <monster_kind_row(monster)->name>][, asleep]), since <origin_year>`; the monster part when `monster` ≠ 0 and `monster_at(monster, Y).alive`, `, asleep` when not `awake`; otherwise `(lair, empty)` |
| outpost | `<name> (outpost[ of <founder name>]), since <origin_year>` |

Differences from 2D, by design (section 15): no `, sealed in <year>` on a mine and no warren text on a settlement (both live in `HistoryState`, which is not in the plain library).

## 4. Placement (plain C++)

With N the map side, `heights` the preview's height buffer (V3D-01 5.2) and `clamp_cell(v) = min(max(v, 0), N − 1)`:

1. Settlements, ruins, old ruins, lairs and outposts: model cell = record cell. Mines and towers: record cell + `inspect::site_glyph_offset` (a mine (+7, −7), a library (−7, −7), a wild tower (0, 0)), clamped.
2. Collision, sites only, in record order: while an earlier record's model cell is within Chebyshev distance `MARKER_CLEAR_CELLS` 4 of this one's and `moves` < 3: model cell += (+7, +7), clamped, `moves` += 1. Settlement entries never move (a lair on a ruin's cell moves off it).
3. Anchor: (X, Z) = `cell_to_world(layout, cx, cy)`, Y = `heights[cy·N + cx]` exactly (the vertex Terrain3D draws at that cell; a lake's surface on a lake cell).
4. `top` = Y + the largest vertex y of the model's mesh (5). Beacon base and label point B = (X, top + 2, Z).

## 5. Models (plain C++: one `MeshData` per marker, vertices relative to the anchor)

Primitives (flat normals, V3D-02's winding, per-vertex sRGB colour as `MeshData`):

| Primitive | Triangles | Shape |
| --- | --- | --- |
| box(cx, cz, w, d, h, yaw) | 12 | footprint w (x) by d (z), turned by yaw about Y |
| gable(cx, cz, w, d, rh, yaw) | 6 | two slopes and two gables on a w × d box top, ridge along the longer side, height rh |
| pyramid(cx, cz, w, h) | 4 | square base w, apex h above it, no bottom |
| ellipsoid(c, rx, ry, rz) | 48 | V3D-02 6.2's lat-long ellipsoid, 8 sides, 4 rings (8 · (2·4 − 2)) |

**Ground rule.** For a box or gable part, g0 = the least `ground_low` (V3D-02 6.5) at the four footprint corners in world metres, g1 the largest; the part spans y from g0 − Y − `SINK` 0.5 to g0 − Y + h (a roof sits on its box's top). An ellipsoid's centre is at g0 − Y of its bounding square. So no part floats; a part on a slope may sink up to g1 − g0 into the uphill side.

**Houses.** `HOUSE_SLOTS`: the 289 pairs (i, j) with |i|, |j| ≤ 8, sorted by (i² + j², j, i) ascending: (0, 0), (0, −1), (−1, 0), (1, 0), (0, 1), (−1, −1), (1, −1), (−1, 1), (1, 1), (0, −2), … House k stands at (16 i, 16 j) m from the anchor plus a jitter, one terrain cell each. Per house, h = `hash32(uint32(record id), k, HOUSE_KEY 0x484F5553)` (`ground.hpp`), fields by bits:

| Bits | Field | Value |
| --- | --- | --- |
| 0–3, 4–7 | jitter x, z | (b − 7.5) · 0.125 m (±0.94) |
| 8–10 | w | 6 + 0.5 b m (6 to 9.5) |
| 11–13 | d | 5 + 0.5 b m (5 to 8.5) |
| 14–15 | wall h | 3.5 + 0.75 b m (3.5 to 5.75) |
| 16 | yaw base | 0° or 90° |
| 17–20 | yaw jitter | (b − 7.5) · 1.25° |
| 21 | roofless | only when declining |

Half-diagonal ≤ 6.4 m and jitter ≤ 0.94 m, so houses 16 m apart never overlap. Walls `HOUSE_WALL` (180, 164, 140); gable rh = 0.45 · min(w, d) in the roof colour, each channel × 12 / 16 (floor) when declining.

`house_count(kind, population) = min(64, max(MIN_HOUSES[kind], isqrt(population / 8)))`, `MIN_HOUSES` = {1: 2, 2: 4, 3: 8, 4: 16, 5: 6} (integer; population 100 → 3, 1,000 → 11, 5,000 → 25, 32,768 and above → 64). e = max over the used slots of max(|i|, |j|).

| Kind | Parts | Size (m) | Draw to (m) |
| --- | --- | --- | --- |
| settlement 1 to 3 | H houses (box + gable, no gable when roofless); capital: slot 0 is a hall, box 12 × 12 × 10 in `HOUSE_WALL` + pyramid 13 × 6 in the roof colour | 16 (2e + 1) across, 4 to 10 high (hall 16) | `MODEL_FAR` 8,000 |
| settlement 4 city | as above, plus a wall: square of half-side S = 16 e + 12, thickness 2, height 6, `WALL_STONE` (150, 144, 135): north, east, west boxes of length 2S + 2, south in two boxes leaving a 10 m gate at the centre (5 boxes) | up to 2S + 2 | 8,000 |
| settlement 5 fortress | as 4 with wall height 8, and slot 0 a keep: box 10 × 10 × 16 (capital 12 × 12 × 20) in `WALL_STONE` + pyramid 11 × 5 (13 × 6) in the roof colour | | 8,000 |
| ruin | ceil(H/2) stubs on the first slots: box w × d × (1 + 0.5 · bits 14–15) in `RUIN_STONE`, no roof; a broken ring: 8 tangential segments at radius R = 16 e + 10, segment m centred at (R sin 45m°, −R cos 45m°), length 2R sin 22.5° · 0.85, thickness 2, height 2; segments m ∈ {0, 2, 3, 5, 7} only; here e is the extent of the stub slots (the first ceil(H/2) slots), not of the town's H houses | 2R + 2 | 8,000 |
| old ruin | ceil(H/4) stubs at half height; ring segments m ∈ {0, 3, 5} at height 1, e over the ceil(H/4) stub slots; `OLD_RUIN_STONE` | 2R + 2 | `OLD_RUIN_FAR` 3,000 |
| lair | mound ellipsoid radii (10, 6, 10) in `LAIR_ROCK`; mouth box 6 × 2 (z) × 4 at (0, 8.5), `LAIR_MOUTH` (12, 10, 10) | 20 × 6 | 8,000 |
| outpost | palisade: four boxes on a 22 m square, thickness 1, height 4, `WOOD` (112, 82, 52); watchtower box 4 × 4 × 9 at (−9, −9) + pyramid 5 × 2.5 in the founder colour | 23 × 11.5 | 8,000 |
| mine | pit box 8 × 8 × 0.4 `LAIR_MOUTH`; headframe: posts 0.8 × 0.8 × 10 at (±4, 0) in `WOOD`, beam 9.6 × 0.8 × 0.8 at 10 m in the founder colour; spoil heap ellipsoid radii (7, 3.5, 7) at (12, 0), (96, 92, 86) | 27 × 10.8 | 8,000 |
| tower | column box 6 × 6 × 36 (library 26) in (150, 142, 132) + pyramid 7 × 6 in `MARKER_TOWER`; a library adds a hall box 12 × 8 × 7 at (9, 0) with a gable in `MARKER_TOWER` | 6 × 42 (library 18 × 32) | 8,000 |

Triangles: a settlement 18 per roofed house, 12 per roofless one, a hall or keep (16) in place of slot 0's house, +60 for a wall; ruin 12 · (stubs + 5); old ruin 12 · (stubs + 3); lair 60; outpost 64; mine 96; tower 16 (library 34). A 1024 world's few hundred markers stay under about 150,000 triangles.

## 6. Beacons (plain C++ transform, one `MultiMesh` for all)

A beacon is a vertical quad from (−0.5, 0, 0) to (0.5, 1, 0), turned to face the camera and scaled each frame so that it keeps a minimum size on screen. a = `pixel_angle(fov_deg, viewport_h) = 2 tan(fov/2) / viewport_h` (radians per pixel; the camera's fov is 70°, vertical). d = |camera − B| (3D).

```
d >= far:            basis 0 (hidden), origin B
taper = d <= fade ? 1 : (far - d) / (far - fade)
w = max(BEACON_MIN_W, d * px_w * a) * taper            BEACON_MIN_W = 2 m
h = max(h_kind, d * px_h * a)
yaw = atan2(cam.x - B.x, cam.z - B.z)
basis = RotY(yaw) * diag(w, h, 1); origin B
```

| Kind | h_kind (m) | px_w | px_h | fade, far (m) | Colour |
| --- | --- | --- | --- | --- | --- |
| settlement 1 to 5 | 80, 120, 180, 260, 220; capital + 100 | 4 (capital 6) | 40 | 16,000, 24,000 | the record's colour (not dimmed) |
| ruin | 60 | 2 | 24 | 5,000, 8,000 | `RUIN_BEACON` (140, 132, 120) |
| old ruin | no beacon | | | | |
| lair, outpost, mine, tower | 70, 70, 70, 90 | 3 | 24 | 8,000, 12,000 | (150, 40, 30), the record's colour, `MARKER_MINE` (60, 60, 60), `MARKER_TOWER` |

At 24 km with a 900-pixel view, a = 0.001556: a settlement beacon is 4 px wide and at least 40 px (1,494 m) tall. Beacon instances: every record but old ruins, in record order, fixed at load.

## 7. Labels (`Label3D`, plain C++ selection)

Label point B (4). d_i = |camera − B_i|, ties by record index i.

```
C  = { i : d_i <= LABEL_RANGE } sorted by (d_i, i)                          LABEL_RANGE = 6,000 m
C' = { i : d_i <= LABEL_RANGE + LABEL_SLACK } sorted by (d_i, i)            LABEL_SLACK = 500 m
shown = first LABEL_MAX of C                                                LABEL_MAX = 12
      + { i in previous : rank of i in C' < LABEL_KEEP }                    LABEL_KEEP = 16
sorted by (d_i, i), at most LABEL_KEEP
```

The slack and keep stop two labels swapping at the edge from flickering. A label: text `<name>\n<kind_word>`; `billboard` enabled, `fixed_size` true, `no_depth_test` true (a label behind a hill still shows: 12 nearest only), `font_size` 20, `outline_size` 6, outline black, `modulate` white (ruins (200, 200, 200), old ruins (170, 170, 170)), `offset` (0, 24) px above B, `pixel_size` chosen so that capitals are 14 to 18 px high at 1600 × 900 (W26; built: 0.0017, capitals 14 to 15 px). At most 16 labels of about 150 × 40 px cover under 7% of the screen.

**Declutter (r2, built beyond r1).** After `select_labels`, when `LABEL_DECLUTTER` (true; false switches it off), each shown label i gets a screen box (`label_box`): its point B_i is projected with the camera (`is_position_behind`; when behind, the box is off screen); with a = `pixel_angle` (6), em = `font_size` · `pixel_size` / a and offset = 24 · `pixel_size` / a pixels; chars = the longer line of `<name>\n<kind_word>` counted in UTF-8 characters (bytes not of the form 10xxxxxx);

```
w = chars * LABEL_CHAR_EM * em            LABEL_CHAR_EM = 0.55
h = 2 * LABEL_LINE_EM * em                LABEL_LINE_EM = 1.25
centre (x, y) = (sx, sy - offset)
```

`declutter_labels` walks the boxes in shown order (nearest first); box j (on screen) is hidden when some earlier box i that is on screen and drawn has ox > 0, oy > 0 and ox · oy > 0.5 · min(w_i h_i, w_j h_j), where ox, oy are the overlaps of the two boxes in x and y. A hidden or off-screen box hides nothing; an off-screen box is never hidden. A hidden label's node is not visible; the selection itself (and so the `previous` of the next frame) is unchanged. Reason: a mine or library and its own settlement, 112 m apart, always printed one label over the other.

## 8. HUD, keys, options, status

### 8.1 Options after `--`

| Option | Meaning |
| --- | --- |
| `--year <Y>` | the markers of year Y, a whole number ≥ 0 (default E); Y > E gives E with a status note |
| `--no-markers` | the history is not read; no marker node; key M does nothing |
| `--at-marker <v>` | start at `jump_view` (8.3) of the marker `v`: `settlement:<id>`, `site:<id>`, `rank:<k>` (entry k, from 1, of the jump order), else a name: the first record whose name equals v byte for byte, else the first equal under ASCII case folding (bytes ≥ 0x80 as they are); `--alt`, `--above`, `--heading`, `--pitch` override as with `--at-peak` |

Refusals (exit 1, the text on stderr, as V3D-04 T29): `--year <v>: a whole number from 0`; `--at-marker cannot be combined with --at, --at-forest, --at-peak or --no-markers`. A `--at-marker` that matches nothing is not refused (the world is not read yet): the default start, and the status line `--at-marker <v>: no marker of that name or id at year <Y>; starting at the default view`.

### 8.2 Keys and HUD

| Key | Action |
| --- | --- |
| M | markers on or off: the `visible` of `Markers` (models, beacons, labels); no rebuild; nothing without loaded markers |
| N, Shift+N | jump to the next, previous entry of the jump order (wraps; the first N goes to entry 1); works with markers off |

`HUD_KEYS` gains `, M markers, N/Shift+N next/previous settlement`. HUD line 3, between the vegetation line and the keys:

| State | Line |
| --- | --- |
| loaded, on | `markers <n> at year <Y>; nearest <D> <bearing>: <text>` when the nearest record by horizontal distance is within `HUD_TEXT_RADIUS` 2,000 m, else `markers <n> at year <Y>; nearest <name> (<kind_word>), <D> <bearing>` |
| loaded, off | `markers off (M)` |
| none | `markers: none, no history in this world` |
| failed | `markers: none, the history could not be read` |
| `--no-markers` | `markers off (--no-markers)` |

D: `<int> m` below 1,000 m (rounded down), else `<one_decimal(km)> km`. Bearing from the camera to the record: θ = atan2(dx, −dz) in degrees from north, sector floor((θ + 22.5) / 45) mod 8 of N, NE, E, SE, S, SW, W, NW (float, HUD only). The line is cut at a UTF-8 character boundary so that it is at most `HUD_LINE_MAX` 200 bytes including the `...` added when cut (`cut_utf8`: keep at most 197 bytes, back to a boundary, then `...`). Ties in "nearest" go to the smaller index. The key line and line 3 are shown with `--no-markers` too, so a `--no-markers` picture differs from V3D-04's in the HUD text only.

### 8.3 Jump order and view

`jump_order`: the `settlement` records (standing only), population descending, ties the smaller settlement id. `jump_view(marker)`: camera X = anchor X, Z = anchor Z + `JUMP_DIST` (south of it), heading 0; when anchor Z + `JUMP_DIST` > 8N − 16, Z = anchor Z − `JUMP_DIST` and heading 180. Altitude A = max(top + `JUMP_ABOVE`, `ground_clamp(X, Z)` + 100). Pitch = −atan2(A − top, `JUMP_DIST`) in degrees, clamped to [−89, 89]. Speed unchanged. Built (r2, tuned): `JUMP_DIST` 500 m, `JUMP_ABOVE` 150 m (r1: 1,200 and 350; from 1.2 km a city showed as a few pixels; from 500 m, pitch about −17°, a city reads as roofs inside its wall).

### 8.4 Status lines

Loaded: `markers: <s> settlements (<c> capitals), <r> ruins (<o> old), <l> lairs, <p> outposts, <m> mines, <t> towers at year <Y> of <E>`, then `--year <Y'> is after the history's end: markers at <E>` when clamped. None: `no history in this world: no markers`. Failed: `markers: the history could not be read: <code>: <message>`. `--no-markers`: `markers: off (--no-markers)`.

## 9. Plain C++ contracts (`markers3d.hpp`, namespace `unmapped::terrain`)

| Name | Contract |
| --- | --- |
| `enum class MarkerKind : std::uint8_t { settlement = 1, ruin, old_ruin, lair, outpost, mine, tower }` | 3.1 |
| `struct MarkerRecord { MarkerKind kind; std::int64_t id; int x, y; int settlement_kind; std::int64_t population; bool capital, declining, library; std::uint32_t colour; std::string name, kind_word, text; }` | id: settlement or site id; colour 0xRRGGBB |
| `enum class MarkerStatus { none, loaded, failed }`; `struct MarkerLoad { MarkerStatus status; std::string code, message; std::int64_t end_year = -1, year = -1, asked_year = -1; bool year_clamped = false; std::vector<MarkerRecord> records; }` | |
| `MarkerLoad load_marker_records(const worldgen::db::WorldReader&, std::optional<std::int64_t> year)` | section 3; never throws |
| `struct Marker3D { MarkerRecord rec; int cell_x, cell_y, moves; float X, Y, Z, top; bool beacon; std::uint32_t beacon_colour; }` | |
| `std::vector<Marker3D> place_markers(std::span<const MarkerRecord>, const Layout&, std::span<const float> heights)` | section 4 (top from `build_marker_mesh`); throws `std::invalid_argument` when heights is not N² |
| `MeshData build_marker_mesh(const Marker3D&, const Layout&, std::span<const float> heights)` | section 5; the same bytes for the same inputs |
| `HOUSE_SLOTS` (289), `int house_count(int kind, std::int64_t population)` | 5 |
| `float pixel_angle(float fov_deg, int viewport_h)`; `std::array<float, 12> beacon_transform(const Marker3D&, float cx, float cy, float cz, float a)` | 6; basis columns then origin, Godot's `Transform3D` order |
| `std::vector<std::size_t> select_labels(std::span<const Marker3D>, float cx, float cy, float cz, std::span<const std::size_t> previous)` | 7 |
| `struct LabelBox { float x, y, w, h; bool on_screen; }`; `LabelBox label_box(const MarkerRecord&, float sx, float sy, float em_px, float offset_px, bool on_screen)`; `std::vector<bool> declutter_labels(std::span<const LabelBox>)`; `std::string label_text(const MarkerRecord&)`; `std::uint32_t label_modulate(MarkerKind)` | 7 (declutter, r2) |
| `std::string cut_utf8(const std::string&, std::size_t max)` | 8.2 |
| `std::optional<std::size_t> nearest_marker(std::span<const Marker3D>, float X, float Z)`; `std::string markers_hud_line(...)`; `std::string bearing_word(float dx, float dz)`; `std::string distance_text(float m)` | 8.2 |
| `std::vector<std::size_t> jump_order(std::span<const Marker3D>)`; `FlyState jump_view(const Marker3D&, const Layout&, std::span<const float> heights)` | 8.3 |
| `std::optional<std::size_t> find_marker(std::span<const Marker3D>, std::span<const std::size_t> order, std::string_view v)` | 8.1 |
| `std::optional<std::int64_t> parse_year(std::string_view)`; `std::string markers_status_line(const MarkerLoad&)` | 8.1, 8.4 |
| constants of sections 4 to 8 | `MARKER_CLEAR_CELLS` 4, `SINK` 0.5, `HOUSE_KEY`, `MIN_HOUSES`, colours, `MODEL_FAR` 8,000, `OLD_RUIN_FAR` 3,000, `MODEL_FADE_MARGIN` 500, beacon table, `LABEL_RANGE` 6,000, `LABEL_SLACK` 500, `LABEL_MAX` 12, `LABEL_KEEP` 16, `LABEL_LIFT` 2, `LABEL_DECLUTTER` true, `LABEL_CHAR_EM` 0.55, `LABEL_LINE_EM` 1.25, `HUD_TEXT_RADIUS` 2,000, `HUD_LINE_MAX` 200, `JUMP_DIST` 500, `JUMP_ABOVE` 150 (r2) |

## 10. Scene side

In `open_world`, after the vegetation and before `place_start`: unless `--no-markers`, `load_marker_records`, `place_markers`, then build:

| Node (child of the preview root) | Contents |
| --- | --- |
| `Markers` (Node3D) | parent of the three below; its `visible` is key M |
| `Markers/Models` | one `MeshInstance3D` per record, named `s<id>` (settlement entries) or `t<id>` (sites), position (X, Y, Z), an `ArrayMesh` from `build_marker_mesh`; one shared `StandardMaterial3D`: vertex colour as sRGB albedo, roughness 0.9, cull back; `visibility_range_end` the kind's draw distance, margin 500, fade self |
| `Markers/Beacons` | one `MultiMeshInstance3D`: the beacon quad, `use_colors`, instance count = beacon records; material unshaded, cull disabled, vertex colour as albedo; `custom_aabb` the map's box up to 30 km |
| `Markers/Labels` | 16 `Label3D` nodes `l0` to `l15`, hidden unless shown |

Each frame (when `Markers` is visible): every beacon's transform from `beacon_transform` with the current camera and viewport, set as one buffer; `select_labels` with last frame's result, labels filled in shown order (`l0` the nearest), a label hidden by the declutter (7) left not visible. The HUD line every frame. A world without history, a failure or `--no-markers` builds no `Markers` node. Opening another world or key G drops and rebuilds it (key G keeps M's state).

## 11. Invariants

1. The preview opens `world.db` read-only and writes nothing but `--screenshot`'s file; nothing in `worldgen/`, `tools/`, `game/data/`, `extern/`, the schema or a golden file changes.
2. The records at Y are exactly: the settlement entries of `markers_at(h, Y, {true, false, false, true})`, its sites (kinds 3, 4), and the sites open at Y of kinds 1 and 2 from the timeline; in the order of 3.1; one model per record.
3. Every model's position is (X, `heights[cy·N + cx]`, Z) of its model cell, which is the record cell for settlement entries, lairs and outposts before the collision rule.
4. No part of a model lies wholly above the ground under it (section 5's ground rule).
5. Vegetation, ground, control, look and the far tier are V3D-04's bit for bit with markers on, off or absent: V3D-03 invariants 2 to 4 and V3D-04 invariant 2 hold (nothing in vegetation reads a marker; trees may stand among houses, Q-V3D-05-1).
6. At most 16 labels, each with d ≤ 6,500 m; a beacon is hidden (zero basis) exactly when d ≥ its far.
7. Key M changes `visible` only; the records, meshes and beacon count do not change until the world is reopened.
8. A world without history, or `--no-markers`, has no `Markers` node and the status and HUD lines of 8.

## 12. Tests

### 12.1 Unit tests (`gdext-terrain-tests`, no Godot)

Fixtures: hand-made `MarkerRecord` lists and hand-made height grids (flat at 100 m; a 30° slope; a single spike) on a 512 layout, in the test file. The unit tests have no generated world, and worldgen's history fixtures (`worldgen/tests/data/`) are read by worldgen's test support, not its public API, so `load_marker_records` is tested by the terrain check only (T31).

| Id | Test |
| --- | --- |
| U40 | `HOUSE_SLOTS`: 289 distinct pairs, the first 13 as in 5, sorted by the key |
| U41 | `house_count` at kinds 1 to 5 × populations 0, 7, 8, 100, 1,000, 32,767, 32,768, 10^7 against the formula |
| U42 | `place_markers`: settlement cells unchanged; mine (+7, −7), library (−7, −7), wild tower 0; a lair on a ruin's cell moves (+7, +7) once; a chain of four colliding sites stops after 3 moves; clamping at the four map edges; Y equals the height buffer at the model cell exactly |
| U43 | `build_marker_mesh`: triangle counts of 5 for every kind, capital, declining (roofless count from the hash), city and fortress walls; every part's lowest y ≤ ground_low − Y − 0.5 + 1e-4 at its corners on the slope grid; every vertex colour from the tables; `top` the largest y; two calls give the same bytes |
| U44 | `beacon_transform` at d = 0, 1,000, fade, between, far − 1, far, 2 · far for a settlement, a ruin and a site: w, h and the zero basis by the test's own formula (1e-4 relative); the quad's normal faces the camera; old ruins have no beacon; `pixel_angle(70, 900)` = 0.0015560 ± 1e-7 |
| U45 | `select_labels`: 30 hand-placed markers: at most 12 new, at most 16 with the kept; a sequence of 20 camera steps across the 12th rank: no label leaves while its rank in C' is under 16; ties by index; none beyond 6,500 m |
| U46 | `bearing_word` at the eight directions and at ±22.4°, ±22.6°; `distance_text` at 0, 999.9, 1,000, 12,345; `markers_hud_line` for each state of 8.2 and both forms; the 200-byte cut never splits a UTF-8 character |
| U47 | `jump_order` by population, ties by id, standing settlements only; `jump_view` south, the north fallback near the south edge, the altitude rule over high ground, the pitch |
| U48 | `find_marker`: `settlement:3`, `site:7`, `rank:1`, `rank:0` and `rank:` beyond the list (none), exact name, case-folded name, a name with a byte ≥ 0x80, no match |
| U49 | `parse_year`: `0`, `6000`; refused `-1`, `x`, `1.5`, empty, `+3` |
| U50 | `markers_status_line` for loaded (with and without clamping), none, failed; the text forms of 3.2 for every kind from a hand-made record (the library-facing words: `settlement_kind_word`, `settlement_status_word`, `mine_depth_word`) |
| U51 | the constants of section 9 (`JUMP_DIST` 500, `JUMP_ABOVE` 150) |
| U52 | (r2) `label_box`: w, h and centre from 7's formulas, UTF-8 characters counted once; `declutter_labels`: a box shifted a quarter width over a nearer one is hidden, three quarters is drawn, an off-screen box neither hides nor is hidden, a hidden box hides nothing |

### 12.2 Terrain check (`gdext-terrain-check`; additions)

The check world `w512` (seed 49, 512) is now generated with every stage (no `--until regions`), so it carries a history; `w1024` stays geography only and is the no-history case. W25 below: the layers of `w512` are the same bytes either way, so T1 to T30 are unaffected. Expected cost: the history of a 512 world (a few seconds; record it) plus four short runs, about 15 s; the timeout stays 300 s. New runs on `w512`: `--year 3000`, `--no-markers`, `--at-marker rank:1`, `--at-marker settlement:<the largest settlement's id>`, `--at-marker nowhere`. Measured (r2): the check takes 37.5 s in `linux-gcc-release` (31.9 s before) and 68.3 s in `linux-gcc-debug`. The check keeps its own copy of sections 4, 6, 7 and 8.3, and collects its own expected records from `MarkerIndex` and `Timeline` (section 3 steps 1 to 6).

| Id | Check |
| --- | --- |
| T31 | the preview's records equal the check's own at E (kind, id, cell, settlement kind, population, capital, declining, colour, name), in order; counts per kind equal; content assert that `w512` has at least one standing settlement, one capital and one ruin (lairs, outposts, mines, towers: counted, an info line when none) |
| T32 | `Markers/Models` holds exactly one `MeshInstance3D` per record with its name, at its model cell by the check's own rule, Y equal to the height buffer exactly; mesh arrays equal `build_marker_mesh` (an info line when the renderer keeps no arrays, as T28); visibility ranges as 5 |
| T33 | `Markers/Beacons`: instance count and colours per record; at three camera points (300 m above the largest settlement, 3 km above the centre, 30 km south of the map) every transform equal to the check's own 6 within 1e-4 relative |
| T34 | labels at the same points and along a 40-frame path: the shown set and order equal the check's own selection, and the drawn ones its own copy of the declutter (7); text, `billboard`, `fixed_size`, `no_depth_test`, `font_size`, `outline_size`, `modulate`; hidden labels not visible |
| T35 | the status line and the HUD line of 8 in each run; `--year 3000` (content assert 3000 ≤ E): records equal the check's own at 3000; with a `--year` above E the clamped records and the note |
| T36 | key M hides `Markers` and sets the HUD line, M again restores; records and meshes untouched |
| T37 | key N from the start gives `jump_view` of order entry 1, N again entry 2, Shift+N back to 1, Shift+N at 1 wraps to the last; `--at-marker rank:1` and `settlement:<id>` start at the same view; `--at-marker nowhere` the default start and its status line |
| T38 | vegetation: T12, T13 and T17 to T19 pass on `w512` with markers (invariant 5) |
| T39 | no history (`w1024`, the elevation-only and test-pattern worlds) and `--no-markers`: no `Markers` node, the status and HUD lines; key M does nothing |
| T40 | refusals of 8.1: `--year -1`, `--year x`, `--at-marker rank:1 --at 10 10`, `--at-marker rank:1 --at-forest`, `--at-marker rank:1 --at-peak`, `--at-marker rank:1 --no-markers` |

### 12.3 Screenshots (the engineer judges from these)

```
X() { env -u WAYLAND_DISPLAY xvfb-run -a -s "-screen 0 1600x900x24" "$GODOT" --display-driver x11 --path "$B/game" res://viewer3d/terrain_preview.tscn -- "$@"; }
OUT=~/.cache/the-unmapped/out     # seed42-1024.db, seed42-2048.db: full worlds (README "Flying over a world")
"$B/tools/worldgen-cli/worldgen-cli" generate --seed 42 --map-size 1024 --until regions --out $OUT/seed42-1024-geo.db
"$B/tools/worldgen-cli/worldgen-cli" render --db $OUT/seed42-1024.db --composite territory --markers --out $OUT/seed42-1024.territory-markers.png
X $OUT/seed42-1024.db --at-marker rank:1 --screenshot $OUT/v3d05-city-1024.png
X $OUT/seed42-1024.db --at-marker rank:1 --no-markers --screenshot $OUT/v3d05-city-off-1024.png
X $OUT/seed42-1024.db --at-marker rank:1 --above 40 --pitch -4 --screenshot $OUT/v3d05-street-1024.png
X $OUT/seed42-1024.db --at-marker settlement:<R> --screenshot $OUT/v3d05-ruin-1024.png
X $OUT/seed42-1024.db --at-marker site:<L> --screenshot $OUT/v3d05-lair-1024.png       # likewise -mine, -tower, -outpost
X $OUT/seed42-1024.db --at 512 960 --alt 5000 --pitch -20 --screenshot $OUT/v3d05-air-1024.png
X $OUT/seed42-1024.db --at 512 960 --alt 5000 --pitch -20 --year 3000 --screenshot $OUT/v3d05-air-y3000-1024.png
X $OUT/seed42-2048.db --at 1024 1980 --alt 12000 --pitch -25 --screenshot $OUT/v3d05-air-2048.png
X $OUT/seed42-1024-geo.db --screenshot $OUT/v3d05-nohistory-1024.png
X $OUT/seed42-1024.db --bench;  X $OUT/seed42-2048.db --bench;  X $OUT/seed42-2048.db --no-markers --bench
```

R, L and the mine, tower and outpost ids are chosen by the engineer from `worldgen-cli report --db $OUT/seed42-1024.db --history --level full` and recorded in the README; a kind the world lacks is noted.

As built (r2): not taken: `v3d05-city-off-1024`, `v3d05-air-y3000-1024` (V5 rests on T31 and T35) and the 2048 `--no-markers` bench. The ruin, lair, outpost, mine and tower were not judged up close (V2 and V3 not judged or partly met). The README records the verdicts.

| Id | Good means |
| --- | --- |
| V1 | `v3d05-city-*`: a cluster of houses with roofs in the civilization's colour (the same as `territory-markers.png` at that place), a wall for a city or fortress, a hall or keep for a capital; nothing else differs from `-off` but the beacon and label |
| V2 | `v3d05-street-1024.png`: houses stand on the ground: no gap under any wall, none sunk to its roof |
| V3 | `v3d05-ruin-*`, `-lair-*`, `-mine-*`, `-tower-*`, `-outpost-*`: each reads as its kind (grey broken ring, dark mound with a mouth, headframe over a pit, tall thin tower, square palisade) |
| V4 | `v3d05-air-*`: beacons as thin coloured poles to the haze, at least 4 px wide for settlements; at most 16 labels, none over another's text by more than half, the HUD clear |
| V5 | `v3d05-air-y3000-1024.png` differs from `-air-1024.png` by fewer standing places and fewer ruins |
| V6 | `v3d05-nohistory-1024.png`: the V3D-04 picture, the status line of 8.4 |
| V7 | bench: the per-frame marker work under 0.5 ms; no other bench line changes by more than its noise |

### 12.4 By hand on a GPU (Paul)

| Id | Look for |
| --- | --- |
| G23 | M hides and shows every model, beacon and label; the HUD line follows |
| G24 | N and Shift+N tour the settlements largest first; each lands 500 m south, looking at the place |
| G25 | flying over a region at 200 m/s and with Shift: labels do not flicker; the HUD's nearest marker and bearing follow |
| G26 | houses on steep ground are sunk on the uphill side, never floating |
| G27 | colours match the 2D viewer's territory colours for the same civilizations |
| G28 | from 12 km above the 2048 map the beacons show where the people are; no stall when the markers load |

## 13. Bench (measurements only, no bounds)

After the look line (headless too, when markers are loaded):

```
UNMAPPED-TERRAIN-BENCH: markers: history <ms> ms, records <ms> ms, meshes <ms> ms, nodes <ms> ms; <n> markers (<s> settlements, <r> ruins, <o> old, <l> lairs, <p> outposts, <m> mines, <t> towers), <tri> triangles, beacons <b>
```

Drawn, after the view line: `UNMAPPED-TERRAIN-BENCH: markers per frame over the view: mean <us> us, max <us> us` (beacons, labels and the HUD line). The README records both at 1024 and 2048 with the view line against `--no-markers`.

Measured (r2; seed 42, year 6000, under Xvfb): at 1024, history 6.5 ms, 116 markers: 80 settlements (10 capitals), 20 ruins of which 9 old, 1 lair, 6 outposts, 0 mines, 9 towers; 107 beacons; marker work per frame mean 26 us, max 85 us. At 2048, history 11.9 ms; per frame mean 30 us, max 66 us. In the bench line `<r>` counts ruins that are not old (11 at 1024) and `<o>` the old ones; the status line's `<r> ruins (<o> old)` counts both in `<r>` (20).

## 14. Determinism, CI, to verify

Determinism: the records come from worldgen's integer API in a defined order (3.1); placement, house slots and the house hash are integers; the meshes, beacons, labels, bearing and HUD distances use floating point (`std::sin`, `std::cos`, `std::atan2`, rendering only), are compared with tolerances and never hashed. No unordered container; ties broken by record index. Name matching compares bytes.

CI: no workflow change; `gdext-linux` and `gdext-windows` run U40 to U52 and the longer `gdext-terrain-check` (5.6 s more in release, 12.2). The `build` jobs, `world-hashes.sh` and `compare-gdext-hashes` are unaffected. MSVC and CI results are pending (run 46114777774).

| Id | Assumed | If false | r2 |
| --- | --- | --- | --- |
| W25 | the layers of seed 49 at 512 are the same bytes with and without the history stages (they write tables only); verify with `hash --detail` | keep `w512` geography only and add a full `h512` for T31 to T38 (with its own `biome.png` and `map.png`) | holds: the same layer lines from `hash --detail` with and without; the full world generates in 1.8 s |
| W26 | `Label3D` with `fixed_size` keeps a constant screen size; a `pixel_size` gives 14 to 18 px capitals at 1600 × 900 | `fixed_size` false and the scale set per frame from d · a | holds: `pixel_size` 0.0017, capitals 14 to 15 px |
| W27 | `MultiMesh::set_buffer` each frame for a few hundred instances costs well under 0.5 ms | update the beacons every fourth frame | holds: marker work per frame mean 26 to 30 us, max 85 us |
| W28 | a model's base at the vertex height stays under Terrain3D's coarser far LODs closely enough by 8 km | raise `SINK` to 1.5 m | not judged |
| W29 | `load_history` of a 1024 world takes under about 2 s, so the preview may load it on the main thread | load on a `WorkerThreadPool` task and add the markers when done (as the near tier) | holds: 6.5 ms at 1024, 11.9 ms at 2048 |

## 15. API gaps (for the lead)

1. The 2D marker hover text (`site_marker_text`, `describe_marker`) lives in `gdext/src/viewer/viewer_model.cpp`, which has Godot types and keeps it in an anonymous namespace, so this stage restates it (3.2). A plain `gdext/src/viewer/marker_text.*` used by both views would remove the copy; a gdext refactor, not a worldgen change.
2. The P5-07 parts (`, sealed in <year>`, the warren text) are computed by `HistoryState` from the `deep_woken` events; `HistoryState` is built into the extension only, with the 2D territory state, so the 3D text leaves them out. The same refactor could carry them. worldgen has no single call for "the sealed year of the horror woken at mine s".
3. worldgen has no word for site kinds 1 and 2 (`lair`, `outpost`); they are written here, as the 2D viewer has none.
4. `MarkerIndex::markers_at` gives only sites of kinds 3 and 4; lairs and outposts come from `Timeline::sites_existing` and `site_at` (no gap, two sources).

## 16. Questions for Paul (built as recommended, pending review)

| Id | Question | Recommendation (built) | Reverse with |
| --- | --- | --- | --- |
| Q-V3D-05-1 | Clear vegetation from settlements (a radius in the exclusion rule: changes the far and near instance sets, V3D-03's partition invariants and T12, T13, T17 to T19 with it, and ties vegetation to the year) or leave it and accept trees among houses | leave it: settlements stand mostly on farmland with few trees; a clearing belongs with site plans | nothing to undo; a clearing is a later stage |
| Q-V3D-05-2 | Names as 3D labels (12 nearest within 6 km, seen through hills) or in the HUD only | 3D labels | `LABEL_MAX` 0 |
| Q-V3D-05-3 | N tours settlements by population (largest first) or by distance from the camera | population: a fixed tour, which a distance order is not after each jump | `jump_order` |
| Q-V3D-05-4 | Beacons on ruins and sites too (shorter ranges, none on old ruins) or on standing settlements only | on all but old ruins | the ranges in 6 |
| Q-V3D-05-5 (r2) | A library or mine whose 7-cell offset from its settlement falls in the sea stands in the sea (the Library of Eded at Aleldford, seed 42 at 1024): keep the 2D offset as it is, or move the model | move such a model to the nearest land cell within the offset ring; a viewer-side rule only (the 2D glyph and the world file unchanged) | not built; 4 step 1 as r1 |
| Q-V3D-05-6 (r2) | The mine beacon's dark grey `MARKER_MINE` (60, 60, 60) is near-invisible against forest | a lighter or warmer colour | not built; one constant |
| Q-V3D-05-7 (r2) | Houses 4 to 6 m high are small next to 11 to 15 m trees, and towns in forest are partly hidden by them (ties to Q-V3D-05-1) | a clearing in a later change that restates V3D-03's partition checks | nothing to undo now |
| Q-V3D-05-8 (r2) | Accept the tuned `JUMP_DIST` 500 and `JUMP_ABOVE` 150 (8.3) and the label declutter (7) | accept | r1's 1,200 and 350; `LABEL_DECLUTTER` false |

### Open after r2

1. Q-V3D-05-5 to 8 above.
2. Seen by the lead in the screenshots: distant labels on the horizon still overlap one another side by side (three town names running together; the declutter's half-area rule lets a partial overlap through); beacons lean at the screen edges; a label can sit under the HUD's bottom status lines.
3. MSVC and CI pending (run 46114777774); keys M, N and Shift+N not tried by hand (G23, G24); the 3D marker text restates the 2D viewer's and leaves out the seal year and warren text (15 items 1 and 2).
4. Not taken or not judged: the screenshots and bench of 12.3's note; W28.

No `proposals.md` entry: the stage reads only what the library already gives, and changes neither the design nor the world file.

## Revision notes

- r1 (2026-11-28): first draft, built on its recommendations while Paul is away; his review decides Q-V3D-05-1 to 4.
- r2 (2026-11-28): as built, still interim. `JUMP_DIST` 500 m and `JUMP_ABOVE` 150 m (8.3, 9, U51, G24); the label declutter with `LABEL_DECLUTTER`, `label_box` and `declutter_labels` (7, 9, 10, T34, new U52); the ruin ring's e over the stub slots (5); the 200-byte HUD cut includes the `...`, and `--no-markers` pictures differ from V3D-04's in HUD text (8.2); W25 to W29 marked (14); measurements (12.2, 13); screenshots not taken (12.3); Q-V3D-05-5 to 8 and the open list (16).
