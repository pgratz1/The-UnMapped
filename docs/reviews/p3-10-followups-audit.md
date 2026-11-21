# Determinism audit: P3-10 wording, site and conflict follow-ups (generator 0.19.0, schema 7)

Auditor: determinism auditor, 2026-11-21. Change audited: `origin/p3/10-int` at `14fc50d`, against `origin/main` `030c8e7` (generator 0.18.0), as the two-dot diff `git diff origin/main origin/p3/10-int -- worldgen/src worldgen/include tools/worldgen-cli game/data cmake CMakePresets.json .github tools/ci`. Change spec `docs/specs/p3-10-followups.md` r2. Audit branch `p3/10-audit`, cut from `14fc50d`; it adds only this file. Line numbers are those of `14fc50d`.

Scope (non-test code):
- `worldgen/src/history/engine.cpp`, `worldgen/include/worldgen/history/engine.hpp`: `RegionState.holder` and `holder_changes` in `State::set_region_owner`, `quiet_stretch` (kind 38) replacing the quiet eras, `StepContext::event_year`.
- `worldgen/src/history/peoples.cpp`, `peoples_geo.cpp`, `worldgen/include/worldgen/history/peoples.hpp`: kind 37 and the plague plan's `reached`, the cause memory (`recent`, `dwindle_cause`, `last_colony_`), the one last-refuge predicate (P3-09 audit A2), `lie_low` and `low_until_`, the site value with `cell_weights.port`, `habitability`, `inland` and the capped sea distance, the name core rule in `next_name`.
- `worldgen/src/history/conflict.cpp`, `conflict_war.cpp`, `worldgen/include/worldgen/history/conflict.hpp`: aim by holding the goal (`quiet_ends`, `end_checks`, `exhausted`), searches (`fought`, `searched_until_`, `search_weary`, `search_written_`), the holder in `futile_`, per-race `odds_min`, `lying_low`, the awakening quarrel (`quarrel_seen_`, `quarrel_pending_`, `awakening_quarrels`), `note_reduction` only with a loss, the `flight_plan` row copy (P3-09 audit A1).
- `worldgen/src/history/names.cpp`, `names.hpp`: `NameCores`, `name_core`, `within_one_edit`, `choose_name` with cores.
- `worldgen/src/history/event_catalogue.cpp`: kinds 37 and 38, `grouped_text` (thousands separators), the `{@causes:…}` placeholder (parser and renderer).
- `worldgen/src/history/digest.cpp`: the reworded flare-up note.
- `worldgen/src/stages/regions.cpp`, `regions.hpp`: the keeper per land noun, V7 and `region_name_stems`.
- `worldgen/src/data/kinds_history.cpp`: the new race, peoples and conflict fields and ranges, `conflict_quarrel_strain`, the template length 512.
- `worldgen/include/worldgen/version.hpp`: 0.19.0.
- `game/data`: race files, `history/*.toml`, `name_filters/`, `placenames/` (data, read through the existing loaders).
- Build: `CMakePresets.json`, `cmake/UnmappedTestShards.cmake`, `cmake/UnmappedCheckTestShards.cmake`, `.github/workflows/ci.yml`, `tools/worldgen-cli/tests/CMakeLists.txt` (slow suites).
- Tests (light look): the 90 changed files under `worldgen/tests/` and `tools/worldgen-cli/tests/`.

**Verdict: pass with notes.** No blocking and no should-fix finding. No draw was added, removed or re-keyed (no `stream_key`, `substream`, `streams::` or `.draw(` line in the diff; `streams.hpp` unchanged at 53 names); the new rules only skip keyed draws (a lying-low civilization's founding pass, the search in a region already fought over, extra name and region-name attempts), and a skipped keyed draw shifts no other. All new arithmetic is `std::int64_t` with `floor_div` and the checked helpers, within ranges bounded by the schema; no floating point, `long`, locale, `<cctype>` or standard-library hash. Every new container that decides an outcome is ordered and keyed by integers or strings (`std::map`, `std::set`), and the two loops over sets (`NameCores::near`, `lying_low`) compute any-of, which cannot depend on order. Every new sort has a total key. No reference into `State`'s lists is held across a founding. Same-seed worlds are byte-identical at `--threads` 1 and 4 (section 4). Lint clean; no new rule (section 3). Three advisories, notes.

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. `awakening_quarrels` holds a row of the hop cache across calls that may fill other rows.** `worldgen/src/history/conflict.cpp:1416` binds `const std::vector<std::int64_t>& from = view.hops(cap.region)` and reads it inside the settlement loop, which calls `disposition_of`, `alive_visible` and `hidden`. This is the pattern of the P3-09 audit's A1, which the change answered in `flight_plan` with a copy (`conflict_war.cpp:202`) and by documenting the cache (`peoples.hpp`, next to `hops_`: the outer vector is sized once at construction, `peoples_geo.cpp:627`, and a fill assigns only its own row, `peoples_geo.cpp:680-699`). With that documented invariant the reference is valid, and the header says so; `local_force` (`conflict.cpp:754`) relies on it the same way. Fragile only if the cache is ever changed to grow, be cleared or become a map. Fix (no output change), optional: copy the row as `flight_plan` does, or name `awakening_quarrels` in the header comment beside `local_force`.

**A2. Lying low is read in two places with two predicates.** Peoples skips a civilization's foundings while `ctx.year() < low_until_[c]` (`peoples.cpp:836`), the year set by `flee` (`conflict_war.cpp:278`); conflict's `lying_low` (`conflict.cpp:856-864`) asks whether a settlement of `hidden_until_` is still standing, owned by the civilization and hidden. They differ when the hidden refuge is taken, abandoned or ruined before its hidden year: conflict resumes claims, strain and declarations, while peoples still founds no colony until the year. Both read only ordered, integer-keyed state and the same code runs everywhere, so this is not a platform risk; P3-05 r12 22.7a states the year form for peoples, so it matches the spec. It is the same shape as the P3-09 audit's A2 (one rule stated twice), and a later edit to one side would split them silently. Fix (no output change): a comment at each pointing to the other, or a spec note that the two predicates are meant to differ.

**A3. The quarrel strain can exceed `strain_max`.** `conflict.cpp:1441-1442` sets `strain_[Pair{a, b}]` to at least `quarrel_strain x 1000`; `quarrel_strain` is checked against `claim_strain` (`kinds_history.cpp`, `conflict_quarrel_strain`; `conflict.cpp:315`) but not against `strain_max` (range 1 to 1000000), while every other strain is capped at `strain_max x 1000` by `strain_after` (`conflict.cpp:418`). A hand-made tuning with `strain_max` below `quarrel_strain` would hold, for one step, a strain above the cap that `declare_chance` reads (its ramp is capped at 1000, so the chance stays bounded; the value is small, at most 5 x 10^6, so no overflow). Shipped data: `strain_max` 5000, `quarrel_strain` 600. Deterministic on every platform; a contract gap only. Fix: `min(strain_max, quarrel_strain)`, or a check `quarrel_strain <= strain_max`.

### Notes

**N1. Holder and holder changes** (`engine.cpp:1018-1024`). `RegionState{civ, event, civ != 0 ? civ : r.holder, r.holder_changes + new_holder}` reads the old `r` in the initializers of a temporary that is then assigned, so no read sees a half-written value. `holder_changes` counts only a different non-zero civilization, so resettlement by the same holder clears nothing (ruling A). Both are in memory only (no column), set only through `set_region_owner` (the only writer of `regions_` besides the initial `resize`). `futile_count` (`conflict.cpp:837-846`) and `search_weary` (`conflict.cpp:849-854`) compare the stored holder and epoch by equality: pure integer reads.

**N2. Quiet stretch** (`engine.cpp:578-623`). One event per quiet phase row; realms in id order from `civs_`; when more than `MAX_PARTICIPANTS`, `std::stable_sort` on `(-people, id)` (ids distinct, so the key is total and stability does not matter), `resize`, then `std::stable_sort` by id (distinct). People summed with `checked_sub` over the settlements in id order. Later steps of the row only update `end_year` and `values[0]` of the stored event by index (`open_stretch_ - 1`), not by pointer.

**N3. Plague plan** (`peoples.cpp:1013-1066`). `reached` is incremented exactly where `total` gains a spread's dead, in the same ascending-id loop with the same predicates the strikes use; the plan reads a copy (`pop`, `up`), and the values are appended after `(dead, population)` in a fixed order (`peoples.cpp:965`).

**N4. Cause memory** (`peoples.cpp:510-525`). `recent` compares `ctx.year() - event_year(e)` with `cause_memory_years` (0 to 100000): integers, no overflow. `last_colony_` is written by `slot` before `found_settlement` (`peoples.cpp:935`), an index write, no reference held. `note_reduction` now runs only where `loss[k] != 0` (`conflict_war.cpp:485-488`), in ascending `k`.

**N5. Capped sea distance** (`peoples_geo.cpp:488-519`). A level-synchronous multi-source search over the 8-neighbour grid: a cell gets the first level that reaches it, so the result is the Chebyshev distance capped at `inland_cap` whatever the frontier order; computed only when a present race has `cell_inland > 0`, and read in `site_value` only when `cell_inland != 0`, which the schema bounds to 0 to 1000 (`kinds_history.cpp`), so the empty vector is never indexed. `inland_cap` is 1 to 256 (no division by zero, fits `uint16_t`). The site value's products are below 2^40 (`habitability` at most 65535 x 1000; `cell_inland x d x 255` at most 1000 x 256 x 255).

**N6. Name cores** (`names.cpp:406-520`). `NameCores::near` is an any-of over a `std::set<std::string>`; the answer does not depend on the order visited. `within_one_edit` compares bytes (equal or one substitution, insertion or deletion), so `char` signedness cannot matter. Cores are added in the order of the names drawn, which is the founding order. The cores belong to the peoples subsystem (civilizations and settlements); gods in chronology use the registry form without cores, as the spec says.

**N7. Region names** (`regions.cpp:1278-1397`). Keepers per land noun in a `std::map<std::string, std::size_t>`, filled in region-id order (ties to the first, the smaller id). V7's stems are byte-wise lower-cased (`lower_text`, ASCII only), "the " stripped, cut to five bytes (a multi-byte character may be cut, but bytes are the same everywhere); stems in a `std::set`, tested by any-of. Slot indices 0 and 1 are `{noun}` and `{suffix}` of `PLACE_LIST_FIELDS` (`regions.hpp:86-87`).

**N8. Rendering.** `grouped_text` (`event_catalogue.cpp:29-44`) works on the text of `db::format_i64` (no locale, `INT64_MIN` handled as text, never negated). `{@causes:…}`: the parser (`event_catalogue.cpp:243-290`) is a pure string function; the renderer sorts a copy of the cause ids with `std::stable_sort` on `int64` (equal elements indistinguishable), keeps each word once in first-seen order and joins with ", " and " and "; the `const std::string*` it collects point into the token and are only dereferenced, never compared. The flare-up note (`digest.cpp:97-105`) is concatenation.

**N9. Conflict rules.** `quiet_ends` (`conflict.cpp:1359-1376`) binds `const ConflictWar& w = wars_[i]` and uses it only in the arguments of `end_war`, which are evaluated before the call; `exhausted` and `w.attacker` are pure reads, so their unspecified order does not matter. `battle` works on a copy `w0` (`conflict_war.cpp:48`); `fought` is a sorted vector maintained with `lower_bound`. `search_written_` is a plain flag set after the first `search_abandoned`; events are appended in a fixed order, so "first of the log" is the same everywhere. `awakening_quarrels` picks the neighbour by the tuple `(within enmity_hops ? 0 : 1, hops or INT64_MAX, squared distance, id)`, compared lexicographically: a total order ending in the settlement id, no draw; squared distance through `checked_mul`/`checked_add` (coordinates below 2^12). `quarrel_pending_` and `quarrel_seen_` are `std::set<std::int64_t>`; pending is copied into a vector before the loop that erases from the set. `strain_[Pair]` created by `operator[]` is immediately set to at least `quarrel_strain x 1000`, and `strain_` is only looked up, never iterated, so the entry's existence decides nothing.

**N10. Founding and references.** The new code that founds is unchanged in shape: `foundings` skips lying-low civilizations by `continue` before any draw (`peoples.cpp:836`); `flee` calls `lie_low` after `set_region_owner`, both index writes. No new range-for over `civs()`, `settlements()` or `relations()`; the lint's `history-live-iteration` and `history-held-reference` rules are clean.

**N11. Resume and threads.** The history stage runs whole: on resume (`history_inputs_from_world`, `inputs.cpp:75-120`) the engine and every subsystem are constructed fresh from `HistoryInputs`, so each new in-memory state (holder, holder changes, `searched_until_`, `quarrel_seen_`/`quarrel_pending_`, `low_until_`, `search_written_`, `last_colony_`, `NameCores`, `sea_d_`) is rebuilt from the same inputs by the same serial code. `sea_d_` derives from the water layer, which R1 checks equal between the pipeline and the stored world (`history_engine_golden_resume_test.cpp:23`, and Y2 at line 27 compares the resumed world to one run). No history code uses the parallel facility (`grep` finds no `parallel` or `threads` in `worldgen/src/history/` or `stages/history.cpp`); the region-name code is serial.

**N12. Build and CI.** The slow-suite change adds CTest entries with label `slow`, two test presets, and one CI step on the two release presets. No compiler flag, no `extern/` change, no network fetch, no action pin change (the CI diff adds a `run:` step only). `test-base` excludes the label, so the default entries are those of 0.18.0. The lint's CMake scan is clean.

**N13. Tests.** The changed test files add no `float`, `double`, unordered container, clock, `rand` or environment read.

## 2. What was checked

- Floating point and platform math: none in the diff (lint `float-type`, `std-math`, `banned-include` clean; read by hand).
- Random streams: no stream added; no draw key changed; skipped draws only, all keyed (rule H4; lint `history-sequential-rng`, `two-draws` clean).
- Unordered iteration and pointer order: new containers `std::map`/`std::set` keyed by integers, `Pair` or strings; loops over them any-of or copied first. No pointer comparison (lint `unordered-container`, `pointer-order`, `std-hash` clean).
- Sorts: the two `stable_sort`s of `quiet_stretch` and the one in the cause renderer have total keys; `abandon`'s existing `stable_sort` of cause ids unchanged.
- Overflow: thousands (text), permille products (`cell_habitability`, `cell_port`, `cell_inland`, `odds_min`, `exhaustion_permille` through `checked_mul`), squared distances (checked), the quarrel strain (checked), people sums (checked).
- Undefined behaviour: no shift, no signed narrowing beyond `static_cast<std::uint16_t>` of values below 257; `sea_d_` never read when empty (schema ranges).
- State and references: section 1 N1, N9, N10, A1.
- Database: no schema change; rows are written by the existing ordered writers; kinds 37 and 38 appended to the catalogue in id order.
- Rendered text: no locale, no `std::to_string`, no stream formatting (lint `number-text`, `stream-format`, `locale` clean).
- Resume and `--threads`: N11 and section 4.
- Build flags: N12.

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test`: passed (18 snippet files). `python3 -B tools/lint/determinism_lint.py`: 164 C++ files, 13 CMake files, 0 findings. No new banned pattern in this change is mechanically detectable with few false positives (the row reference of A1 is now documented as valid, so a rule against it would flag safe code), so no rule was added.

## 4. Experiments

Built `linux-gcc-release` and `linux-clang-release` (`worldgen-cli` only) from `14fc50d` in the audit worktree; worlds written under `~/.cache/the-unmapped/out/p3-10-audit/`, nothing in the source tree.

| World (shipped data) | `--threads` 1 = 4 (GCC) | GCC = Clang (`--threads` 4) | SHA-256 (first 16) | claims, declarations, `search_abandoned`, `refugees_settled`, kind 37, kind 38 |
| --- | --- | --- | --- | --- |
| seed 42, 1024, default | yes | yes | `40aeb33c440d29b4` | 53, 30, 5, 0, 64, 11 |
| seed 36, 1024, default | yes | not run | `bfbb3d44d63bb423` | 46, 32, 7, 0, 85, 10 |
| seed 7, 1024, `--civs-per-race 4` | yes | yes | `6f5396d82b6e0f34` | 246, 119, 10, 1, 157, 23 |
| seed 3, 512, default | yes | yes | `1e834a21b6b9ed3a` | 24, 18, 2, 1, 36, 10 |

Between them these worlds exercise orcs (the awakening quarrel and odds 500), repeated searches (salience 15 and the weary years), a flight with a hidden refuge (lying low), outbreaks with spreads (kind 37's `total` and `reached`), quiet stretches, separators and the cause lists in rendered text (the `report` text is part of the compared bytes only through the database; the validation specialist's `world-hashes.sh compare` covers `report` and `hash` text on every preset, MSVC included). Seed 36 is the world of the same-step cession case named in the integration commit.

## 5. Verdict

**Pass with notes.** No blocking findings. Advisories A1 to A3 need no change for this merge.
