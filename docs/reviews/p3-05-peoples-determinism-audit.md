# Determinism audit: P3-05 peoples (generator 0.15.0, schema 7)

Auditor: determinism auditor, 2026-11-19. Change audited: `git diff origin/p3/04-int..origin/p3/05-int` at the tip `b62c1cf` ("P3-05 tests: a lambda capture clang accepts"), non-test code. Spec: `docs/specs/p3-05-peoples.md` on that branch (13.1 is the normative form of the P3-04 audit's A1 and N1 to N5; 3.3 the streams; 5 to 10 the rules), with P3-04 r6 6.9. `origin/p3/04-int` already carries the P3-04 audit (`11473ba`) and its hardening (`fa87955`: A3, A4, N1); `engine.cpp` is unchanged by this diff. Line numbers are those of `b62c1cf`.

Scope:
- `worldgen/src/history/peoples.cpp`, `peoples_geo.cpp`, `worldgen/include/worldgen/history/peoples.hpp` (all new).
- `worldgen/src/history/event_catalogue.cpp` (kinds 6 to 19), `worldgen/include/worldgen/core/streams.hpp` (12 streams, 46 names).
- `worldgen/src/stages/history.cpp` (the shipped `EngineConfig`), `history/inputs.{hpp,cpp}` (`biomes` and three layers), `stages/stage.{hpp,cpp}`, `stages/pipeline.hpp` (committed `biome` rows), `db/rows.cpp` (`read_biome_rows`), `db/reader.hpp`, `db/writer.hpp`.
- `worldgen/src/data/kinds.cpp`, `kinds_history.cpp`, `data/schema.hpp` (`history.peoples`, race field `density_permille`).
- `worldgen/src/inspect/history.{hpp,cpp}` (the per-race `[history]` keys).
- Data: `game/data/history/peoples.toml`, `report.toml` (14 templates), `game/data/races/*.toml`.

Read for context, unchanged here: `history/engine.cpp` (`State`, writer, bookkeeping, at `fa87955`), `history/draw.cpp` (`chance`, `pick`), `core/checked.hpp`, the reference `worldgen/tests/reference/p3_peoples_reference.py` (only for the lead's rulings it records).

**Verdict: pass with notes.** No blocking and no should-fix finding. Everything the subsystem writes is integer arithmetic through `checked_*` and `floor_div` with bounds well inside `int64`; every random decision is a keyed draw on its own declared stream with keys from ids and the year; every loop over civilizations or settlements runs over ids up to a count taken before it; no reference, pointer or span into `State` outlives a founding; the two caches in `PeoplesGeo` are pure functions of their key, so their content cannot depend on query order; every sort is total. Same-seed worlds are byte-identical across thread counts, and the independent reference's golden vectors agree (section 4). Three advisories, eleven notes, and a list of what P3-06 must respect (section 2). One lint rule added: `history-held-reference` (section 3).

Classification as in earlier audits: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. A legal data set divides by zero in the founding pressure.**
- `peoples.cpp:713-715`: p(s) = max(P × 1000 div C_s, P × 1000 div `kind_cap`(s)). `kind_cap` (`peoples_geo.cpp:403-421`) is `village_min` − 1 for a race whose highest ladder kind is the hamlet. The schema allows `village_min` = 1 (`kinds_history.cpp`, int(1, 1000000)) and a race may list only `hamlet` (P3-01). Then the cap is 0 and `floor_div` raises a FatalError (`checked.hpp`: fatal if b = 0).
- Deterministic (the same fatal error on every platform), and unreachable with shipped data (the lowest ladder top is the faeries' 1,499). But spec 14 claims its invariants "for every data set within the schemas", and the failure is a generic `floor_div` message, not a data error.
- Fix asked (engineer, low priority): a named check of `history.peoples` or of the race kind (`village_min` ≥ 2, or the cap at least 1), or use max(1, `kind_cap`) in the divisor. `grow_population`'s use of the cap (`min(P + d, max(P, cap))`) is safe at 0.

**A2. `[history]` sums stored populations without a bound (the P3-04 A3 pattern again).**
- `inspect/history.cpp:255, 257`: `st.population += row[11].integer` and the per-race sum. The row shape is checked, but the value is not. A hand-edited world with two settlements at 2^62 each overflows a signed `int64`, which is undefined behaviour. The generator never writes such a value (`State` bounds population to [0, 2^31 − 1]).
- Fix asked (engineer, low priority): report `corrupt table 'settlement': row <id>: population <p> outside 0 to 2147483647` before summing, as `fa87955` did for phase years. P3-07's `[stats]` shares these keys and should do the same.

**A3. The `homes()` cache is public and keyed by (step serial, race) only.**
- `peoples.cpp:265-267`, `339-341`. `homes()` must give `awakening_place` and `awaken` the same answer within the awakening step, and it does: the cache is filled before any state change, and the engine appends only the awakening event between the two calls.
- But `homes()` is a public member (`peoples.hpp:279`). A caller that asks again in the same step after `awaken` (a test, or P3-06 inspecting where a race would awaken) gets the stale pre-awakening list, although regions are now held. Deterministic, but a trap.
- Fix asked (engineer, low priority): make `homes()` private (the awakening handler is a friend), or clear the cache at the end of `awaken`. Rename `homes_year_` to `homes_serial_`: it holds a step serial, not a year (`peoples.hpp:325`).

### Notes

**N1. The engineer's questions on references and reallocation: verified.**
- **`foundings`** (`peoples.cpp:689-806`). `g.site(..., state.settlements(), ...)` (`:738`, `:745`) builds the span at the call. `PeoplesGeo::site` only reads it (`peoples_geo.cpp:637-694`), and the result is a `SiteCell` value. The founding comes later (`:798`), after the span is gone. Inside the civilization loop (count `nc` taken at `:695`), the settlement loops re-read `settlement_count` per civilization (`:706`), hold `const Settlement&` only inside their own bodies, and found nothing there. `state.civ(c)` is re-fetched by id. The only references held across `found_settlement` are `const PeoplesRace& r` and `const PeoplesGeo& g`, which point into `PeoplesGeo`, never into `State`.
- **Secession inside the T4 crisis loop** (`:920-938`, `crisis` `:941-1049`). The loop runs to `nc` taken before it, so the seceded civilization c2 (id > `nc`, created in this step) is not processed. `race` and `culture` are copied before `found_civilization` (`:1017-1018`). After it, only ids are used (`set_capital`, `set_settlement_owner`, `set_region_owner`). The caller's `const Settlement& cap` (`:934`) is last used in building the event, before `crisis` is called. `found_civilization` touches only the civilization vector; the settlement vector is untouched.
- **`const Settlement&` across `strike` / `abandon` / `ruin`.** None of `strike` (`:810`), `abandon` (`:610`), `ruin` (`:525`), `end_civilization` (`:594`) or `crisis`'s no-split path founds anything. They call only `set_population`, `set_settlement_status`, `set_settlement_owner`, `set_region_owner`, `set_capital`, `end_civilization` and `append`, which change elements in place and never resize a `State` vector. So the engineer's claim holds, including refugee placement: `ruin` adds survivors to another settlement through `set_population`, in place.
- The references are also used defensively. `strike` copies `p`, status, kind, owner and region before its first setter (`:813-819`). `abandon` copies owner and region and reads `s.population` before `ruin` (`:612-619`). `ruin` copies everything it needs from `s0` (`:528-537`). The plague block re-fetches `state.settlement(id)` after the famine strike (`:875`) and the owner after the outbreak (`:894`).
- One step adds settlements: an awakening may found several civilizations and settlements in one loop (`awaken`, `:345-389`). There, ids are taken fresh per home and no reference into `State` is held across a founding.

**N2. The `PeoplesGeo` caches cannot depend on query order.**
- `hops(from)` (`peoples_geo.cpp:563-583`) is a BFS over the static neighbour lists (ascending ids), from a fixed root. `candidates(region, race)` (`:612-635`) is a sort of a static cell list by a static value. Each entry is a pure function of its key. Whichever query fills it first, the content is the same.
- References stay valid: the outer vectors `hops_` and `candidates_` are sized once in the constructor (`:523-525`) and never resized; filling one entry assigns only that inner vector. So `outward_target`'s `from_capital` reference (`peoples.cpp:650`) and `site`'s `list` reference (`peoples_geo.cpp:671`) cannot dangle when another entry is filled.
- The caches live in the subsystem, and `history_stage` builds a new subsystem per run, so a resume starts cold, with the same results (P3-04 test item 24 still passes).

**N3. Every sort is a strict total order.**
- `homes`: `stable_sort` on (−σ, draw, region id), unique by id (`peoples.cpp:275-279`).
- Candidates: (value desc, cell index asc), unique by cell (`peoples_geo.cpp:626-628`).
- Neighbours by region id, unique per border pair (`:519-522`); race biomes by biome, unique per race (`:253-254`); biome shares by biome (`:299-300`).
- Every arg-max loop breaks ties by id in scan order: outward target (`peoples.cpp:681`, smaller id), source (tuple ending −id, `:719`), nearest refugee destination and new capital (`:548`, `:575`, smaller id), last ruined (pair (year, id), larger id, `:414-415`), far region (tuple ending −id, `:990`), secession capital (`:1012`).

**N4. Arithmetic and widths.** Every product of the rules goes through `checked_mul` / `checked_add`, and every division through `floor_div` (exact floor, positive divisors). With the schema maxima:
- **Capacity:** area × q × density × `people_per_kcell` ≤ about 4·10^6 cells × 1000 × 1000 × 10^5 ≈ 4·10^17 < 2^63, with an explicit fatal error above 2^31 − 1 (`peoples_geo.cpp:344-350`).
- **Growth:** P × g × (C_s − P) with P < C_s ≤ C ≤ about 10^7 at legal maxima, about 10^17.
- **Shares and tolls:** C × weight ≤ 2^31 × 10^4. Tolls are P × t ≤ 2^31 × 1000.
- **Rates:** crisis rate ≤ 1000 × 1000 div 1 × 10^4 div 1000 = 10^7 ppm.
- **Chance thresholds:** p × 2^32 ≤ 10^7 × 2^32 < 2^56 (`peoples.cpp:78-81`), clamped to [0, ONE]. `chance` compares the draw's high 32 bits against the raw value (`draw.cpp:31-36`), so p ≥ the scale means always and p ≤ 0 never, identically everywhere.
- **Picks:** `pick` ranges ≥ 1 by `peoples_tolls`.
- **Refugees:** may raise a settlement to 2C ≤ 2 × 10^7 (the 2^31 − 1 bound of `set_population` is far).
- **Unchecked sums:** `PeoplesView::population` and the split's `pop_of` are plain sums of at most 10^6 populations ≤ 2^31.
- **Widths and formatting:** no `long`, no `size_t` in stored values, no float, no `<cmath>`, no `printf`; text through `format_i64`. `std::function` is used only to hold the four candidate sets of 7.1, called in a fixed order.

**N5. Quiet-step growth is the per-year loop, exactly (invariant 9).**
- There is no closed form or shortcut: `grow_years(ctx, L, true)` (`peoples.cpp:429-456`) iterates L years over every visible standing settlement.
- The single share snapshot (`:433`) equals recomputing each year. Shares depend on the standing set, owners, capitals, regions and capacities, and inside the loop only populations change, plus declining → thriving, which keeps a settlement standing. Kind changes and abandonment come after the loop. `kind_cap` does not depend on the current kind, except a fortress's, which never changes.
- Recovery runs each year inside Q1, as in T1. That follows the lead's ruling 4 recorded in the reference ("invariant 9 governs 9.2"). Spec 9.2's text still reads "then 8.3's recovery … once at the end" and "shares recomputed each time". The second is equivalent; the first is not, when a declining settlement above its share recovers early in the step and then decays below `recover_permille` × `before`. The spec writer should align 9.2 with the ruling. No determinism impact.

**N6. Streams and keys.**
- `streams.hpp` registers the twelve `hist.peoples.*` names after `hist.chronology.awakening`, in the order of 3.3 (46 names). `peoples.cpp:53-58` maps them by position into `keys_`, derived once per run with `stream_key(seed, name)` (`:167-169`). One stream per decision kind.
- Keys match 13.1 item 5 exactly: `home` (race, region), `civs` (race, 0), `found` and `outward` (civ, 0), `famine`, `famine_toll`, `plague`, `plague_toll` (settlement, 0), the spread toll (target, 1), `plague_spread` (origin, target), `abandon` (settlement, 0), `crisis` and `split` (civ, 0). All are with w0 = `StepContext::year()`. No key is a list position.
- No draw is taken from the sequential generator; the lint rule `history-sequential-rng` is clean over every new file.
- One property of these keys, for the record: a settlement struck more than once in one T4 while already declining (own famine, then own plague, or a spread and then its own outbreak) draws `abandon` (s, 0) with the same key each time, so the later chances repeat the first outcome. Deterministic and as the spec states; effectively one abandonment chance per settlement per year.

**N7. Visibility.** Peoples reads its own entities through `visible()` everywhere except the stated exception of 8.5 (refugee destination, new capital, last-settlement test, which may be created earlier in the same step). The secession's `mine` uses `visible()` (`:949`). The `in_region` slot count (`:733-736`) counts standing settlements created earlier in the same T2 by other civilizations, as spacing does. The spec's 6.3 says "standing settlements in R (any owner)", so this is the reading of the spec, and it is order-dependent only through ascending civilization id, which is fixed.

**N8. Events and templates.** Kinds 6 to 19 are appended with ids 6 to 19 (`event_catalogue.cpp`), no shipped kind edited. Every emitter checks the quiet budget before it changes state (`abandon_events`, `:458-468`, counts the 1 to 3 events an abandonment appends; kind changes `:506`; foundings `:764`; fading charged to civilization 0, `:1062`), so W9 holds by construction. Region and settlement of each event agree (W7), and every event is dated at `ctx.year()` (W2, W3). The 14 new templates are ASCII and use only roles, values and `?` choices of their kinds; `render_event` is unchanged.

**N9. `[history]` keys.** `population`, then `population_<key>`, `settlements_standing_<key>` and `regions_held_<key>` for every `race` row in id order (present or not), from the reader's primary-key order (`inspect/history.cpp:227-268`, `295-304`), integers through `format_i64`. See A2 for corrupt input.

**N10. Resume.** `HistoryInputs` gains `biomes` (from the pipeline's committed `biome` rows, appended by `write_biome` in the writer's enforced ascending order; from the world by `read_biome_rows`, primary-key order) and the layers `habitability`, `river`, `flow_dir` (decoded blobs on both paths). R1, R2 and Y2 cover them and pass (section 4). The race data not echoed in the world comes from the `ContentSet`, whose hash the resume checks.

**N11. Configuration and order.** `stages/history.cpp` installs `PeoplesAwakening` and `PeoplesSubsystem` in a fixed order; `steps()` lists T1, T2, T4, Q1, Q3, Q4 once each, which the hardened engine (`fa87955`, N1) would refuse otherwise. P3-05 opens no relation and has no tension source.

## 2. What P3-06 must respect in this code

1. **Order.** Install conflict after peoples in `EngineConfig::subsystems`. In T2 and Q4 peoples runs first. Its foundings of the same step are live state (the region is held at once, `:802-804`) but invisible to conflict's iteration (`created_step`).
2. **`ruin(ctx, s, e)` preconditions and effects.** The settlement must be standing and owned (fatal otherwise, `:529-531`). Survivors move to the owner's nearest other standing settlement, including one created earlier in the same step. `ruin` may append `capital_moved` with cause `e`. It does **not** end the civilization. `e` must be an event already appended in this step (it becomes the `ruin_event`). In a quiet step `ruin` appends without a budget check of its own, so conflict must check `quiet_events_left` for the up to two events (its own and `capital_moved`) before calling it.
3. **Ending a civilization.** After a sacking leaves a civilization with no standing settlement, conflict must end it itself (`civ_conquered`). Otherwise the next T1 ends it with `civ_ended`, caused by the last `ruin_event`. A civilization that loses its last settlement by **transfer** (`set_settlement_owner` without a ruin) has no ruined settlement on record, and the next T1 stops with `history: peoples: civilization <c> has no standing settlement and none was ruined` (`:422-424`). Conquest by transfer must therefore end the civilization in the same step.
4. **Ruin only in T6, or end at once.** If conflict ruins a civilization's last settlement in T2 and leaves it alive, T4 of the same year may give that civilization a succession crisis. The no-split path then applies `set_population` and `set_settlement_status(declining)` to its ruined capital (`:1041-1048`), turning a ruin back into a standing settlement and clearing its `ruined_year`. T6 is after T4, and T1 ends empty civilizations before the next T4, so ruining in T6 as planned is safe.
5. **Transfers.** Move regions only with settlements (invariant 4). Call `note_reduction(s, e)` after casualties, so a later abandonment names the right cause. Keep populations within [0, 2^31 − 1], by `set_population` only.
6. **Draws.** Use conflict's own declared streams (one per decision kind), keyed by ids and the year, never `hist.peoples.*` streams, and never a position in `PeoplesView::settlements()` / `regions()` lists (they are ascending id lists, but a position shifts when the set changes).
7. **Costs of the view.** `PeoplesView::share` recomputes every share (O(settlements)) per call (`share_now`, `:254-259`), and `wanted_held` runs a full outward scan. Call them once per civilization per step, not per pair, at the 1024 sizes of P3-00 (performance only).
8. **References.** P3-06 founds no settlements (P3-05 20), so the reallocation hazard there is only through peoples. Still, keep ids, not references, in conflict's members across steps. The lint rules `history-live-iteration` and `history-held-reference` apply to its files under `worldgen/src/history/`.
9. **`homes()`.** Do not call it (A3).

## 3. Lint

Before the change: `--self-test` passed (16 snippet files) and the default scan found 0 findings in 157 C++ and 13 CMake files. `history-live-iteration` reports nothing in the P3-05 code, which iterates by id everywhere.

It misses one form, the held reference. A `const Settlement& s = state.settlement(id)` (or a pointer, a `std::span<const Settlement>`, or an `auto&` from `.civ(`) used after a later `found_*` call in the same block is the other half of P3-04's A1. P3-05 does not contain it, but the hand check took most of this audit. Added rule **`history-held-reference`** (library history code, `HISTORY_DIRS`):
- **What it reports:** a declaration of a `Civilization&` / `Settlement&` / pointer, a `std::span<const Civilization|Settlement>`, or an `auto&` / `auto*` / `auto` whose initializer is exactly a call of `.civ(`, `.settlement(`, `.civs()` or `.settlements()`, when the name is used after the closing parenthesis of a later `found_civilization(` or `found_settlement(` call in the same block. A use inside the founding's own arguments does not count, since arguments are evaluated before the vector grows.
- **What it does not see** (stated in the README): a founding inside a called helper (`crisis` is one: the hand check above covers it), a use earlier in a loop body that runs after a founding later in the previous iteration, and references kept in members.
- **Self-test:** new snippet `selftest/bad_history_held.cpp`, with five reported forms and near misses: fields copied first, the reference only in the founding's arguments, re-fetched after, a block ending before the founding, another type, a range-for variable, an `int64` named `automatic`, and the stub's `const auto c = static_cast<std::int64_t>(state.civs().size()) + 1`, which a first draft of the pattern flagged.
- README updated.

After the change: self-test passed (17 files, 0 failures), and the default scan found 0 findings (the rule fires on none of `engine.cpp`, `stubs.cpp`, `peoples.cpp`, `peoples_geo.cpp`).

## 4. What was run

All in a worktree of `origin/p3/05-int` at `b62c1cf` (branch `p3/05-audit`), build tree outside the source tree, `linux-gcc-release` only, `-j 3`.

| Run | Result |
| --- | --- |
| Lint `--self-test` and default scan, before the change | passed (16 files); 0 findings |
| The same after adding `history-held-reference` | passed (17 files, 0 failures); 0 findings |
| `generate --seed 42` (1024, default settings), `--threads 1` and `--threads 5` | byte-identical (SHA-256 `a552d3afffa1129c…`) |
| `generate --seed 7 --map-size 512 --event-intensity turbulent --civs-per-race 4`, `--threads 1` and `--threads 4` (a stress world: 487 events, 2 secessions, 5 abandonments, a faded ruin, 138 famines, 69 plagues, 48 spreads) | byte-identical (`129dc012bc1d7d80…`) |
| `hash --detail` of the seed 42 world against `hist-peoples-42-1024` in `worldgen/tests/golden/p3-05-peoples.txt` (the independent Python reference) | all 13 history table lines equal |
| `worldgen-tests -sf='*history_peoples_*,*peoples_engineer_test*'` (unit, property, data, golden 512 including resume and threads 1/5) | 29 of 29 cases pass, 31,003 assertions |
| `worldgen-tests -sf='*history_peoples_golden*' -ts=history-slow --no-skip` (the 1024 golden worlds and the hard bounds of 15.5, the seed 42 sample) | 4 of 4 pass, 35,474 assertions |
| `worldgen-tests -sf='*history_engine_*,*engine_hardening*'` (P3-04's engine tests at 0.15.0, R1, R2, Y2, Y3, Y4) | 48 of 48 pass, 121,742 assertions |

Not run: `linux-clang-asan` and any second compiler (instructions). The cross-compiler `world-hashes.sh` comparison is the validation specialist's; nothing found here predicts a difference.
