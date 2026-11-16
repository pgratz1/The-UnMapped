# P1-02 Hydrology

Status: approved by the lead, 2026-11-15 (tuning values marked "guess" and the statistics ranges are starting points, to be revised against the first seed sweep); originally a draft, 2026-11-15. Amended by the spec writer, 2026-11-16 (draft, awaiting approval), for the follow-up of the critic's review (`docs/reviews/p1-02-hydrology-critic.md`): a tie-break on flats in H2 (S2, new stream and tuning field, `GENERATOR_VERSION` 0.5.0; re-specified the same day as the engineer's "variant B" after the first form, a G3 flood on fine keys, was found not to break the bundles), known limitations 2 to 5, measured river sources; the river drawing (S1) is in P1-00 10.2. Phase P1 Geography, stage 2 (`hydrology`). Design doc section 4 (stage 2: "flow accumulation gives rivers and lakes; sea level sets the coast"), 6, 7 (rivers carved into chunk heightmaps). Depends on P0-01 to P0-04, P1-00 (shared algorithms G1 to G6, rules D1 to D10) and P1-01 (the `elevation` layer and its postconditions).

## 1. Purpose

From the final terrain, derive where water is and where it goes: the ocean and the coast, lakes and their outlets, a downstream direction for every land cell, how much land drains through each cell, and which cells carry rivers and how large they are. These layers feed climate (coast distance, water), biomes (river and lake special cases), resources (fresh water), regions, history (rivers and lakes as places and borders) and the runtime (water planes and river carving). The stage is analysis: it does not change `elevation`, and its only seed-dependent input is one coordinate noise that breaks ties between cells of equal height (H2), which never changes a lake or a water level.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation` | the stored layer (P1-00 5.1), `i16` decimetres, N × N |
| World seed | `meta.seed`, for the tie-break noise only |
| Tuning | definition `worldgen.hydrology` (section 4) |
| Streams | K_flat = `stream_key(seed, "geo.hydrology.flat")`, a noise key (H2); registered in `streams.hpp`. No `Rng`. |

Preconditions, checked first (H1); a violation is the fatal error ("hydrology: elevation precondition: …"), since it means the elevation stage or a fixture is wrong:

1. The layer is N × N with N = `param.map_size` (library: the grid given).
2. No cell is 0.
3. At least one cell is ocean (e < 0), and the ocean is 4-connected and touches the map edge.

The elevation postcondition "every depression is a kept lake" (P1-01 invariant 7) is not required: hydrology labels every depression it finds as a lake, whatever its size, so it is also correct on hand-made fixtures.

## 3. Outputs

Seven layers, all N × N, codec 1, written in this order in one stage transaction (P1-00 section 4 has the catalogue entries):

| Layer | Type | Content |
| --- | --- | --- |
| `water` | `u8` | 0 dry land, 1 river, 2 lake, 3 ocean |
| `water_depth` | `u16` | decimetres of standing water: ocean −e, lake (lake level − e), else 0 |
| `flow_dir` | `u8` | D8 code (P1-00 2.3) of the downstream cell; 8 on ocean |
| `flow_accum` | `u32` | cells draining through the cell, itself included; 0 on ocean |
| `river` | `u8` | river class 1 to 15, 0 if not a river |
| `lake` | `u32` | lake id from 1, 0 if not a lake |
| `coast_dist` | `i16` | metres to the coast, positive on non-ocean cells, negative on ocean |

## 4. Tuning

Fields of `worldgen.hydrology`, all required (P1-00 6.1):

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `river_min_area_m2` | int(256, 1000000000) | 125000 | designed (lead's decision, 2026-11-16, after the first measurements; was 500000) | upstream area at which a cell becomes a river |
| `flat_noise` | `fbm_m` (P1-01 4.1: `wavelength_m` int(64, 65536), `octaves` int(1, 10), `gain` fx(0.0625, 1)) | wavelength 400, octaves 1, gain "0.5" | guess (the critic's prototype: one octave of value noise on a 24-cell, 384 m lattice) | the tie-break noise of H2; only its order on flats matters |

Derived: T = ceil(`river_min_area_m2` / 256) = (a + 255) div 256 cells; with the default 125000 m² (0.125 km²), T = (125000 + 255) div 256 = 489 (489 cells are 125184 m²). Measured with the implementation at 1024, seeds 0–9: `river_share_of_land` 1.8% to 2.1%. At the former default (500000 m², T = 1954) the share was 0.6% to 0.9% and the 1024 worlds had 45 to 52 river sources; at the shipped value the validation sweep (108 worlds; `docs/reviews/p1-02-hydrology-validation.md`) measured 37–62 / 178–235 / 891–971 river sources and a river share of 1.25%–2.03% / 1.70%–2.27% / 2.31%–2.54% of land at 512 / 1024 / 2048, before the H2 tie-break.

```toml
# game/data/worldgen/hydrology.toml (and the fixture worldgen/tests/data/geo/worldgen/hydrology.toml)
[worldgen.hydrology]
river_min_area_m2 = 125000
flat_noise = { wavelength_m = 400, octaves = 1, gain = "0.5" }
```

Fixed constants (not tuning): river class cap 15; chamfer weights 5 (orthogonal) and 7 (diagonal); 16 m per cell.

## 5. Algorithm

Notation: e[i] the elevation in decimetres as `int64`; O the ocean, {e < 0}; "land" means not ocean (lakes included). Steps H1 to H9 in order.

### H1. Preconditions (D5)

Check section 2. The connectivity check is a 4-connected flood from the ocean cells on the map edge (G2 style); every ocean cell must be reached.

### H2. Routing flood with a tie-break on flats (D1, then D6)

On a decimetre flat every cell has the same flood level, so a flood ordered by level alone pops the flat in FIFO order, and G4 falls back to flood parents everywhere on it: rays leave the flat's exit in the eight directions and neighbouring catchments draw parallel straight 45° rivers one cell apart (critic's S2, 1024 seed 9 at x 546–610, y 762–834). H2 therefore orders the cells of one level by a smooth coordinate noise t, so the flood tree on a flat grows lowest-t first and merges flows into trunks, while the decimetre levels themselves are those of a plain priority flood.

1. **Tie value** t[i] (`int64`), a per-cell function (D1, G6 row blocks):
   ```
   ocean:  t = 0
   land:   X = from_int(16x + 8); Y = from_int(16y + 8)            // cell centre, metres (P1-00 2.1)
           nz = fbm_m(K_flat, X, Y, flat_noise)                     // P1-01 4.1, nz in [−ONE, ONE]
           t  = min(65535, (nz.raw + 2^32) >> 17)                    // int64, in [0, 65535]
   ```
   (nz.raw + 2^32 ∈ [0, 2^33], so the shift gives [0, 65536] before the `min`.) Noise coordinates are bounded as in P1-01 7 (X, Y ≤ 32760, wavelength ≥ 64).
2. **Flood** (D6, single-threaded; a function of hydrology, not the shared G3, which is unchanged). Outputs: the flood level L[i] in decimetres (`int64`), the flood parent `parent[i]` (a D8 code, 8 on ocean) and `order`, the cells in pop order.
   ```
   seq = 0; Q = empty                                   // entries (key, seq, cell)
   for i in 0 .. n − 1 (index order):
       if i in O: L[i] = 0; parent[i] = 8; visited[i] = true; push (0, seq, i); seq += 1
   while Q not empty:
       pop the entry (key, s, c) with the smallest (key, s)
       append c to order
       for code in 0 .. 7 (8-neighbour order):
           m = neighbour of c in direction code; skip if outside the map or visited[m]
           visited[m] = true                            // a cell is visited when it is pushed, once
           L[m] = max(e[m], L[c]); parent[m] = (code + 4) mod 8
           push (L[m] × 2^16 + t[m], seq, m); seq += 1
   ```
   A cell is marked visited when it is pushed, so it is pushed once, from the first popped neighbour that reaches it, and that neighbour is its parent. Keys: ocean 0; land L × 2^16 + t with L ∈ [1, 32767] (the preconditions), so keys lie in [65536, 2^31). **Queue contract:** any priority queue that pops the entry with the smallest (key, seq), seq being the global push counter from 0; (key, seq) pairs are unique, so the pop order is defined by this text alone. Keys are *not* non-decreasing over the pops (a cell pushed with a smaller t than the cell just popped has a smaller key), so the monotone radix heap or bucket queue of G3 does not apply; a binary heap with this comparison does (O(n log n)).

**Pops are non-decreasing in L.** Because 0 ≤ t < 2^16, key order refines L order: L[a] < L[b] implies key(a) < key(b). Claim: when c is popped, every entry in the queue has L ≥ L[c]. Initially all entries are ocean cells with L = 0. When c is popped it has the smallest key, so the smallest L, of all entries; every entry pushed while c is processed has L[m] = max(e[m], L[c]) ≥ L[c]. So the L of successive pops never decreases. All ocean cells (key 0) are popped before any land cell (key ≥ 65536).

**L is the minimax level, whatever t is.** Write M(i) for the minimum over all 8-paths from i to an ocean cell of the largest e on the path, ocean cells counting 0. Claim: L[i] = M(i) for every cell. First, L[i] ≥ M(i): following parents from i reaches the ocean, and L[i] = max(e[i], L[parent]) is by induction the largest e on that path. Second, L[c] ≤ M(c), by induction over the pop order: ocean cells have L = 0 = M. Let c be popped, assume the claim for every cell popped before c, and suppose L[c] > λ = M(c). Take a path P from c to the ocean whose largest e is λ, and let y be the first cell of P, counted from its ocean end, that has not been popped yet (y exists, since c is on P and not yet popped). Its predecessor x on P has been popped, with L[x] = M(x) ≤ λ (the rest of P from x is a path with largest e ≤ λ). When x was popped, y was pushed by x if it was not visited yet; otherwise it had been pushed by a cell z popped before x. Either way y's pusher was popped no later than x, so it has L ≤ L[x] ≤ λ (pops are non-decreasing in L), and L[y] = max(e[y], L[pusher]) ≤ λ, since e[y] ≤ λ. If y = c this contradicts L[c] > λ. Otherwise y is in the queue when c is popped, with L[y] ≤ λ < L[c], so key(y) < key(c): a contradiction, since c had the smallest key. Hence L = M.

So L, the lake cells (L > e), the lake components and ids (H3, keyed on L), the lake levels, `water_depth`, `lake`, the cells with `water` 2 and 3, `coast_dist` and invariant 3's surface W do not depend on t (invariant 11). What t does change: `parent`, `order`, which lake cell is the outlet, and so `flow_dir`, `flow_accum`, `river` and which land cells are river cells (`water` 1), with the statistics built on them.

**t = 0 is the stage before the tie-break, byte for byte.** With t = 0 every key is L × 2^16, and L[m] = max(e[m], L[c]) is exactly G3's L[m] = max(h[m], L[c]) for keys h = e; the pushes happen in the same order with the same seq, and scaling all keys by 2^16 does not change the (key, seq) order, so `order`, `parent` and L equal G3's on keys e. H3 to H9 are unchanged. Hence with t = 0 every layer equals that of `GENERATOR_VERSION` 0.4.0, and the worked example of 5.1 and the unit tests of 8.1, given for t = 0 (the seams take t as a parameter, section 8), stand. A worked example where t decides the route is in 5.2.

### H3. Lakes (D5, G2)

Lake cells: land cells c with L[c] > e[c]. Lakes are the components of lake cells by G2 with 8-connectivity and the equivalence "adjacent cells belong together only if their L are equal". Lake ids are the G2 labels (1, 2, … in order of each lake's smallest index). The lake level of lake k is the common L of its cells.

Every neighbour of a lake cell has L at least the lake level (for adjacent u, v: L[u] ≤ max(e[u], L[v]); with e[u] < L[u] this forces L[v] ≥ L[u]). A neighbour with L equal to the lake level is either in the same lake or a land cell with e equal to the lake level: the rim, over which the lake spills.

### H4. Receivers (D1 then D4, D5)

1. `rcv` = G4 on L (decimetres), parent, O for every land cell, exactly as in 0.4.0: the neighbour with the steepest strictly lower L, else the flood parent of H2. (Parallel by G6 row blocks.) On a decimetre flat no neighbour is strictly lower, so the flood tree H2 grew lowest-t first decides the route.
2. **One outlet per lake.** For each lake, in id order: the outlet is the lake cell that comes first in `order`. Its receiver is its flood parent, `rcv[outlet] = parent[outlet]`, which is a rim cell: a land cell outside the lake whose e and L both equal the lake level. Argument: the parent p was popped before the outlet, and the outlet is the first lake cell popped, so p is not in this lake. H2 set L[outlet] = max(e[outlet], L[p]) when p pushed it; the outlet is a lake cell, so L[outlet] > e[outlet], hence L[p] = L[outlet], the lake level ≥ 1, and p is not ocean (ocean L is 0). If p were depressed (L[p] > e[p]) it would be an adjacent depressed cell with equal L and so in the same lake (H3); hence e[p] = L[p], the lake level. The implementation checks this and treats a violation as the fatal error "hydrology: a lake outlet's flood parent is not a rim cell at the lake level"; by the argument it cannot occur on input that meets the preconditions, so it guards the implementation, not the input. Every other cell of the lake gets as receiver its parent in a FIFO breadth-first search over the lake's cells starting at the outlet, visiting neighbours in the 8-neighbour order: when the search reaches cell m from cell c through direction code k, `rcv[m] = (k + 4) mod 8`.

Without step 2, a lake reached from several rim cells at the same level would have several exits (G4 gives every lake cell its flood parent, and several lake cells can have rim parents). Step 2 makes the lake one node of the drainage network with one outflow, which history and the runtime need.

### H5. Accumulation (D4)

A[c] for land cells as defined by G5. After H4 step 2, H2's `order` is no longer a topological order of `rcv` inside lakes, so the reference computes it by Kahn's method: indeg[c] = number of land cells u whose receiver is c; a FIFO queue starts with every land cell with indeg 0 in index order; popping u, if its receiver m is land: A[m] += A[u], indeg[m] −= 1, push m when it reaches 0. A is a sum of integers, so every topological order gives the same values. A ≤ N² ≤ 2^22 (`uint32`).

### H6. Classification (D1)

For each cell:

| Cell | `water` | `river` | `water_depth` | `lake` | `flow_dir` | `flow_accum` |
| --- | --- | --- | --- | --- | --- | --- |
| ocean | 3 | 0 | −e | 0 | 8 | 0 |
| lake cell of lake k | 2 | 0 | L − e | k | `rcv` | A |
| other land, A ≥ T | 1 | min(15, bit_length(A div T)) | 0 | 0 | `rcv` | A |
| other land, A < T | 0 | 0 | 0 | 0 | `rcv` | A |

`bit_length` is amendment A1 of P1-00. Class k ≥ 1 means A in [T·2^(k−1), T·2^k), so each class step is a doubling of upstream area (roughly a 40% wider river under the usual width ∝ √area rule). Bounds, for any `i16` elevation that meets the preconditions: on ocean −e ∈ [1, 32768] (e = −32768 gives 32768); on a lake L ≤ 32767 (the largest land key) and e ≥ 1, so L − e ∈ [1, 32766]. Both fit `u16`. On layers written by the elevation stage (land ≤ 29990, ocean ≥ −29005, P1-01 6.1) both are below 30000.

### H7. Coast distance (D4)

A two-pass chamfer distance transform with weights 5 (orthogonal) and 7 (diagonal), run twice:

```
chamfer(source):                       // source[i] true where the distance is 0
    d[i] = source[i] ? 0 : INF          // INF = 2^40
    forward, i in index order:  for (dx, dy, w) in (-1,0,5), (-1,-1,7), (0,-1,5), (1,-1,7):
                                     if the neighbour exists: d[i] = min(d[i], d[neighbour] + w)
    backward, i in reverse index order: for (dx, dy, w) in (1,0,5), (1,1,7), (0,1,5), (-1,1,7):
                                     if the neighbour exists: d[i] = min(d[i], d[neighbour] + w)
    return d

dl = chamfer(source = ocean)            // for land cells
do = chamfer(source = land)             // for ocean cells
coast_dist[i] = land:  min(32767, (32 × dl[i] + 5) div 10)        // RHA(dl × 16 / 5), metres
                ocean: −min(32767, (32 × do[i] + 5) div 10)
```

The two passes give, for each cell, the minimum over 8-connected paths to a source of the sum of step weights (Borgefors 1986); the unit is a fifth of a cell, so metres = d × 16 / 5. The error against Euclidean distance is at most about 4%. Lakes count as land: this is the distance to the sea.

### H8. Flow direction

`flow_dir` as in H6.

### H9. Write

In one transaction, in this order: `water`, `water_depth`, `flow_dir`, `flow_accum`, `river`, `lake`, `coast_dist`.

### 5.1 Worked example

The example is 8 wide and 7 tall (the library functions accept any W × H). T = 3 (`river_min_area_m2` 513 to 768). Elevation e (decimetres):

```
 y\x   0    1    2    3    4    5    6    7
  0   -5   -5   -5   -5   -5   -5   -5   -5
  1   -5   60   70   80   70   60   50   -5
  2   -5   50   20   22   60   40   30   -5
  3   -5   45   21   23   55   35   25   -5
  4   -5   40   30   28   40   30   20   -5
  5   -5   35   33   31   25   20   10   -5
  6   -5   -5   -5   -5   -5   -5   -5   -5
```

**H2.** Flood level L (ocean 0); the 2 × 2 basin at (2..3, 2..3) fills to the spill at (3, 4), 28:

```
  1    0   60   70   80   70   60   50    0
  2    0   50   28   28   60   40   30    0
  3    0   45   28   28   55   35   25    0
  4    0   40   30   28   40   30   20    0
  5    0   35   33   31   25   20   10    0
```

(rows 0 and 6 are 0). Flood parent (8 on ocean):

```
  1    8    5    5    5    5    5    5    8
  2    8    5    2    3    3    1    7    8
  3    8    5    1    2    3    1    7    8
  4    8    5    0    1    1    1    7    8
  5    8    5    3    3    3    3    7    8
```

Pop order (cell indices, i = 8y + x): 0, 1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 49, 50, 51, 52, 53, 54, 55, 46, 38, 45, 30, 44, 35, 26, 27, 18, 19, 22, 37, 34, 43, 42, 41, 29, 33, 36, 21, 25, 14, 17, 28, 9, 13, 20, 10, 12, 11.

**H3.** One lake, id 1: cells (2, 2), (3, 2), (2, 3), (3, 3), level 28, depths 8, 6, 7, 5.

**H4.** G4 gives the lake cells their flood parents (no neighbour has a lower L): (2, 3) → SE to (3, 4) and (3, 3) → S to (3, 4), two exits. The outlet is (2, 3) (index 26, popped before 27, 18, 19), receiver SE (code 1) to the rim cell (3, 4). BFS from (2, 3): code 0 reaches (3, 3) (receiver W, 4), code 6 reaches (2, 2) (receiver S, 2), code 7 reaches (3, 2) (receiver SW, 3). Final `flow_dir`:

```
  0    8    8    8    8    8    8    8    8
  1    8    4    6    6    6    6    0    8
  2    8    4    2    3    4    1    0    8
  3    8    4    1    4    4    1    0    8
  4    8    4    0    1    2    1    0    8
  5    8    2    2    2    2    2    0    8
  6    8    8    8    8    8    8    8    8
```

**H5.** `flow_accum`:

```
  1    0    1    1    1    1    1    1    0
  2    0    1    1    2    1    1    1    0
  3    0    1    6    2    1    1    2    0
  4    0    1    1    8    1    1    2    0
  5    0    1    1    1   10    1    2    0
```

(rows 0 and 6 are 0). The outlet (2, 3) collects the four lake cells, with (4, 2) draining into (3, 2) and (4, 3) into (3, 3): 1 + 1 + 2 + 2 = 6. The rim (3, 4) has 1 + 6 + 1 (from (2, 4)) = 8, and (4, 5) has 1 + 8 + 1 (from (4, 4)) = 10.

**H6.** River cells (A ≥ 3, not lake): (3, 4) with A 8, class bit_length(8 div 3 = 2) = 2; (4, 5) with A 10, class bit_length(3) = 2. `water`:

```
  0    3    3    3    3    3    3    3    3
  1    3    0    0    0    0    0    0    3
  2    3    0    2    2    0    0    0    3
  3    3    0    2    2    0    0    0    3
  4    3    0    0    1    0    0    0    3
  5    3    0    0    0    1    0    0    3
  6    3    3    3    3    3    3    3    3
```

`water_depth`: 5 on every ocean cell; 8, 6 (row 2) and 7, 5 (row 3) on the lake; 0 elsewhere. `lake`: 1 on the four lake cells, 0 elsewhere.

**H7.** `coast_dist` (metres):

```
  0  -22  -16  -16  -16  -16  -16  -16  -22
  1  -16   16   16   16   16   16   16  -16
  2  -16   16   32   32   32   32   16  -16
  3  -16   16   32   48   48   32   16  -16
  4  -16   16   32   32   32   32   16  -16
  5  -16   16   16   16   16   16   16  -16
  6  -22  -16  -16  -16  -16  -16  -16  -22
```

For example (3, 3): three orthogonal steps to the sea, chamfer 15, 15 × 16 / 5 = 48 m; the corner (0, 0): one diagonal step to (1, 1), chamfer 7, (32 × 7 + 5) div 10 = 22 m.

More river class values with T = 3: A = 3, 5 → class 1; A = 6, 11 → 2; A = 12 → 3; A = 3 × 2^14 = 49152 → 15; A = 3 × 2^20 → 15 (capped).

### 5.2 Worked example: the tie-break on a flat

A 5 × 4 test grid: column x = 0 ocean (−5), every other cell 10 dm, a flat with one exit side. Tie values t (row y = 0 to 3; ocean 0):

```
  0  900  800  700  600
  0  500   50  400  300
  0  200  100   60  650
  0  990  950  980  970
```

Cell indices i = 5y + x. With **t = 0** (0.4.0): land pop order 1, 6, 11, 16, 2, 7, 12, 17, 3, 8, 13, 18, 4, 9, 14, 19 (FIFO by push order); `flow_dir` rows `8 4 4 4 4`, `8 4 5 5 5`, `8 4 5 5 5`, `8 4 5 5 5` (column 1 drains west to the sea; the other cells take their flood parents, west on row 0 and north-west below it: parallel rays); `flow_accum` rows `0 10 6 3 1`, `0 3 3 2 1`, `0 2 2 2 1`, `0 1 1 1 1`.

With **the t above**: the four column-1 cells are pushed from the ocean (seq 4 to 7: cells 1, 6, 11, 16) and popped lowest-t first, so 11 (t 200) goes first and its neighbours are claimed from it. Land pop order: 11, 7, 13, 12, 9, 8, 6, 4, 14, 3, 2, 1, 17, 19, 18, 16. `flow_dir` rows `8 4 2 3 2`, `8 4 3 4 3`, `8 4 4 5 4`, `8 4 5 6 5`; `flow_accum` rows `0 1 1 1 1`, `0 1 10 1 2`, `0 13 1 6 1`, `0 1 1 1 1`. The flow that t = 0 spreads over four parallel exits gathers into one trunk through (2, 1) and (1, 2), which carries 13 of the 16 land cells to the sea. L = 10 on every land cell and there is no lake in either case (invariant 11).

## 6. Invariants

For every input satisfying the preconditions (and so for every world):

1. `flow_dir` = 8 exactly on ocean cells; elsewhere it is 0 to 7 and points to a cell inside the map.
2. **Every land cell drains to the sea.** Following `flow_dir` from any land cell reaches an ocean cell in fewer than N² steps (no cycles). Every lake drains through exactly one outlet; there are no endorheic basins in P1 (open question 1).
3. **No river flows uphill.** With the surface W = e + `water_depth` on land (e on dry land and rivers, the lake level on lakes) and W = 0 on ocean: W[down(c)] ≤ W[c] for every land cell c. W equals the flood level L on every land cell. It holds with the H2 tie-break: a G4 receiver has strictly smaller L; a flood-parent receiver p has L[p] ≤ max(e[c], L[p]) = L[c]; inside a lake every receiver joins cells of the one lake level, and the outlet's receiver is a rim cell at that level (H4). No cycles: P1-00 7.4's argument holds, since a parent step goes to an earlier-popped cell with L no greater.
4. **Accumulation is local.** For every land cell, `flow_accum` = 1 + the sum of `flow_accum` over the cells whose `flow_dir` points to it; 0 on ocean.
5. **Rivers.** `river` > 0 ⟺ `water` = 1 ⟺ (land, not lake, `flow_accum` ≥ T), and then `river` = min(15, bit_length(`flow_accum` div T)). The downstream cell of a river cell is a river, lake or ocean cell, and along a river the class never decreases downstream.
6. **Lakes.** Each lake id 1 to K occurs; its cells form one 8-connected set with one common W (the lake level) and positive depth; exactly one of its cells (the outlet) has a downstream cell outside the lake, and that cell is a land cell with e equal to the lake level. Lake cells are exactly the land cells with L > e (every depression of the stored terrain is a lake). Lake ids increase with each lake's smallest index.
7. **Water and depth.** `water` = 3 ⟺ e < 0 ⟺ `water_depth` = −e > 0; `water` = 2 ⟺ `lake` > 0 ⟺ (land and `water_depth` > 0); dry land and rivers have depth 0.
8. **The ocean is one connected region touching the map edge** (4-connected), re-checked on the output `water` layer.
9. **Coast distance.** `coast_dist` > 0 on every land cell and < 0 on every ocean cell; a land cell 4-adjacent to the ocean has 16; an ocean cell 4-adjacent to land has −16; |`coast_dist`| equals the chamfer path minimum (checked by Dijkstra on small grids).
10. Pure function of `elevation`, the world seed (through the tie noise only) and `worldgen.hydrology`. The layers `water_depth`, `lake` and `coast_dist`, and the cells with `water` 2 or 3, do not depend on the seed or on `flat_noise` (H2).
11. **Tie-break invariance.** For any tie values t, the flood level L, the lake cells, lake ids and levels, and the layers `water_depth`, `lake` and `coast_dist` equal those for t = 0 (H2's proof that L is the minimax level). In `water` only the values 2 (lake) and 3 (ocean) are invariant: river cells (1) and dry land (0) may trade places. With t = 0 the stage output is that of 0.4.0 byte for byte.

## 7. Determinism notes

| Step | Rule | Notes |
| --- | --- | --- |
| H1, H3 | D5 | FIFO, index-order seeds, neighbour orders of P1-00 2.3 |
| H2 tie value | D1, parallel | location-keyed noise only (D9); one `fbm_m` per land cell |
| H2 flood | D6 | hydrology's own flood (not G3); keys L × 2^16 + t in [0, 2^31); any priority queue popping the smallest (key, seq) is correct; keys are not monotone over pops, so not G3's radix heap |
| H4 step 1 | D1, parallel | |
| H4 step 2 | D5 | outlet by pop position; BFS parents depend on the stated neighbour order |
| H5 | D4 | integer sums; result order-independent |
| H6 | D1, parallel | |
| H7 | D4 | each pass is an in-place sweep in a stated order; not parallel |

No `Rng`, no floating point; the only seed-dependent value is the tie noise of H2. Overflow: flood keys < 2^31; G4 on decimetre levels (drops ≤ 32767); A ≤ 2^22; finite chamfer values are below 7 × 2 × 2048 < 2^15, so 32 × d < 2^20; INF = 2^40 plus a weight, and 32 × INF, still fit `int64`; an INF left over (only on a test grid with no land) saturates to ∓32767.

Performance (P1-00 11): 0.3 / 1.2 / 5 s at 512 / 1024 / 2048. The tie noise (one octave per land cell, parallel) and the two chamfer transforms are linear in N²; the flood uses a comparison heap, O(N² log N²) (about 22 comparisons per push at 2048), single-threaded. The budget is unchanged; the validation specialist measures the stage after the change.

## 8. Tests required

Library seams, as declared on the implementation branch `p1/02-hydrology` (`worldgen/include/worldgen/stages/hydrology.hpp`, namespace `worldgen::stages`, `threads` defaults to 1), with the changes this amendment needs (marked `// S2`), plus the shared G3, G4, G5 functions of `worldgen/geo/flow.hpp` on any W × H grid. The tie values are a parameter so that unit tests and worked examples can use t = 0 (an empty span means t = 0 everywhere):

```cpp
struct HydrologyTuning { std::int64_t river_min_area_m2; geo::FbmM flat_noise; };   // S2: flat_noise
HydrologyTuning hydrology_tuning_from(const data::ContentSet&);          // and (const data::Record&)
std::int64_t river_threshold_cells(std::int64_t river_min_area_m2);      // T
std::uint8_t river_class(std::int64_t a, std::int64_t t);               // min(15, bit_length(a div t)), 0 if a < t
void check_hydrology_preconditions(geo::Grid, std::span<const std::int16_t> e);   // H1, fatal on violation
struct HydrologyNetwork {
    std::vector<std::uint8_t> ocean;          // O
    std::vector<std::int64_t> level_dm;       // H2: L, decimetres (no fine-unit level is kept)
    std::vector<geo::Direction> parent;       // H2: flood parent, 8 on ocean
    std::vector<geo::CellIndex> order;        // H2: pop order
    std::vector<geo::Direction> g4;           // H4 step 1, before rerouting (8 on ocean)
    geo::Components lakes;                    // H3, label = lake id
    std::vector<std::int64_t> lake_level;     // [k - 1]
    std::vector<geo::CellIndex> lake_outlet;  // [k - 1]
    std::vector<geo::Direction> rcv;          // H4 final receivers (flow_dir)
    std::vector<std::uint32_t> accum;         // H5
};
std::vector<std::uint16_t> flat_tie(std::uint64_t seed, geo::Grid, std::span<const std::uint8_t> ocean,
                                    const HydrologyTuning&, std::uint32_t threads);       // S2: H2 step 1, 0 on ocean
HydrologyNetwork hydrology_network(geo::Grid, std::span<const std::int16_t> e,
                                   std::span<const std::uint16_t> tie, std::uint32_t threads);   // S2: tie
std::vector<std::int16_t> coast_distance(geo::Grid, std::span<const std::uint8_t> ocean);   // H7
struct HydrologyLayers { std::vector<std::uint8_t> water; std::vector<std::uint16_t> water_depth;
                         std::vector<std::uint8_t> flow_dir; std::vector<std::uint32_t> flow_accum;
                         std::vector<std::uint8_t> river; std::vector<std::uint32_t> lake;
                         std::vector<std::int16_t> coast_dist; };
HydrologyLayers hydrology_layers(geo::Grid, std::span<const std::int16_t> e, std::span<const std::uint16_t> tie,
                                 const HydrologyTuning&, std::uint32_t threads);   // S2: tie; the stage passes flat_tie(seed, ...)
```

### 8.1 Unit tests

1. **Worked example 5.1** exactly, with t = 0: L, parent, `order`, every output grid. **Worked example 5.2** exactly: `order`, `flow_dir` and `flow_accum` for both tie settings. Also the pre-rerouting receivers (lake cells with their flood parents) from G4 alone.
2. **Flood properties on the examples:** pops are non-decreasing in L (and, with t = 0, in key); each cell's parent and each strictly lower neighbour precede it in `order`.
3. **Score ties.** A 6 × 3 grid: rows 0 and 2 ocean (−1); row 1 is ocean (−1) at x = 0, then 10 at x = 1 to 5 (land touching the east edge is allowed on test grids). Every land cell has ocean neighbours: orthogonal ones score 2 × 10² = 200, diagonal ones 100, and among equal scores the lowest code wins, so S (2) beats W (4) and N (6). Expected `flow_dir` row 1: 8, 2, 2, 2, 2, 2.
4. **A plateau.** 7 × 5, border −1, all 15 interior cells 10. Expected `flow_dir`, rows 1 to 3: `8 4 6 6 6 0 8`, `8 4 5 5 5 0 8`, `8 2 2 2 2 0 8`. The three cells (2..4, 2) have no lower neighbour and take their flood parents (NW, code 5); (1, 1) takes W (score 200, the first 200 in code order after the diagonal SW at 100); (2, 1) to (4, 1) take N. `flow_accum` rows 1 to 3: `0 2 2 2 1 1 0`, `0 1 1 1 1 1 0`, `0 1 1 1 1 1 0`.
5. **Two lakes at different levels draining one into the other** (a terraced fixture designed by the test writer, at least 9 × 9): the upper lake's outlet drains over its rim into a path that enters the lower lake; ids follow the smallest index; each lake has one outlet.
6. **A lake reached from several rim cells:** a fixture where two rim cells at the lake level touch the lake; exactly one cell (the first popped) exits.
7. **River classes** of 5.1's list; `river_min_area_m2` 256 gives T = 1, 257 gives T = 2, 2048 gives 8, 125000 (shipped) gives 489, 500000 gives 1954.
8. **Chamfer** (the H7 function alone; this grid does not meet the stage preconditions) on 5.1 and on a 9 × 9 grid that is land except one ocean cell at (4, 4): (4, 4) → −16; (5, 4) chamfer 5 → 16 m; (5, 5) 7 → 22 m; (6, 5) 12 → 38 m; (8, 8) 28 → 90 m; (8, 4) 20 → 64 m; (0, 0) 28 → 90 m.
9. **Preconditions:** a grid with a 0 cell, with no ocean, and with an ocean pocket not connected to the edge each raise the fatal error.

### 8.2 Property tests

Over seeds 0 to 63 at N = 128 and 0 to 3 at N = 512 (fixture tuning, elevation from P1-01; with the shipped elevation values a map smaller than 187 cells has no lake, P1-01 4.4, so the N = 128 seeds also run with the small-map fixture `worldgen/tests/data/geo-small/`), and over 200 random fixtures, which make many depressions, flats and lakes. Fixture f (f = 0 … 199) is a 34 × 34 grid drawn from `Rng(substream(stream_key(0, "test.hydro"), f))` (`test.hydro` registered in `streams.hpp` as a test-only stream name), one draw per cell in index order: a border cell (x or y equal to 0 or 33) gets `uniform_i64(-50, -1)`, an interior cell `uniform_i64(1, 200)`. Each fixture is run twice, with river threshold 2048 m² (T = 8) and with T = 1 (256 m², every land cell not in a lake is a river), each with t = 0 and with random tie values: for fixture f, t on each land cell in index order from `uniform_i64(0, 65535)` of `Rng(substream(stream_key(0, "test.hydro"), 1000 + f))` (0 on ocean). The two tie settings must give equal `water_depth`, `lake` and `coast_dist` and equal lake and ocean cells (invariant 11); the world seeds above are also run with t = 0 and with `flat_tie` and compared the same way. Unit tests of `flat_tie`: t = 0 on ocean, t ∈ [0, 65535] on land, and t at three cells of seed 0, N = 64, equal to the test's own `fbm_m` evaluation. Check invariants 1 to 11 with code written independently of the stage: flood levels by a Dijkstra-style minimax search, accumulation by recursion over upstream cells, components by a union–find, chamfer by Dijkstra.

### 8.3 Determinism tests

- Threads (D10): 1, 2 and 5 threads give identical layers for seeds 0 and 42 at N = 128 and 512.
- Resume (P1-00 X2): hydrology on the `elevation` read back from a written world equals the uninterrupted pipeline.
- Queue independence: the H2 flood computed with two different priority-queue implementations in the test (a binary heap and a sorted-set or pairing heap, both ordered by (key, seq)) gives identical `order` and `parent` on the random fixtures with random t; with t = 0 a bucket queue over decimetre levels also gives the same order, and equals G3 on keys e.

### 8.4 Golden values

With the H2 tie-break (`GENERATOR_VERSION` 0.5.0, content hash changed by `flat_noise`) the hydrology layer goldens and the world-file goldens are re-recorded; the `elevation` layer hashes do not change, and of the hydrology layers `water_depth`, `lake` and `coast_dist` keep their 0.4.0 layer hashes (invariant 11), which the re-recording checks.

Layer hashes (P0-04 8) of all seven layers, fixture tuning, compared on every preset: seeds 0, 42 and the phrase "The Unmapped" at N = 512, seed 1 at N = 1024, seed 0 at N = 128 (library). The hydrology golden test may read its `elevation` input from the elevation golden case, so a hydrology failure is not masked by an elevation change: when the elevation golden changes, both are re-recorded together.

## 9. Decisions on rivers, lakes and terrain

### 9.1 Hydrology does not feed back into elevation

The final elevation is `elevation`, written once by the elevation stage. Hydrology does not carve valleys or fill pits in it:

- Valleys are cut by the elevation stage's erosion (P1-01 E8), which routes water with the same G3 flood and G4 receivers, then draws each cell's receiver at random among its lower neighbours (weighted by slope²) so that incision spreads over a slope instead of cutting straight gullies. Hydrology's own `flow_dir` is plain G4 (with H2's flood parents on flats) on the stored terrain, so a river need not follow the exact cells erosion last used; it runs down the valleys erosion cut.
- Depressions that do not become lakes (too small, too shallow, too large for the map, or beyond the lake share of land) are filled by the elevation stage's last step (P1-01 E10), so the stored terrain drains everywhere except into the kept lakes, and hydrology finds exactly those lakes.
- The river channel itself (metres wide, below the 16 m cell) is carved by the runtime when it builds chunk heightmaps (design doc section 7), from `river`, `flow_dir` and `water_depth`.

So there is one terrain layer, and every stage and the game read the same one. The alternative (hydrology writes a second, carved elevation) would leave two candidate terrains and a question in every later spec about which to use.

### 9.2 Rivers are rasters in P1

Rivers are stored as `flow_dir`, `flow_accum` and `river` only, with no polyline table:

- The rasters determine the river network completely. Tracing downstream from each river source along `flow_dir` gives every river as a cell path, deterministically, whenever it is needed.
- A cell-centre polyline is a staircase of D8 steps. The runtime has to smooth and meander it at sub-cell scale with seeded detail anyway; that belongs to runtime realization, where every piece is a function of the seed and coordinates.
- A river table needs decisions history will make: names, which reaches count as one river, fords and bridges. Adding the table then, derived from these rasters, changes no layer and so no earlier golden value.
- Roads are different: they are history's output, not derivable from any raster, so the design's `road` table stands.

Proposals entry (B4 of P1-00):

> ### Rivers are stored as rasters in P1; a river table comes with history
>
> - Date: 2026-11-15
> - Raised by: spec-writer
> - Design doc section: 6 (tables: `road` has polylines, nothing for rivers), 7 ("Rivers, lakes and roads are carved into chunk heightmaps from the database layers and polylines")
> - Problem: the design does not say whether rivers are stored as polylines, and section 6 has no table for them.
> - Proposal: P1 stores rivers as rasters: downstream direction, upstream area and a size class per cell, plus lake ids and water depth. The runtime traces and smooths river paths from these. When history needs named rivers (P3), a `river` table (id, name, source and mouth cells, length, and the cell path as a blob) is added, derived from the rasters. Section 7 would read "rivers and lakes are carved from the database layers; roads from their polylines".
> - Interim choice: as proposed, in `docs/specs/p1-02-hydrology.md` section 9.
> - Status: open

Proposals entry (B5 of P1-00):

> ### One terrain layer; hydrology does not carve it
>
> - Date: 2026-11-15
> - Raised by: spec-writer
> - Design doc section: 4 (stages 1 and 2)
> - Problem: the design puts erosion in stage 1 and rivers and lakes in stage 2 but does not say whether stage 2 changes the terrain, so it is unclear which layer is the final elevation.
> - Proposal: the elevation stage's output is the final terrain. It ends by filling every depression that does not pass its lake test (size, depth and a cap on the lake share of land), so every remaining depression is a lake. Hydrology only analyses the terrain; river channels narrower than a cell are carved at runtime.
> - Interim choice: as proposed, in p1-01 section 5 (E10) and p1-02 section 9.
> - Status: open

## 10. Open questions

1. **No endorheic lakes.** Every lake spills to the sea, because hydrology runs before climate and cannot know where evaporation exceeds inflow. A closed basin (a salt lake in a rain shadow) would need climate first, or a later pass after climate that lowers the levels of lakes whose catchment rainfall is below evaporation. Interim: none in P1.
2. **Uniform rain.** `flow_accum` counts cells, as if every cell got the same rain. Rivers in a rain shadow are therefore as large as rivers on the wet side. Options for the second pass: a `discharge` layer written by the climate stage (rainfall-weighted accumulation, using these `flow_dir` values unchanged), used by biomes and resources instead of `flow_accum`; or leave it. Decide in the climate spec.
3. **River threshold.** Decided by the lead: 0.125 km² (T = 489), replacing the first guess of 0.5 km², which gave a river share of 0.4% to 0.9% of land on the v16 terrain. The validation sweep confirms or revises it. The river class definition does not depend on it; statistics and second-pass calibration that do are marked provisional (P1-00 13.1, P1-04, P1-05, P1-06).
4. **Coast distance metric.** Chamfer 5-7 (error up to about 4%) is chosen over exact Euclidean distance for simplicity and exactness of definition. Climate and biomes need "near the coast", not survey accuracy.

## 11. Known limitations

1. **Small lakes may show no outflow.** Every lake has exactly one outlet (H4), but its outflow is drawn as a river only where the accumulation reaches T. A lake whose catchment, itself included, is smaller than T cells (0.125 km² with the shipped threshold) drains through dry-land cells (`water` = 0) to the next river, lake or the sea, so on the composite map it looks closed. `flow_dir` still records the outflow; the runtime and history can use it. P1-00 13.2 checklist item 7 is read accordingly. With the shipped values it did not occur in the validation sweep (536 lakes in 108 worlds) or the critic's 27 worlds: the smallest kept lake (391 cells) plus its catchment almost always reaches T at its outlet.
2. **Angular lowland trunks.** On gentle lowland, larger rivers run in straight 0°, 45° and 90° segments with sharp turns ("staircase" rivers; 1024 seed 13 interior basin x 440–640, y 400–600; seed 1 north plain, a 1.2 km diagonal from (660, 323) to (717, 390); 2048 seed 1 east plain). They are inherited from the decimetre terrain: the flats there are narrow channel floors and terraces cut by erosion, so the path is fixed by elevation and only the choice of cell within a one-decimetre band is free. The H2 tie-break does not remove them, and nothing in hydrology can without crossing decimetre steps, which would break invariant 3. Revisit with the next elevation revision; walking-scale smoothing is the runtime's (9.2).
3. **Trunks parallel to the coast.** Where a coastal rim from elevation blocks the short way to the sea, a trunk can run along the coast for kilometres (1024 seed 1: the largest river, class 8 to 9, about 600 m inland for about 4 km on a level 61 m plateau, reaching the sea at (910, 401)). Routed correctly; it reads odd but legible.
4. **Single-cell mouths.** Rivers reach the sea through one cell, with no estuaries or deltas; a river of 10 to 35 km² is a few metres wide, below the cell size.
5. **The longest river can exceed the map side.** In worlds with interior basins the longest river winds far (1024 seed 13: 25.5 km and seed 1: 20.6 km on a 16.4 km map). Plausible; P1-00 13.1 sets no upper bound on `longest_river_m`.

