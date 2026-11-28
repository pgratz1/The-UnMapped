# P5-08 Pantheon, divine acts and cults: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-27.
Scope: `git diff origin/main...HEAD -- worldgen/src worldgen/include tools/worldgen-cli` at 5ca9daa (branch `p5/08-int`, audited on `p5/08-audit`): 67 files, 4,688 lines added. Core: `worldgen/src/history/pantheon.cpp` and `pantheon_cults.cpp` (new), and the changes in `monsters.cpp`, `monsters_deep.cpp`, `figures.cpp`, `alliances.cpp`, `conflict.cpp`, `peoples.cpp`, `peoples_geo.cpp`, `engine.cpp`, `stages/chronology.cpp`, `stages/history.cpp` and the database layer. The readers, digest, report, timeline, browser and markers were read for order and platform questions only. Tests were not run (integration ran them). Spec `docs/specs/p5-08-pantheon-cults.md` r1a (sections 3, 6.10, 6.12, 7, 9, 14), hand-over `docs/reviews/p5-08-impl-notes.md`, integration notes Part 3b, P3-04 6.9. Generator 0.30.0, schema 14.

**Verdict: pass with advisories.** No blocking finding. Three advisories (A1 to A3). P5-07's A1 and A2 are closed. No lint rule was added: nothing new in the diff is both banned and recognisable by text.

## Cross-compiler and thread comparison

`linux-gcc-release` and `linux-clang-release` were built from 5ca9daa (`-j 5`). The worlds are at `--map-size 1024` with the checkout's `game/data`. For each world four things were compared byte for byte: the whole file (`sha256sum`), `hash --detail`, `report --history --level summary` and `--level full`.

| Case | Whole-file SHA-256 (both compilers) | Paths in the world (full report) |
| --- | --- | --- |
| seed 0 | `6f824eac9fd7ed27352e7033eed4c4776488348d5eefc0d537bf6377b0cbfba3` | 14 acts (bless, curse, mark), 1 gods' flare-up, 4 cults, 17 spreads, 7 faiths, 1 deep stir whose flare-up (5403, trigger e339 `delving_deepened`) wakes the stirring mine in its first year (e450) and loses the hold at the gate (e451, the gate's flight plan), a later take after a raid (e457), 1 aid after its outcome, 4 towers abandoned |
| seed 3, `--divine-activity meddling` | `79461160d7acb81e2a123dc0be2ea168e235ecb6fdd4500abb7e94307f85e10b` | 24 acts (all five kinds: 1 champion, 2 sent monsters), 3 flare-ups, 6 cults, 1 suppression into hiding (a sanctum) |
| seed 1, `--divine-activity distant` | `11d5d18ef7c5d0b7015d2037e1ce3a6bcd7f84dd1ecc80a18b0f4c38bf267073` | 11 acts (2 sent monsters), 3 flare-ups, 1 hidden cult, 1 tower taken up |
| seed 7, `--monster-pressure high` | `e7413a430d684fe8597f630280ac6cfc4fe774c2a58c081c5d491cf502063fb6` | 15 acts, 4 flare-ups, 1 hidden cult, 1 tower taken up, 9 abandoned, 1 aid |
| seed 42 | `aa2b56cc591c2afb6d6c4723a8f09e70ded17f434ca466e65e8a730e4e966213` | 12 acts (1 champion, 1 sent monster), 2 flare-ups, 4 cults, 1 tower taken up |
| seed 42, `--threads 1` | as seed 42 | as seed 42 |

**All comparisons are identical** on both compilers (file, detail and both reports). Seed 42 with `--threads 1` matches the default thread count on both. MSVC was not built here; CI's `compare-hashes` covers it.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 22 snippet files, 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 194 C++ and 13 CMake files, 0 findings.
- The diff adds no `determinism-lint: allow` suppression and changes no CMake file.

## What was checked

1. **Streams** (`core/streams.hpp`). Thirteen names appended after `hist.deep.warren_kind`, all unique; `all_stream_names` grows from 107 to 120. The pantheon fills its eleven keys once in `prepare` (`pantheon.cpp:193`), monsters its two stir keys (`monsters.cpp:415`).
2. **Keys, one draw per decision.** Every draw goes through `ctx.draw`, which adds the step's year (`engine.cpp:1356`). The pantheon runs in T4, T6 (no draw) and Q4, its source and applier in T5, so each (stream, year) pair is reached in one step only.
   - `act`, `kind`, `monster_kind` (g, c): at most one intervention a year, from one candidate.
   - `flare` (g, c): Q4 only, at most once.
   - `cult_found` (act event): an act waits at most to a later step, so a later draw has another year.
   - `cult_name` (cult id) and `site_name` (site id): shrine, temple and sanctum names take the next site id, which is fresh at each call.
   - `cult_spread`, `adopt`, `suppress` (k, c): one draw each per pair and step. `hide` (k): only at the last realm's suppression, after which the cult is hidden or ended.
   - `deep.stir` (site) and `deep.warren_stir` (settlement): Q4, at most one success, ascending ids.
   - No key holds a list position or an address. Figures, sent monsters and lairs use their existing streams and id keys (`raise_divine`, `create_sent`).
3. **Integer arithmetic.** No floating point and no `<cmath>`. Every new product uses `checked_mul` or `checked_add`, and every division is `floor_div` by a positive constant.
   - Regard is clamped to `±regard_max`. The fade is cumulative, `F(Y + L) - F(Y)`, so it does not depend on how steps cut a span (`pantheon.cpp:455-468`).
   - `growth_permille` is clamped to [-900, 1000] and `force_permille` to [-500, 500]. The divine force factor is `f × (1000 + d) div 1000` with `1000 + d` ≥ 500 (`conflict.cpp:803-809`).
   - The divine growth is `min(cap, P + d × (1000 + g) div 1000)` with `g` ≥ -900, then `max(·, P)`.
   - The stir chance is `ppm × M div 1000 × L × deep_stir_permille div 1000`, all checked.
   - `telling` values (`(c + acts) % 3`, `(id + 4d) % 9`, `k % 3`) take only non-negative operands.
4. **Loop order and containers.**
   - The new state is vectors indexed by id plus `std::map`/`std::set` keyed by integer ids (`pairs_`, `cult_in_`, `latest_`, `faith_of_`, `shrine_settlements_`), and `deeds_by_kind_`, a `std::map<std::string, …, std::less<>>`. A string key orders by `char_traits<char>::lt`, which the standard defines as an unsigned comparison, so the order is the same on every compiler.
   - There is no unordered container and no pointer key.
   - `best_candidate` walks `pairs_` in (g, c) order, so a tie keeps the smaller pair.
   - `endings` copies `ended_realms_` and the cult keys before closing anything.
   - `faiths` copies the `cult_in_` keys and re-tests `cult_in(k, c)` for each.
   - `spreads` runs to `settled_cults_`, taken before `foundings` (E4).
   - `AlliancesSubsystem::partners` builds a `std::map` by partner id, so the result ascends.
   - `regions_by_hops` emits by (hops, region id). The settlement scans run by id to a count taken first, and ties keep the smaller id (strict `>`).
5. **Sorts.** No `std::sort` remains in `worldgen/src/history`. Each new `std::stable_sort` is on integers, pairs of integers, or a comparator that ends in an id:
   - `ranked_causes`: |points| descending, then event descending.
   - The report's god, cult, faith and `in` lists: (since, id), (acts, id), (status, id).
   - The monster rank: rank, hoard, id.
   - Every one is a total order, or orders equal elements that cannot be told apart.
6. **P3-04 6.9 and held references.**
   - The pantheon founds no settlement. Its new figures (`raise_divine`), sites (`open_site`) and monsters (`create_sent`) are appended outside any loop over `State`'s live lists.
   - `const Settlement& seat` and `t` in `pantheon_cults.cpp` are held across `ctx.append`, `place_name`, `raise_divine` and `open_site`. None of these touches `State::settlements_`.
   - `champion_to_war` iterates `wars_` and, inside, calls `appoint_champion`, which changes only `wars_.at(w)` (no insert) and then returns, so the loop ends.
   - In `faiths`, the `faith_of_` iterator stays valid across `faith_of_[c] = …`: a `std::map` insert does not invalidate iterators.
7. **Engine seam** (`engine.cpp:589-603`). The applier is called in source order for each valid intervention. An intervention on a civilization created in this step is dropped (`created_step >= serial_`). The source checks the same eligibility as `apply`, on the same state and in the same step.
8. **The finish** (`pantheon.cpp:386-401`, E3). T6 of year E - 1 only, once (`finished_`). It scans the log from `cursor_` to the last event in id order. It makes no draw, does not move `cursor_` and reads no deeds. Then `endings` and `temples` run exactly as in a step: by cult id within a realm, and by temples in site order. A history that ends in a quiet step has no T6 at E - 1. There the Q4 read already sees every ending of the step, since the pantheon is last in the subsystem order.
9. **Growth** (`peoples.cpp:486-496`, `peoples_geo.cpp:471-488`).
   - `growth_term` is the former body of `grow_population`, moved without change, and `grow_population` calls it. The bytes of a world without divine effects are therefore unchanged.
   - The factor is applied only when `next > p`. It acts on the uncapped term, and the cap comes after.
10. **Champion rules** (`figures.cpp:1149-1205`, `:1278-1290`; `monsters.cpp:1517-1567`).
    - `hunt_levy` and `hunt_prowess` are the lines removed from `hunts()`, with the same loop bounds, order and arithmetic. `hunts()` computes `floor(levy × prowess / 1000)` exactly as before.
    - `huntable_by` is pure and walks monsters by id.
    - The scan of `wars_` is ascending war id with sides 0 then 1. The first free side gets the champion, and every busy side is flagged only when no side is free.
    - The `champion_waits` flag is a member of `KnownWar`, value-initialised (`figures.hpp:201`). It is cleared by every `appoint_champion`. It is read in `vacancies()` in war-id order before the vacancy test.
    - `appoint()`'s champion branch makes no draw and comes before `hist.figures.appoint`.
    - `hero_of` prefers the living champion. The hunt's draws are keyed by monster and civilization, not by figure, so the choice of hunter changes no key.
11. **Monsters.**
    - Phase tracking (`monsters.cpp:567-570`, `pantheon.cpp:340-343`): `phase_seen_` and `phase_start_` are set at T4 on the first tick year of each phase id. Both start at 0. Both subsystems are rebuilt by the replay on resume.
    - The certain waking happens only when `phase_kind` is flare, the trigger is `stir_event_` and the year is `phase_start_`. It wakes before the ordinary candidates, and `deep_waking` returns after one waking.
    - `deep_stir` runs only when no flare is pending, mines before warrens, by ascending id.
    - Sent monsters are outside the world cap (`living()`). `prepare_sent` is pure. `create_sent` checks that the ids are still the next ones before it pushes the monster and the lair.
12. **Event and row order.**
    - Waking (`monsters_deep.cpp:447-503`): `deep_woken`, then the outcome (`hold_lost` with its flight and ruin events, or the seal), then one `ally_aided_hold` per aid in ascending ally id. Each aid cites (waking, outcome) and is followed by its own `credit`.
    - Towers: in `die()` the tower is found by `lower_bound` on `towers_` (ascending site). Only the tower whose `master` is the dead caster is updated. `take_up_tower` keeps the vector's order.
    - The lapse is tested after the ending codes 5 and 4, and waits on a quiet step's budget.
    - Every list of causes is ascending: `stable_sort` or append order.
    - `faction` rows are written by ascending id after `book`. The writer checks `require_next_id`, and the DDL is the 37th statement.
    - `god` gains `domain`, `title` and `race`. The writer binds `?1`–`?6` and checks that `domain` is 1 to 9. Gods are written creators first, then the pantheon in the kind's definition order (the content loader's order, the same as for `beast` and `monster`).
13. **Platform.** No `char` sign dependence: the new grammar checks compare with ASCII literals, and string ordering is unsigned by the standard. No shift. No `size_t` is stored: the counts are cast to `int64` before they are written. No locale, time, environment or thread use in the diff.
14. **I0** (spec 14). With `act_permille` 0 and `flare_permille` 0 the act draw always fails and no flare is set. `pending_acts_` stays empty, so no cult is founded, and every `DivineView` read returns 0 or false. `strengthened`, `grow_years` and `shunned` then return exactly their former values, so I0 holds by reading.

## Integration judgment: a mine whose region is won and lost in its opening year

The case is fixture alliances-small, seed 5, mine si21, year 4593 (integration notes Part 3b, check 13).

- Conflict runs before alliances in the subsystem order, and both run in T2 and T6.
- In T2 the founder holds the region, and `mine_openings` opens the mine. The mine cites `max(region.owner_event, town.founding_event)`, which exists in the log.
- In T6 conflict gives the region away. `mine_closings` then ends the mine with ending 4 and the region's new `owner_event`, which has the same year.
- `region_control` rows are taken by the engine's bookkeeping at the end of each step (`engine.cpp:890-907`). An owner that both comes and goes inside one tick step leaves no row. This is P3-04's rule, and relations follow the same rule with a `since = until` row.

The engine's behaviour is defined. It depends only on the fixed step order, the fixed subsystem order and ascending ids. It is the same on every platform and is not a defect. The relaxed check, and the reference that mirrors it, are a correct reading. It should be written into spec P5-04 (see A3).

## P5-07 advisories

- **A1 (the forced seal relied on two equal flight plans): closed.**
  - At the gate the plan is kept in `std::optional<DeepFlightPlan> plan` (`monsters_deep.cpp:387-396`).
  - It is passed to `deep_take(..., plan ? &*plan : nullptr)`, and `deep_take` uses it instead of recomputing (`:651-655`).
  - When the settlement is not a last refuge, no plan is needed and `deep_take`'s own `deep_last_refuge` test still returns false. `deep_last_refuge` reads settlements and owners, and these do not change between the gate and the take.
  - A later take after a raid passes `nullptr` and computes its own plan, as before.
- **A2 (later takes did not check the waking realm's visibility): closed.** `deep_after_raid` now tests `!ctx.visible(state.civ(c))` (`monsters_deep.cpp:707-711`).

## Advisories

**A1. Divine growth in a quiet step uses the step's first year for every year of the step** (`peoples.cpp:490`). `grow_years` loops over the L years of a quiet step but asks `divine_->growth_permille(..., ctx.year())`, which is the step's start year. Spec 7.1 says an effect is "in force in every year y with Y ≤ y < Y + years", and section 3 defines Y as "the step's year (a quiet step's start)". Two readings follow:

- an effect that ends inside a quiet step keeps acting for the rest of that step;
- a shrine growth that starts inside it waits for the next step.

This is deterministic and the same on every platform. The test-writer's reference must use the same reading, and the lead should confirm which one is meant. If per-year is intended, pass `ctx.year() + year` (no world change unless an effect boundary falls inside a quiet step, which it can for blessings and curses).

**A2. `GodState::acts` points into the subsystem's own members** (`pantheon.cpp:223`, `:228`): `&tuning_.creator` and `&defs_[i].acts`. It is safe today: the subsystem is built once on the stack in `history_stage`, is never copied or moved, and `defs_` is not resized after `prepare`. A copy or move of `PantheonSubsystem` would leave dangling pointers, which is undefined behaviour. Proposed fix: store the index (creator or `defs_` index) instead of a pointer, or delete the copy and move constructors.

**A3. Spec text for the mine won and lost in one step.** The engine is right (see above). Spec P5-04's mine invariant should say that a region won and lost within one step leaves no `region_control` row of the founder, and that the mine then ends with the event that gave the region away. Otherwise the next reader of check 13 will treat the exception as a defect.

## Not exercised across compilers

One path was reviewed by reading only (item 8 above): the finish's closing of a realm that ended in T5 or T6 of the last year. The doctest fixtures cover it on every preset. No `world-hashes.sh` case uses `--divine-activity meddling`; adding one (for example seed 3 at 1024, all five act kinds) would let `compare-hashes` check the pantheon's most active paths on MSVC as well.
