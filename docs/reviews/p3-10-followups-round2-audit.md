# Determinism audit: P3-10 second round (generator 0.19.0, schema 7)

Auditor: determinism auditor, 2026-11-21. Change audited: `origin/p3/10-int2` at `25187f1`, against the first round's audited tip `14fc50d` (`docs/reviews/p3-10-followups-audit.md` on `origin/p3/10-audit`, pass with notes, A1 to A3), as `git diff 14fc50d..origin/p3/10-int2 -- worldgen tools/worldgen-cli game/data` without the test directories. Specs: `docs/specs/p3-10-followups.md` section 12 (items 18 to 26), P3-06 r17 section 22, P3-04 r15 20.1 item 1a, P3-05 r15 22.7a, P3-02 r13 section 13. Audit branch `p3/10-audit2`, cut from `25187f1`; it adds only this file. Line numbers are those of `25187f1`.

Scope (non-test code, 21 files):
- `worldgen/src/history/conflict.cpp`, `conflict_war.cpp`, `worldgen/include/worldgen/history/conflict.hpp`: `quarrel_with_`, `quarrel_held_back`, `lying_low` delegating to peoples, `aim_achieved`, the foothold cession (`cede(..., move_region = false)`), `hidden_years` from the founder's race, the A1 copy, the `quarrel_strain <= strain_max` guard.
- `worldgen/src/history/peoples.cpp`, `peoples.hpp`: `LowRecord`, `lie_low(civ, settlement, until)`, `lying_low`.
- `worldgen/src/history/engine.cpp`, `schedule.cpp`, `schedule.hpp`: `quiet_step_end(ends_row, next_length)`, the assured length, `stretch_decided_`, `quiet_min_years` and `engine_quiet_min`.
- `worldgen/src/history/names.cpp`: `lower_letter`, `triple_letter`.
- `worldgen/src/data/kinds_history.cpp`: race `hidden_years`, `quarrel_min_settlements`, `quiet_min_years`, checks `conflict_quarrel_strain_max` and `engine_quiet_min`.
- `game/data`: six race files, `history/conflict.toml`, `history/engine.toml`, the three name filter lists.
- No change to `tools/worldgen-cli/` (outside its tests), `cmake/`, `CMakePresets.json`, `.github/`, `tools/ci/`, `extern/` or `streams.hpp`. `GENERATOR_VERSION` stays 0.19.0 (unreleased; spec 12).

**Verdict: pass with notes.** No blocking, should-fix or advisory finding. Advisories A1, A2 and A3 of the first round are closed. No draw was added, removed or re-keyed and no stream was added: the diff has no `stream_key`, `substream`, `streams::` or `draw` line; the new rules only skip a keyed draw (a held-back declaration, the extra name attempts of the triple-letter rule, each keyed by its attempt index). Every new container is an ordered `std::map` keyed by civilization id; every new loop runs in id order or over a `std::map`. All new arithmetic is `std::int64_t`. Lint clean with its self-test; no new rule. Twenty worlds (five worlds, `--threads` 1, 2 and 6 with GCC, 6 with Clang) are byte-identical per world (section 4).

Classification as in the first round: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

None.

### Notes

**N1. Closure of the first round's advisories.**
- **A1, closed.** `awakening_quarrels` copies the hop row (`conflict.cpp:1452`, `const std::vector<std::int64_t> from = view.hops(cap.region);`), as `flight_plan` does (`conflict_war.cpp:202`). No output change.
- **A2, closed.** There is one predicate now: `PeoplesSubsystem::lying_low` (`peoples.cpp:1326-1338`), which returns true when the latest `LowRecord` has `year < until`, its settlement id is in range, and the refuge is owned by the civilization and standing. The 9.3 skip (`peoples.cpp:836`) and `ConflictSubsystem::lying_low` (`conflict.cpp:861-863`) both call it. `hidden_until_` is still written in `flee` (`conflict_war.cpp:277`) and read only by `hidden` and the hidden-year lookup (`conflict.cpp:707`, `837`), so it serves hiddenness only. Both values come from the same `until` (`conflict_war.cpp:276`), so the two cannot disagree on the year.
- **A3, closed.** `quarrel_strain <= strain_max` is checked as a named check in the schema (`kinds_history.cpp`, `conflict_quarrel_strain_max`) and again in the seam guard (`conflict.cpp:318-320`).

**N2. The quarrel record** (`quarrel_with_`, `std::map<std::int64_t, std::int64_t>`, `conflict.hpp:464`). It is set at `conflict.cpp:1479`, in `awakening_quarrels`, which runs over a copy of `quarrel_pending_` in id order. In `declare` (`conflict.cpp:1370-1376`) the entry is erased for (a, b) and (b, a) through `find`, and only when the value matches. The housekeeping erase (`conflict.cpp:1063-1070`) walks the map in key order with the `erase`-returns-next idiom, and its predicate reads only `ended_year`. `quarrel_held_back` (`conflict.cpp:869-876`) is a pure read: a `find`, a `claims_.count`, and the size of `visible_settlements`, an id-ordered vector. In `declarations` it sits after the `declare_min` test and before `odds_of`, and both skip without a draw. In the tension it sits before `odds_of`, which takes no draw. The public accessor returns a const reference to an ordered map. The flare branch of `declarations` (`conflict.cpp:1307-1310`) has no hold-back test of its own, and it does not need one. The flare pair is recomputed by `best_claim` in the flare step itself (`conflict.cpp:1283-1287`), on that step's state, and `best_claim` already excludes a held-back claim. The only state that changes between the two reads within the step is from earlier `declare` calls in the same loop, which touch wars and relations, not the settlements or visibility that n(a) counts. So 22.1 item 3.3 holds as written.

**N3. Aim and the foothold cession.** `aim_achieved` (`conflict_war.cpp:539-541`) is static and pure: the region owner plus `standing_settlements_in`, which builds an id-ordered vector from the live list by index. `end_war` (`conflict_war.cpp:562-566`) computes `foothold` from a copied vector and `std::find`. `loser_capital == 0` is never a settlement id, so the test is total. A foothold war skips `cost_plan` and counts as a cost for the quiet-event budget. `cede(..., false)` (`conflict_war.cpp:647-674`) is the existing transfer: an id list taken before any change, each settlement re-fetched by id, `refugees` per settlement in id order. Only `set_region_owner` is skipped, so no `region_control` row and no holder change. No reference into the live lists is held across `refugees`, which may found (P3-04 6.9). `quiet_ends` (`conflict.cpp:1395-1407`) binds `const ConflictWar& w = wars_[i]` only for the arguments of `end_war`, which are evaluated before the call, and `end_war` copies `wars_[i]` first (as in round 1, N9).

**N4. Hidden years per race.** `flee` (`conflict_war.cpp:276`) computes `checked_add(ctx.year(), race_of_civ(ctx, y).hidden_years)`, with `y` being the founder of the refuge; `hidden_years` is limited to 0 to 10000 by the schema. The `ConflictRace` aggregate gains the field as its last member, read in `prepare` (`conflict.cpp:602-606`) from each race record in chronology row order.

**N5. Quiet stretches** (P3-04 r15 20.1 item 1a). `schedule.cpp:276-277` passes `ends_row ? 0 : min(Q, q_e - s)`, after `s += len`, which is the spec's `min(Q, q_e - (s + len))`. `engine.cpp:563-566` passes `checked_add(years_, next_length)`. `quiet_stretch` (`engine.cpp:580-616`) sets `stretch_decided_` only on a refusal. A row with no civilization alive stays undecided, so a later step of the row can decide it from its own length, as the spec says. The flag is reset in `quiet_row` (`engine.cpp:513`). These are integer comparisons, and `quiet_min_years <= 2 x Q` is checked in the schema and the guard (`schedule.cpp:102-104`). The schedule's own rows are unchanged. No event is removed or reordered: the decision is made before the append.

**N6. The triple-letter rule** (`names.cpp:93-107`, `218-220`). It works on bytes and is ASCII only. A UTF-8 byte at or above 0x80 is outside `'a'..'z'` and `'A'..'Z'` whether `char` is signed or unsigned, so `char` signedness cannot change the answer. `c - 'A' + 'a'` is an int expression in range. A rejected attempt is invalid. `choose_name_impl` (`names.cpp:439-458`) then moves to attempt `t + 1`, whose keyed draw is `draw(key, attempt, culture, entity, j)`, so no other draw shifts. Filter list edits are data and are read by the existing loader.

**N7. Data.** The shipped tuning satisfies the new checks: `quarrel_strain` 600 is at most `strain_max` 5000, and `quiet_min_years` 50 is at most twice `quiet_step_years` (`engine.toml`). `hidden_years` is 100 for orcs and 300 for the others. The removed key `history.conflict.hidden_years` is refused as unknown by the schema. Every world's `content_hash` changes, as spec 12.4 states.

**N8. Resume and threads.** The new state (`quarrel_with_`, `LowRecord`s, `stretch_decided_`) lives in memory only and is rebuilt by the serial history stage on resume, from the same inputs, like round 1's state (round 1 N11). No history code uses the parallel facility.

## 2. What was checked

- Floating point and platform math: none in the diff (lint `float-type`, `std-math`, `banned-include` clean; also read by hand).
- Random streams: no stream added and no key changed. The only draw effects are skips (held-back declaration) and extra keyed attempts (names), checked by hand and by grep of the diff (lint `history-sequential-rng`, `two-draws` clean).
- Unordered iteration and pointer order: `quarrel_with_` and `low_until_` are `std::map` keyed by id. The one new loop (housekeeping) runs in key order. There is no pointer comparison (lint `unordered-container`, `pointer-order`, `std-hash` clean).
- P3-04 6.9: no new range-for over `civs()`, `settlements()` or `relations()`. The foothold cession reuses `cede`'s id-list-then-refetch form. No reference is held across a founding (lint `history-live-iteration`, `history-held-reference` clean).
- Overflow and undefined behaviour: `checked_add` for the hidden year and the assured length. The settlement count is cast from `size()` to `int64`. No shift and no narrowing beyond `static_cast<char>` of an ASCII letter. The settlement id in `lying_low` is range-checked before `state.settlement`.
- Containers that reach output: events are appended in the existing fixed order. The foothold `region_ceded` uses the existing writer. No schema change (7).
- Rendered text: no new formatting code.
- Build flags: unchanged.

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test` passed (18 snippet files). `python3 -B tools/lint/determinism_lint.py` scanned 164 C++ files and 13 CMake files with 0 findings. No rule was added. No new pattern in this round can be detected mechanically without false positives.

## 4. Experiments

I built `linux-gcc-release` and `linux-clang-release` (`worldgen-cli`) from `25187f1` in the audit worktree. Worlds were written under `~/.cache/the-unmapped/out/p3-10-audit2/` with `--data game/data`. Nothing was written to the source tree. The counts come from the GCC `--threads 6` file. "foothold" is the number of `region_ceded` events with no `region_control` row naming them.

| World | GCC t1 = t2 = t6 = Clang t6 | SHA-256 (first 16) | claims, declarations, `region_ceded`, foothold, `refugees_settled`, `quiet_stretch`, `search_abandoned` |
| --- | --- | --- | --- |
| seed 23, 1024, default | yes | `8b7489906373b38a` | 45, 27, 10, 0, 1, 8, 2 |
| seed 40, 1024, default | yes | `4bae3ecb142ef8ba` | 70, 31, 8, 1, 2, 8, 2 |
| seed 53, 1024, default | yes | `8b75de8970a8c9da` | 16, 11, 3, 0, 0, 6, 3 |
| seed 5, 1024, `turbulent` | yes | `567743213242ac15` | 207, 113, 36, 0, 2, 11, 4 |
| seed 3, 512, default | yes | `56e9b84e8aefca60` | 22, 17, 6, 1, 1, 7, 2 |

Between them these worlds exercise the foothold cession (seeds 40 and 3), flights with hidden refuges and lying low (`refugees_settled`), quiet rows under the 50-year rule, and orc quarrels. MSVC is covered by the validation specialist's `world-hashes.sh compare`.

## 5. Verdict

**Pass with notes.** No blocking, should-fix or advisory findings. A1, A2 and A3 of the first round are closed (N1).
