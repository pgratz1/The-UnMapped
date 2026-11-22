# Determinism audit: P3-11 follow-ups (generator 0.20.0, schema 7)

Auditor: determinism auditor, 2026-11-21. Change audited: `origin/p3/11-impl` at `7d17a4b`, against `origin/main` (generator 0.19.0), as `git diff origin/main...origin/p3/11-impl -- worldgen tools game/data`. Specs: `docs/specs/p3-11-followups.md`, P3-02 r15 section 14, P3-04 r17 section 21, P3-05 r16/r17 section 23, P3-06 r19 section 23, P3-07 r14 section 23. Audit branch `p3/11-audit`, cut from `7d17a4b`; it adds only this file. Line numbers are those of `7d17a4b`.

Scope (non-test code):
- `worldgen/src/history/names.cpp`, `names.hpp`: `Syllable`, `syllables`, `repeated_syllable`, the `drawn` list and `NameAttempt::repeated`.
- `worldgen/src/history/event_catalogue.cpp`: `sentence_value_text` (one to nine as words) in the `value` and `choice` tokens; kinds 39 `claim_settlements` and 40 `settlements_ceded`.
- `worldgen/src/history/conflict.cpp`: kind 39 in `update_strain`'s claim step, with the holder's seen settlements as participants.
- `worldgen/src/history/conflict_war.cpp`: `cede` takes its id list before the append; kind 40 for the foothold cession.
- `worldgen/src/history/digest.cpp`, `digest.hpp`: `ClaimKinds`, `append_entity`, `war_note` (smallest-id claim cause, region-aware text), `flare_settlements_note`.
- `worldgen/src/history/peoples.cpp`, `peoples_geo.cpp`, `peoples.hpp`: `awakening_population`, `first_colony_pending_`.
- `worldgen/src/data/kinds_history.cpp`: race field `awakening_population`.
- `game/data`: six race files, `cultures/faerie.toml`, `history/report.toml`, three name filter lists; the same in `worldgen/tests/data/geo` and `geo-small`.
- `worldgen/include/worldgen/version.hpp`: 0.20.0.
- No change to `tools/worldgen-cli/`, `cmake/`, `CMakePresets.json`, `.github/`, `tools/ci/`, `extern/`, `streams.hpp`, the schema or the database layer.

**Verdict: pass.** No blocking, should-fix or advisory finding. No draw was added, removed or re-keyed and no stream was added: the diff has no `stream_key`, `substream`, `streams::` or `draw` line. The syllable rule reads strings already drawn, and the first-colony rule only lets an existing `hist.peoples.found` (and `hist.peoples.outward`) draw be reached sooner. The one new container is an ordered `std::set<std::int64_t>` that is never iterated. All new arithmetic is `std::int64_t`. History runs serially. Lint clean with its self-test; no new rule. Six worlds (GCC `--threads` 1, 2 and 6, Clang 6) are byte-identical per world (section 4).

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

None.

### Notes

**N1. `first_colony_pending_` and its lazy clearing** (`peoples.hpp:481`, `peoples.cpp:407`, `642`, `849`, `863`, `967`).
- **Container.** `std::set<std::int64_t>` keyed by civilization id. It is only ever touched by `insert`, `erase` and `count`, and it is never iterated, so its order cannot reach output either way.
- **Insert.** `awaken` (`peoples.cpp:407`), for every civilization placed at an awakening of a race with `awakening_population > 0`, after `set_capital`. Secession does not call `awaken`, so it adds nothing (23.2 item 2).
- **Erase.** At the one `colony_founded` site (`peoples.cpp:967`, after `found_settlement` and the region owner; grep shows `colony_founded` is appended nowhere else). In `PeoplesSubsystem::end_civilization` (`642`). Lazily at the top of `foundings` (`849`) for a civilization whose `ended_year` is set, which covers an ending by conflict's conquest.
- **Read.** The only read in library code is `peoples.cpp:863`, in `foundings`, after the `ended_year` test at `848`, which skips an ended civilization before the read, as it did at 0.19.0. The test seam `first_colony_pending(civ)` (`peoples.hpp:334`) is the only other reader, and no library code calls it.
- **Why the lazy clearing changes no output.** An ended civilization left in the set is never read before it is erased: the only read sits behind the `ended_year` skip. Civilization ids are never reused (`civ_count + 1`), and an ended civilization stays ended, so a stale entry cannot apply to another civilization. So the lazy erase and an erase at the conquest give the same bytes. The only visible difference is the test seam returning true for a conquered civilization until its next T2 or Q4, which its comment states.
- **Readiness.** `!(pending && id == capital)` (`peoples.cpp:879`) reads `state.civ(c).capital`, an id copied at `861` before the loop. The source key, hunger, the chance (`draw(key(FOUND), word(c), 0, 0)`, keyed by year and civilization) and the outward draw are unchanged. A pending civilization with no target now accrues hunger where at 0.19.0 it had no ready source. That is the rule as written (23.2, "hunger grows as before"), not a determinism matter.

**N2. P3-04 6.9 in the peoples change.** `foundings` keeps its 0.19.0 form: civilizations by index `1..nc` with `nc` taken at the start, settlements by index, and every `state.settlement(id)` re-fetched by id. The new lines hold no reference into the live lists. `pending` is a `bool`, `capital` an id, and `r` refers to the geography's race table, not the state. The erase at `967` comes after the founding. In `awaken`, `p0` is computed from `g.capacity(home, race)` before the append and the founding, and nothing new is held across them. Lint `history-live-iteration` and `history-held-reference` are clean.

**N3. The awakening population** (`peoples.cpp:355-361`, `391`). `max(founding_population, min(awakening_population, C))` on `std::int64_t`. The field is bounded 0 to 100000 by the schema (`kinds_history.cpp:749`), and C is bounded by the existing capacity checks, so there is no overflow. The same `p0` feeds `founding_kind`, the `settlement_founded` value and `found_settlement`. With the field at 0, the expression returns `founding_population`, so the five races shipped at 0 are 0.19.0's rule byte for byte. Every data root sets the field: 15 files at 0 and 3 at 1200, the orc file in `game/data` and in both fixture roots. No race file under `game/data` or `worldgen/tests/data` lacks it.

**N4. Kind 39** (`conflict.cpp:1225-1253`).
- **Participants.** `seen` comes from `seen_settlements`, which builds an ascending-id vector from the live list by index. The participants are claimant, holder, then `seen[i]` for `i + 2 < MAX_PARTICIPANTS` (64), so at most 62 settlements, and at most 64 participants, the engine's bound.
- **Cause.** The cause (the most populous seen settlement, first by id on ties) and its loop are unchanged.
- **The new fatal guard on an empty `seen`** cannot fire on a consistent state. The claim region comes from b's front, which lies in `presence(b)` (`conflict.cpp:660-689`). A region b does not own is in `presence(b)` only when b has a visible, non-hidden settlement there. Nothing between the snapshot and the claim changes owners, settlements or the year: the claim loop only appends claims.
- **No other key lookup.** No other code looks the claim up by kind key: `claims_` stores the event id. So kind 39 changes nothing downstream except the stored rows and the digest's note.

**N5. Kind 40 and the earlier id list** (`conflict_war.cpp:654-685`). `standing_settlements_in` is now called before `point_event`/`ctx.append` instead of after. `append` writes an event and does not change a settlement's owner, standing or region, so the list is the same. It is still taken before any transfer (8.3 item 1), and every settlement is re-fetched by id inside the loop, across `refugees`, which may found. Participants are in ascending id, with the same 62 cap. The empty guard cannot fire: the foothold path (`conflict_war.cpp:572-573`) requires `there` to be non-empty, and only `close_war` and `note_futile`, which touch relations and bookkeeping, run in between. `region_ceded` is unchanged for `move_region = true`.

**N6. The repeated-syllable rule** (`names.cpp:118-152`, `270-274`, `284-293`). It compares `std::string`s byte for byte and does no arithmetic and no lookup by `char` value, so `char` signedness and locale play no part. The `drawn` vector is filled by the same `pick_from(..., u(j))` calls as before, in the same order, so the draw indices are unchanged. A rejected attempt makes `choose_name_impl` move on to the next keyed attempt (as for the triple-letter rule, P3-10 round 2 N6), so no other draw shifts. The fatal checks for an unknown symbol or a count mismatch cannot fire, because `name_attempt` itself is fatal on an unknown symbol before it reaches them. The faerie lists use repeated entries as weights. `pick_from` indexes uniformly, so the weighting is pure data.

**N7. Numbers as words** (`event_catalogue.cpp:48-56`, `991`, `1002`). A fixed table indexed by `v - 1` only for `1 <= v <= 9`; every other value goes through `grouped_text`, as before. The rendered text is part of the `report` contract. It is fixed ASCII, so it is the same everywhere.

**N8. Notes in the digest** (`digest.cpp:71-205`). `war_note` takes the claim cause with the smallest event id, a total order on integers, so the result does not depend on the order of `e.causes`. `flare_settlements_note` lists settlement participants in the order the view stores them (rows by key: event, then seq), which is the ascending-id order of N4. Names come from the stored tables. No floating point and no container keyed by pointer: the `const KindDef*` and `const Event*` are compared only to `nullptr`, never ordered.

**N9. Catalogue rows.** Kinds 39 and 40 are appended after kind 38 in `build_compiled`, so the `event_kind` and `event_kind_role` rows keep their ascending order. The schema stays at 7. Templates `{settlement:all}` use the existing `Form::all` rendering.

**N10. Resume and threads.** The new state (`first_colony_pending_`) lives in memory only. Resume reruns the whole history stage from a chronology world (`inputs.cpp`), so the set is rebuilt from the same inputs. No history code uses the parallel facility: `std::thread` and `parallel` appear only in geography stages, `core/parallel.cpp` and `generate.cpp`.

## 2. What was checked

- Floating point and platform math: none in the diff (lint `float-type`, `std-math`, `banned-include` clean; also read by hand).
- Random streams: no stream added and no key changed (grep of the diff). The effects on draws are an earlier arrival at an existing keyed draw (first colony) and extra keyed name attempts (syllables). Lint `history-sequential-rng` and `two-draws` clean.
- Unordered iteration and pointer order: one new `std::set`, never iterated. No pointer comparison (lint `unordered-container`, `pointer-order`, `std-hash` clean).
- P3-04 6.9: no new range-for over `civs()`, `settlements()` or `relations()`. No reference held across a founding (N2, N5).
- Overflow and undefined behaviour: integer `min`/`max` on bounded values. The new casts are `static_cast<std::size_t>(v - 1)` on 0..8 and none other. Loop bounds are `size_t` against `MAX_PARTICIPANTS`. No shift.
- Rows: events are appended in the existing order, participants in ascending id. No schema or writer change.
- Data: every race file in the three roots carries the new required field. The filter and culture edits are data read by the existing loader. Every world's `content_hash` changes, as expected at 0.20.0.
- Build flags: unchanged.

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test` passed (18 snippet files). `python3 -B tools/lint/determinism_lint.py` scanned 164 C++ files and 13 CMake files with 0 findings. No rule was added. The new patterns (an ordered set as subsystem state, a byte comparison of drawn strings, participant lists from id-ordered vectors) are already covered by the existing rules or are safe by construction. The one fragile shape, a member set read only behind an ended-civilization skip, cannot be told apart from correct code mechanically.

## 4. Experiments

I built `linux-gcc-release` and `linux-clang-release` (`worldgen-cli`) from `7d17a4b` in the audit worktree. Worlds were written under `~/.cache/the-unmapped/out/p3-11-audit/` with `--data game/data`. Nothing was written to the source tree. The counts come from the GCC `--threads 6` file.

| World | GCC t1 = t2 = t6 = Clang t6 | SHA-256 (first 16) | claim, claim_settlements, war_declared, region_ceded, settlements_ceded, colony_founded; max participants of kinds 39/40 |
| --- | --- | --- | --- |
| seed 1, 1024, default | yes | `095ab58cf4c3b419` | 52, 1, 23, 12, 0, 113; 3 |
| seed 23, 1024, default | yes | `e084f100e235a163` | 46, 2, 32, 14, 0, 99; 4 |
| seed 40, 1024, default | yes | `d6c0071d6974dd7d` | 64, 3, 38, 8, 1, 92; 3 |
| seed 5, 1024, `turbulent` | yes | `12b6dd9df01c8542` | 209, 15, 112, 35, 0, 149; 4 |
| seed 3, 512, default | yes | `3a1f765d2075fe0b` | 24, 1, 20, 5, 0, 42; 3 |
| seed 65, 1024, default | yes | `6b9d3827f3b6cb2c` | 79, 5, 43, 15, 0, 148; 3 |

`report` text is identical between GCC and Clang for seeds 1 and 5 (turbulent). The worlds cover both new kinds (kind 40 in seed 40), orc awakenings with the 1,200 population and the first-colony rule, and the faerie syllable lists. MSVC is covered by the validation specialist's `world-hashes.sh compare`.

## 5. Not checked

- MSVC and Windows: not built here. The cross-platform comparison is the validation specialist's `world-hashes.sh compare` in CI.
- No world in the sample has a pending civilization ended by conquest before its first colony. The lazy clearing is shown safe by reading the code (N1), not by a run.
- No world reaches the 62-settlement participant cap. The cap is checked by reading the code (N4, N5).
