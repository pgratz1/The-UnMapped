# P5-01 carried-over follow-ups: determinism audit

Auditor: determinism auditor (Opus), 2026-11-22. Branch audited: `origin/p5/01-int` at caed80e. Library change: `git diff origin/p5/01-spec...origin/p5/01-impl -- worldgen/include worldgen/src tools/worldgen-cli game/data` (commit 60afe87); the integration commits after it touch tests and the data files only (no library source differs between `p5/01-impl` and `p5/01-int`). Spec: `docs/specs/p5-01-followups.md` with P3-06 r22 24, P3-04 r19 22, P3-07 r19 25, P3-01 r15 3.8.

This was a reading audit plus the lint. Nothing was rebuilt or rerun: integration reports the C++ history tables equal to the independent reference on every golden world case, and byte-identical reports at both levels under GCC release and Clang debug.

**Verdict: pass with advisories.** I found no blocking finding. There is one advisory (A1).

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 18 snippet files, 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 167 C++ files and 13 CMake files scanned, 0 findings, exit 0.
- The diff adds no suppression. The engineer fixed both lint hits named in the brief instead of suppressing them. `claim_event` sorts its causes with `std::stable_sort` (`worldgen/src/history/conflict.cpp:1023`). The new flags use `std::vector<std::uint8_t>` (`digest.cpp` `dropped`, `phase_war`, `cal`; `event_catalogue.cpp` `clause_words`). The existing rules `unstable-sort` and `packed-bool` cover both patterns.
- I added no lint rule. I found no new construct worth a rule. P3-07's audit already decided against a rule on `std::map::at`.

## What was checked

1. **Toll draws** (`worldgen/src/history/conflict_war.cpp:114-124`).
   - Draws: keyed only, `ctx.draw(keys_[TOLL], word(w0.declared), 0, k)` with k = 0 for the loser and 1 for the winner. `StepContext::draw` adds the year (`engine.hpp:247`), so the key is (Y, w, 0, k) as P3-06 24.5 specifies. The key is unique because a war fights at most one battle a year (7.3 item 2), and `hist.conflict.toll` is a stream of its own, so the key cannot collide with the `battle` or `battle_win` keys.
   - Draws are taken only when `s > 0`, so the hand-made default of 0 takes none.
   - The stream is registered last in `streams.hpp` (54 names) and seventh in `STREAM_NAMES`, matching `ConflictStream::TOLL`. `keys_` grew to 7 and is filled by the existing loop (`conflict.cpp:598`).
   - Arithmetic: integers only. The factor is `1000 - s + pick(u, 2s + 1)` (`pick` is integer, `draw.cpp:23`). The scaling uses `floor_div(checked_mul(dead, v), 1000)`: v ≤ 1500, and an overflow fails deterministically. The two factors are separate full expressions, so evaluation order does not matter.
   - Data checks: `conflict_toll_spread` (`kinds_history.cpp:691`) and its copy for hand-made tuning (`conflict.cpp:327-329`) are int64 products of values of at most 1000 × 1500.
2. **`claim_event`** (`conflict.cpp:991-1026`).
   - It is used by `update_strain` (`:1290`, with no extra causes, so the cause is the same as at 0.20.0) and by `declare` (`:1413-1418`, the old claim as extra cause).
   - Causes are sorted ascending and made unique.
   - The settlement list comes from `seen_settlements` (ascending id). The loop condition `i + 2 < MAX_PARTICIPANTS` (64) caps the list at 62.
   - The most populous settlement is chosen with ties going to the first, hence the smallest id.
3. **Pure reads.**
   - `strength_of` and `claim_defence_force` (`conflict.cpp:879-897, 976-989`) are `const`. They read `force`, `live_front` and `defence_at`, also `const`. `claims_.find` replaces `.at` and fails explicitly when the claim is missing.
   - `quarrel_held_back` (`:879`) now calls `force()` through `strength_of`, from the tension path (`best_claim`, `:1312`) and from the declarations (`:1373`). No draw is taken and no state is written.
   - The only `mutable` members near these paths are the peoples caches (`peoples.hpp:249, 261-262`). They existed before this change and are filled lazily. History runs on one thread, since no history source uses threads.
4. **P3-04 6.9 at the new append sites.** None of the new appends is a founding.
   - `claim_dropped` (`conflict.cpp:1401-1405`) and the fresh claim (`:1413-1418`) are appended in `declare`. `declare` works on a copy of the claim (`const Claim claim_of`, `:1391`). It is called from `declarations()` inside a loop over the copied vector `alive`, not inside the loop over `claims_`. The `claims_` erase and assignment happen outside any iteration of `claims_`.
   - `refuge_hidden` (`conflict_war.cpp:292-299`) is appended after `found_settlement`. It uses ids only (`h`, `y`, `settled`, `plan.region`) and holds no `Settlement&` across the founding.
   - `end_year = min(until, span) - 1` uses int64 on both sides (`params.hpp:30`).
5. **Schedule** (`worldgen/src/history/schedule.cpp`).
   - `flare_min = scale_years(flare_min_years, span)` (`:134`) uses the same integer scaling as the other lengths. The test `f >= max(1, flare_min)` (`:277`) has no draw.
   - The fatal guard (`:309-311`) checks every flare phase row after tiling.
   - The tuning check (`:107-110`) and the data check `engine_flare_min` (`kinds_history.cpp:371`) iterate the fixed level arrays in a fixed order.
6. **Number clauses** (`worldgen/src/history/event_catalogue.cpp:940-1050`). `clause_cuts` compares bytes against `';'`, `','` and `' '`. The clause flags are per-token integers. Numbers are formatted with the project's `sentence_value_text` and `grouped_text`, with no locale, ctype call or stream.
7. **Digest** (`worldgen/src/history/digest.cpp`).
   - The new indexes are `dropped` (by event id, size n + 1, bounds-checked, `:820-827`), `refuge_hidden_end` (by settlement id − 1, bounds-checked, smallest event id first, `:821-835`), the calamity cap `per_phase` (by phase id, bounds-checked, `:1095-1113`) and `phase_war` (by phase id, `:1135-1146`). Every one is a vector filled in event-id order, and none is keyed by hash or pointer.
   - The milestone sort adds the integer key `cal` and keeps the id as the final tie-break, so its order is total.
   - **Advisory A3 of the P3-07 audit is resolved.** Both `std::map::at` calls are now `find`, with defined fallbacks: `first_since` falls back to E (`:780-787`), and `last_claim` falls back to "not the last claim" (`:855-859`). `digest.cpp`, `digest_tables.cpp` and `history_report.cpp` contain no `.at(`.
8. **Report v2** (`worldgen/src/inspect/history_report.cpp`). The text is ASCII and integers only (`num`), with no float and no locale. Every list keeps a defined order:
   - the adversaries are in the digest's order, and the "without war" count line is a sum;
   - `[stats]` is filtered against the fixed array `SUMMARY_STATS` in the digest's order;
   - the ended block uses `milestones_ended`, sorted ascending;
   - `ruin_how` and `refuge_state` are pure functions of rows.
9. **MSVC `/W4` hazards.** In the diff I found:
   - no C4996 functions;
   - no implicit narrowing: the casts to `uint64_t`, `size_t` and `int64_t` are explicit, and the literal `1` passed to `uint8_t` vectors is a constant that fits;
   - no signed/unsigned comparison: the id-to-`size_t` comparisons cast after a `>= 1` test;
   - no shadowing in `battle`, `flee` or `render_event` (C4456/4457): `s`, `n`, `r` and `v` are new names in their scopes.

   `std::find` of a `std::string` over `std::string_view` elements is standard. `<span>` and `<algorithm>` are included where they are used.
10. **Version, schema and data.**
    - `GENERATOR_VERSION` is `"0.21.0"` (`version.hpp:47`).
    - `docs/specs/schema.md` lists kinds 41 and 42 (lines 180-181), with the version 7 note at line 132 and the kind paragraph at line 183. Their roles match `event_catalogue.cpp:840-844`.
    - Every changed file under `game/data/` (cultures/dwarven, history/{conflict,engine,report}, name_filters/{crude,famous,slurs}, placenames/{common,traits}) is byte-identical at HEAD to its copies under `worldgen/tests/data/geo/` and `geo-small/`.
    - The shipped values match the spec: `quarrel_strength_permille` 1000, `toll_spread_permille` 200 (80 × 1200 ≤ 250 × 800), `flare_min_years` 10, which is at most every level's `flare_years` (30, 50, 75).
    - After 60afe87, the integration commit 2beaade reflowed two filter lists (`famous.toml`, `slurs.toml`) and edited the placename lists. The goldens are recorded against HEAD's files, which is what the cross-preset comparison checks.
11. **General.** The diff has no unordered containers, no pointer-keyed maps or sorts, no wall-clock time, environment or file-system order, no threads, and no `float` or `double`. The new maps and keys are `std::map` on integer pairs.

## Findings

### Blocking

None.

### Advisory

**A1. The refuge line indexes the digest's vector by settlement id without a local bounds check** (`worldgen/src/inspect/history_report.cpp:615-621`, `d_.refuge_hidden_end[settlement_id - 1]`). This is currently safe:
- `read_history_tables` rejects settlement ids that are not dense from 1 (`worldgen/src/history/digest_tables.cpp:329`);
- the vector is sized to `t_.settlements.size()` (`digest.cpp:821`);
- the id comes from a settlement row;
- `last_held_by` already uses the same pattern (`history_report.cpp:543`).

A later change that built the digest from a filtered settlement list would read out of bounds: undefined behaviour, and possibly different text on different platforms. Suggested fix, at the engineer's convenience: route both lookups through one helper that fails explicitly when the index is out of range, or note at the two sites that they rely on the density check.

## Verdict

**Pass with advisories** (A1, advisory only). No blocking findings.
