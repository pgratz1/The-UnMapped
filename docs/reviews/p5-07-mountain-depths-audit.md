# P5-07 Mountain depths: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-26.
Scope: `git diff origin/main -- worldgen/src worldgen/include tools/worldgen-cli tools/lint game/data cmake` at f18efdd (branch `p5/07-int`, audited on `p5/07-audit`): 49 files, 1,763 lines added. Core `worldgen/src/history/monsters_deep.cpp` (new) and its callers in `monsters.cpp`, `alliances.cpp`, `conflict.cpp`, `conflict_war.cpp`, `figures.cpp`, `relations.cpp`, `engine.cpp`, `stages/history.cpp`; the catalogue and reader checks (`event_catalogue.cpp`), digest, report, timeline, browser, markers; the data loader (`kinds_history.cpp`) and the shipped data. Tests were read only as context. Spec `docs/specs/p5-07-mountain-depths.md` r2 (sections 3 to 7, 13, 14), hand-over `docs/reviews/p5-07-impl-notes.md`, P3-04 6.9. Generator 0.29.0, schema 13, history report v10.

**Verdict: pass with advisories.** No blocking finding. Three advisories (A1 to A3). No lint rule was added: nothing new in the diff is both banned and recognisable by text.

## Cross-compiler and thread comparison

`linux-gcc-release` and `linux-clang-release` built from f18efdd (`-j 5`). Worlds at `--map-size 1024` with the checkout's `game/data`. For each world the whole file (`sha256sum`), `hash --detail`, `report --history --level summary` and `--level full` were compared byte for byte.

| Case | Whole-file SHA-256 (both compilers) | Deep paths in the world (full stats) |
| --- | --- | --- |
| seed 0 | `5f5f185d6edc453407ea14305913a45249353a22b7a3a70b327b196b95e09f49` | 8 deepened, 2 woken, 1 sealed, 2 holds lost (a fall at the gate and a later take), 1 aid |
| seed 7 | `4e8004e3d8dd8bb660d2ffcd70d891c47f41f8989bac67b3a7367b68bbae128a` | 13 deepened, a depth-3 mine, 1 woken and sealed, 2 warrens delved |
| seed 42 | `a70e8a662ff2c1798ece6f0114580496e93af3d353a09fbb608fabbe0fd45227` | 3 warrens delved |
| seed 42, `--threads 1` | as seed 42 | as seed 42 |
| seed 3, `--civs-per-race 4` | `f23ce1426c4469a0018accaffcac0e541c36f50fb4b7cd09989a953e07570e27` | 9 deepened, 2 woken and sealed, 7 warrens delved, 1 aid |
| seed 5, `--monster-pressure high` | `98a36d8349bace63b545f6c2c5571700befad0028980c77eead38e9ca3411515` | 1 deepened, 2 warrens delved |
| seed 11, high, `civs4` | `51acfc79a1d639bc2469a57268df4f8afb3f79d7ff252886d39039400ee8d4bf` | 4 deepened |
| seed 20 | `743e5dd4c21a9010baacf6c338e29c8cedca8237064c0bb9ba332760fa593530` | 3 deepened, 2 warrens delved |
| seed 66 | `2933374d17e273f17608ddb74fc55fc774e08c77722594662c980e0f451d1456` | 6 deepened, 3 warrens delved |

**All comparisons are identical** (file, detail and both reports) on both compilers, and seed 42 with `--threads 1` matches the default thread count on both. Reading only, not run across compilers (A3): a warren's waking, seal and fall, the deep flight of a last refuge, and a realm lost to the deep. MSVC was not built here. CI's `compare-hashes` covers it.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 22 snippet files, 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 190 C++ and 13 CMake files, 0 findings.
- The diff adds no `determinism-lint: allow` suppression and changes no CMake file.

## What was checked

1. **Streams** (`core/streams.hpp`): nine names appended after `hist.alliances.renew`, all unique, `all_stream_names` grows from 98 to 107 in the spec's order. `deep_keys_` is filled once in `prepare` from the seed (`monsters.cpp:665-671`).
2. **Keys, one draw per decision.** Every draw goes through `ctx.draw`, and the step's year is part of the key.
   - `delve` (site, d+1): at most once per mine and step (`monsters_deep.cpp:144`). In Q4 the budget is tested before the draw.
   - `wake` and `kind` (site): T4 only, `deep_waking` runs once a year, and `woken` bars a second kind draw.
   - `gate` (m) and `aid` (m, a): m is the new monster id, so each key is fresh. Both are drawn before any event (`:321`, `:384`).
   - `take` (m, t): one raid per monster and tick year, so the key is drawn at most once (`:621`).
   - The warren streams are keyed by settlement id and are separate from the mine streams.
   - No key holds a list position or an address. The monster name (`hist.names.monster`, key m) and the lair name (key l = next site id) use their existing streams.
3. **Integer arithmetic.** No floating point and no `<cmath>`. Every new product uses `checked_mul`, and every division is `floor_div` by a positive constant or a positive sum.
   - Gate: S of at most P × 4000/1000 × prowess/1000 plus the aid; `S × 1000 div (S + might)` with S > 0.
   - Wake chance: ppm × M ≤ 10^10, then times `deep_warren_wake_permille`, then min 10^6.
   - Goblin delve rate: `deep_delve_permille × delving_permille div 1000` ≤ 1000. In Q4, `min(1000, p × years)`.
   - Forge: `1000 + forge_strength + forge_depth × D` with D ≤ 3 (`conflict.cpp:790-796`).
   - Later take: `base × f_p div 1000` with both factors ≤ 1000. `chance_scaled` multiplies p ≤ 10^6 by 2^32, which is below 2^53.
   - `telling` values are sums of positive ids taken `% 3`. No negative operand reaches a division or a modulo.
4. **Loop order.**
   - `deepen` and `deep_waking` run over site ids 1..n with n taken before the loop. The lair the waking pushes is past n, and the loop returns after the first waking.
   - `warren_delving` runs over settlement ids up to a count taken first, founds nothing, and updates `live` in the loop, so the smallest id wins.
   - The warren waking runs after every mine candidate.
   - The aid copies `allies(c)` (`allies_of_` is a `std::map` of vectors sorted at each rebuild) and picks q by population with the smaller id winning ties (strict `>` in id order).
   - K, the kinds with room, is built in kind index order.
   - `hold_lost`'s mine is the first open kind-3 site at t in `sites_` order, which is ascending id.
5. **P3-04 6.9 and held references.**
   - The only founding is `deep_flight` → `settle_refuge` → `found_settlement`. It is reached from `deep_wake_at` (outside any loop over live lists) and from `raid` → `deep_after_raid` inside `monsters_year`'s monster loop, which iterates monster ids to a fixed count; 7.4 item 1 allows this.
   - In `raid`, the `Settlement& t` is last used before the call (`monsters.cpp:1255`) and is fetched again afterwards (`:1302`). The `Monster& m` of the raid memory ends before it. `monsters_year` re-reads `monster(id)` after `raid`, and `monsters_` does not grow in a take.
   - `deep_wake_at` builds the `Monster` and `Site` as values and pushes them after the event. It takes `Monster& w` and `site(...)` only after `deep_take` returns, and keeps only ids across `deep_take`.
   - Refuges created in the step are invisible to later raid targets (`ctx.visible`).
6. **Event and row order.**
   - Waking: the `deep_woken`, then the lair and monster rows (pushed in id order), then one `ally_aided_hold` per aid in ascending ally id (each followed by its `credit` with that event), then the outcome: `deep_sealed`/`warren_sealed`, or `hold_lost` and then the flight's `refugees_settled` and the ruin.
   - Causes are listed in append order, which is ascending.
   - `settlement.ending` 2 for a `hold_lost` ruin is derived in both places (`engine.cpp:592`, `event_catalogue.cpp`).
7. **The forced seal** (6.5, 6.12 item 1). `deep_flight_plan` is computed before the lair, the aid events and the credits, and again in `deep_take`. It reads region owners, settlements (`PeoplesGeo::site` takes only `state.settlements()`), the lazily filled hop cache and tuning. None of these change between the two calls, so the plans agree. See A1.
8. **Resume.** History is re-run in full from stored inputs (P3-04 16.3), so depth, `woken`, `sealed`, the warren vectors and `from_warren` are rebuilt by the run itself, not read back. The new region inputs (`walk_cells`, `ore_reach_cells`) are columns of `region` that the reader already reads (`db/rows.cpp:140,152`). `ctx.event_year` reads the engine's own log.
9. **Containers.** The new state is vectors indexed by id, plus `std::map`/`std::span` reads from alliances. There is no unordered container and no pointer key. `report`'s `sealing` keeps `const Event*` only as a value holder, never for ordering.
10. **Readers, digest and report.** The deep checks in `event_catalogue.cpp` walk rows in id order, keep per-id vectors and look up values by binary search in primary-key order.
    - `deep_stats` counts, and deduplicates its id lists after `stable_sort`.
    - The milestones' one-of-each-kind pass keeps the (salience, id) order.
    - The timeline's depth lists are filled in event order and read as a max.
    - The new `tagged` segment and the strings are ASCII through `num` and `escape_history_text`. There is no locale.
11. **Platform.** No `char` sign dependence. The only shift is `1U << d` with d in 1..3, on unsigned. No `size_t` is stored: every count is cast to `int64` before `put`.
12. **Data and flags.** All new tuning fields are integers with ranges. The code defaults are inert (`deep_delve_permille` 0, wake 0, aid 0, warren wake 0). `delving_permille` is optional with default 0, and `race_delves_one_way` is checked. There is no CMake change.

## Advisories

**A1. The forced seal relies on two equal flight plans** (`monsters_deep.cpp:386-390` and `:556-562`). The plan at the gate is computed before the lair site is pushed, before `lair_count_`/`shunned_count_` change and before the aid events and `credit`. `deep_take` recomputes it afterwards. The two agree today because `flight_destination` reads none of that state. If a later stage lets foundings avoid lairs or shunned regions (for example by making `PeoplesGeo::site` or the destination filter read `WildView::shunned`), the second plan could be empty and the waking would stop with the FatalError "found no flight". That would be deterministic, but it would be a crash. Proposed fix: compute the plan once and pass it to `deep_take` at the waking. This is a refactor, not a world change.

**A2. Later takes do not check that the waking realm is visible** (`monsters_deep.cpp:608`). `deep_after_raid` tests `ended_year` but not `ctx.visible(state.civ(c))`, unlike `deepen` and `deep_waking`. It is safe today: the target was chosen visible, and a realm created in the step cannot be a monster's waking realm from an earlier step. It is noted for consistency with P5-02 6.7 item 4.

**A3. Paths not exercised across compilers** in the nine worlds: a warren's waking, seal or fall (`warrens_woken` 0 everywhere), the deep flight of a last refuge (`deep_flights` 0) and a realm lost to the deep. They were reviewed by reading only (items 2, 5 and 7). The `depths-small` fixture covers them in the doctest suites on every preset. A `world-hashes.sh` case that hits a warren waking or a deep flight would let `compare-hashes` cover them on MSVC as well.
