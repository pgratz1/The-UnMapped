# P5-04a Relations and alliances follow-ups: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-26.
Scope: `git diff origin/main -- worldgen/src worldgen/include tools/worldgen-cli game/data` at 1b3946f (branch `p5/04a-int`, audited on `p5/04a-audit`): 30 files, 1,038 lines added. Tests and test data were read as context only. Spec: `docs/specs/p5-04a-relations-alliances-followups.md` r2, sections 2 and 18, with the hand-over `docs/reviews/p5-04a-impl-notes.md` and P3-04 6.9. Generator 0.28.0, schema 12 unchanged, history report v9.

**Verdict: pass with advisories.** No blocking finding. Three advisories (A1 to A3). No lint rule was added: no new pattern in the diff is both banned and recognisable by text.

## Cross-compiler and thread comparison

`linux-gcc-release` and `linux-clang-release` built from 1b3946f (`-j 3`). Worlds at `--map-size 1024` with the checkout's `game/data`; for each, the whole file (`sha256sum`), `hash --detail`, `report --history --level summary` and `--level full` compared byte for byte.

| Case | Whole-file SHA-256 (both compilers) | gcc = clang |
| --- | --- | --- |
| seed 0 | `4e1047bdef84d84a001682e11863aaff875ecd1797e52f30dc91b43f1066452f` | db, detail, summary, full |
| seed 7 | `40f8fa0c531c5d5cd71e7ba72dfabc5cffae91d41a90fa233468d2ed75bc87c9` | db, detail, summary, full |
| seed 42 | `d9ac166d8d5c9f3f3b6245b025efe6fbc877096e74b7bb33ea7806d477d023ce` | db, detail, summary, full |
| seed 3, `--civs-per-race 4` | `c35e3434c91861553405fc57a7f8af7fe28d366bb7a14aa248fbe13b1b2dab16` | db, detail, summary, full |
| seed 42, `--threads 1` | as seed 42 | db, detail, summary, full |

**All comparisons identical**, and seed 42 with `--threads 1` equals the default thread count on both compilers (file, detail and both reports).

Coverage of the new paths in these worlds (full report stats): refusal motives 0, 1, 2, 3, 4, 5 and 6 (none of 7); betrayal motives 0, 2 and 3; `battles_with_arms` 2 to 5 per world; notable renewals 1 to 88; soured alliances 3 (`civs4` only); spurnings at once 0; tower endings: ending 5 twice (`civs4`), ending 4 never; `hero_rose` with `quiet` > 0 once (`civs4`). Not exercised cross-compiler, reviewed by reading only: the at-once spurning and `pending_spurns`, refusal motive 7, tower ending 4. MSVC was not built here; CI's `compare-hashes` covers it.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 22 snippet files, 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 189 C++ and 13 CMake files, 0 findings.
- The diff adds no `determinism-lint: allow` suppression.

## What was checked

1. **Stream** (`core/streams.hpp`): `hist.alliances.renew` appended after `hist.monsters.hero_fall`, `all_stream_names` 97 to 98, name unique; `renew_key_` made once in `prepare` from the seed (`alliances.cpp:164`).
2. **The one new draw** (`alliances.cpp:415`): key (x, y, d), x < y the ordered pair and d = `next_due`. It is made at most once per (alliance, d): a wait (`quiet_ok` false) returns before the draw and leaves d unchanged; a passed draw clears `renewal_due`, so a later wait on the lapse does not draw again; d moves on only after a renewal. Two alliances of one pair cannot share a d: the first draw at d is made in a year >= d, any closing is in that year or later, ruling A9 bars a new alliance of the pair in the closing year, so a new one's first due is above d. No existing stream gains or loses a draw; the join draw keeps its key (war, z, 0) and only its chance changes (the eight factors).
3. **Choices without a draw**: refusal motive (`:228-232`) is the smallest factor of codes 1 to 7 with the smaller code at a tie, over a fixed array; betrayal motive (`:131-139`) a fixed if-chain; renewal basis and notability (`:433-449`) tests of stored ids; `arms_event` (`:249-264`) walks `trades_` (`std::map`, ascending pair) and keeps the smallest seller id; `last_war_end` (`conflict.cpp:602`) is a max over `last_peace_` (`std::map`), order-free; the `civ_foes_` larger opening event (`relations.cpp:1067`) is a max, order-free; the expulsion and cool-down guards are tests of stored years.
4. **Containers**: `arms_sold_` is now `std::map<Pair, int64>`, `arms_cited_` `std::set<int64>`, `towers_` a vector in site id order (pushed after `open_site`, whose ids only grow, so the `lower_bound` of `tower_open` is valid), `last_raid_year_` `std::map`, `pending_crises_` `std::set`. No unordered container, no pointer key. The report's `std::find(chosen, &f)` compares identity only, never orders by address.
5. **P3-04 6.9 and held references.** `calls` copies the ally list before the loop (`:714`); the `Alliance& a` of a refusal is used only before `ctx.append` and the closing is done through a fresh `at()`; `on_withheld`'s reference is not used after `spurn` erases it; `spoils` uses its `find` iterator only for a bool computed before `ctx.append`; `renewals` copies the alliance before `spurn` (`:417`) and takes `live` again after the append. `tower_endings` copies each `TowerSite`, loops by index over a size taken once, and `end_site_seam` does not resize `sites_`; `caster_rises` builds `avoid` as a copy before `open_site` and pushes to `towers_` after it. No founding in any new loop. `history-member-held-reference` and `history-live-iteration` are clean.
6. **Integer arithmetic.** No floating point or `<cmath>` in the diff. The defence chance is `defend_permille` times nine factors in 0..1000 (warmth is clamped 0..1000, `f[3]` and `f[4]` are floor + (1000 - floor) x [0, 1000] / 1000, `f[2]` clamped at 0), each step `checked_mul` then `floor_div`, so every intermediate is below 10^6. Betrayal motive 2: `force x 1000` against `betray_weak_permille x force`, `checked_mul`, force < 2^40. Spoils: `share` and `part` compared with loader-bounded tuning (< 2^31). Tower radius `tower_spacing_permille x map_size` <= 10^3 x 2^13. Chebyshev distance on int64 cell coordinates from a `uint32` index divided by n, no negative division. The report's renewal count `(end - 1 - since) / term` is guarded to non-negative operands and term >= 1.
7. **Reads of rows of the same step.** `tower_endings` reads stored `ruin_event`, `owner_event` and `ending_event` and the event's year; the ending year is the event's year, not the step's. The battle's arms cause reads `AllianceView` state that alliances changes only in its own steps; subsystem order is fixed (`stages/history.cpp`). P5-06 A1 is closed: `pending_crises_` lets a secession read in a later range find its crisis by id (`figures.cpp:437-451`). P5-06 A3 is closed: `split_bonus_permille(civ, year)` takes peoples' `ctx.year()`.
8. **Readers and report** (`digest.cpp`, `history_report.cpp`, `event_catalogue.cpp`). Every new sort has a total order: milestones by (salience descending, id) then id; summary figures by (renown descending, id) after reserved picks chosen over figure rows in id order with a stated tie (longer reign or more books, then the smaller id); living monsters by (hoard, id), ended ones by (harm, id), the room computed without unsigned underflow. Same-day groups compare consecutive listed events in id order. The group marker `'\x01'` is replaced before `escape_history_text`, which works on `unsigned char`. Digits are built with `'0' + m` on a `size_t` below 10; numbers through `num`; no locale. The reader checks' `value_of` binary-searches the value rows in primary-key order, as the readers return them.
9. **Platform.** No `char` sign dependence, no shift of a signed value, no `size_t` stored (every count is cast to `int64` before it is written), no integer division of a negative operand.
10. **Build flags and data.** No CMake change. The new tuning fields are integer with ranges in `kinds_history.cpp`; the code defaults are inert.

## Advisories

**A1. A quiet renewal credits relations with an old event** (`alliances.cpp:451`). A renewal with nothing notable calls `relations_.credit(..., renew_memory, a.renew_event)`, the formation or the last written renewal, possibly many terms old. That is the spec's rule (6.2), and it is deterministic, but it breaks the seam contract recorded at P5-03a audit A4 (a caller passes an event of the current step): the same id can enter the pair's `warmer` list again at every quiet term (duplicates until the list's cap of 8 drops them), it can be cited as a cause of a later `disposition_turned` decades on, and it counts as a "fresh contribution" that lets a warmer turn through the cool-down of 7.2 (`relations.cpp:1290`). Proposed fix: either state in the spec and at `credit` that a quiet renewal is the one exception, or credit the memory without adding a contribution (a units-only overload).

**A2. A tower that lost its land inside one quiet step can be missed** (`figures.cpp:1373-1411`). `tower_endings` tests the current owner at each T3, T6 and Q4. A region or library settlement lost and won back by the realm between two such checks (inside one multi-year Q4) leaves the tower open, though the realm lost the land. Deterministic and a function of the schedule, so not blocking; it only matters if the spec means "ever lost". Proposed fix: none needed for determinism; if the rule should catch it, compare the region's or settlement's `owner_event` with the tower's origin event instead of the current owner.

**A3. Paths not exercised across compilers** in the five worlds: the at-once spurning (and `pending_spurns`), refusal motive 7 and tower ending 4. They were reviewed by reading only (items 2, 3 and 5). The slow suite or the `world-hashes.sh` case `p5-monsters-high-1024` may hit them; a case that does would let `compare-hashes` cover them on MSVC.
