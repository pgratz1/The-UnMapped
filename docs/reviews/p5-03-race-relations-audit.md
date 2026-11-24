# P5-03 Race relations, mixed settlements and migrations: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-24.
Scope: `git diff origin/main...HEAD -- worldgen/include worldgen/src tools/worldgen-cli tools/ci tools/lint game/data`, at `origin/p5/03-int` 6cabd78 (the implementation merged with the tests and with main). Tests and references were read as context only. Spec: `docs/specs/p5-03-race-relations.md` r1 (section 20 for the auditor), with P3-04 section 6.9. Generator 0.24.0, schema 10, history report format v5.

**Verdict: pass with advisories.** I found no blocking finding. There are three advisories (A1 to A3). I extended the lint rule `history-member-held-reference` to `RelationsSubsystem::places_`.

Not reported, as the brief asks: defect D1 (the two history readers give different first errors on one damaged file), which is being fixed on another branch.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 21 snippet files (20 before this audit), 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 184 C++ files and 13 CMake files scanned, 0 findings, before and after the extension.
- The diff adds no `determinism-lint: allow` suppression.
- **Rule `history-member-held-reference` extended** (`tools/lint/determinism_lint.py`, `tools/lint/README.md`, new self-test `tools/lint/selftest/bad_history_place_held.cpp`).
  - `RelationsSubsystem` keeps the shares of each settlement in its own vector `places_` and hands out `Place&` through `place(id)`. `know_settlements` appends to it. So do the people ledger's calls `moved` and `displaced`, because they call `know_settlements`, and so does `expulsions`, because it calls `displaced`. An append can reallocate the vector, which is the same hazard as a monster or a site.
  - The rule now also reports a local `Place&` or `Place*` (or an `auto&` / `auto*` from `place(`) that is used after such a call in the same block. The type and accessor patterns are now built from `_MEMBER_VECTORS` and `_MEMBER_ACCESSORS`, so the next subsystem vector is a one-line addition.
  - The current code passes. In `expulsions` (`relations.cpp:823-851`) the references `p`, `e` and `w` are not used after `displaced(`, and the loop breaks straight after it. In `pending_displacements` the reference `hp` is taken after `know_settlements` (`:776`, `:780`).

## What was checked

1. **Random streams** (`core/streams.hpp:138-145`, `:218-221`; `peoples.cpp:53-61`; `monsters.cpp:58-67`; `relations.cpp:185`).
   - The four new streams are appended after `hist.monsters.raid_target` in the order of spec 3: `hist.beasts.bold_kind`, `hist.monsters.fear`, `hist.peoples.abandon_loss`, `hist.relations.expel`. `all_stream_names` grows from 72 to 76 entries, and every name is unique.
   - The subsystems' enum indices match their name arrays: `ABANDON_LOSS` is index 13 of peoples' 14 streams; `BOLD_KIND` and `FEAR` are indices 13 and 14 of monsters' 15.
   - Every new draw is a keyed `ctx.draw(key, a, b, k)`, with the step's year added by `StepContext::draw`. The keys are the ids of spec 3's table, never list positions:
     - bold_kind (region, 0, 0): `monsters.cpp:724`;
     - fear (monster, settlement, 0): `monsters.cpp:1190`;
     - abandon_loss (settlement, 0, 0): `peoples.cpp:797`;
     - expel (settlement, race, 0): `relations.cpp:831`;
     - the host name of a `dead` kind on `hist.names.monster` (monster, 0, 0), spec 12.2: `monsters.cpp:1647` (`place_name`).
   - **The plague plan's replayed draw.** `planned_plague` (`peoples.cpp:1157`) calls `road_loss(ctx, u, r.after)`. The real strike sets the population to `r.after` (`:1060`) and then calls `abandon` (`:1078`), which calls `road_loss(ctx, settlement, p)` (`:820`) with p equal to that population. Both calls are made in the same step, so they use the same year, the same settlement and the same population, and therefore draw the same value. The plan adds `min(r.after − road, room)` to the destination. The real ruin adds `min(p', room)`, where p' = p − road is set before `ruin` (`:839-841`). The two agree. The destination is `nearest_standing` with the new reach filter on both sides.
   - Each draw runs only after its preconditions. Expulsion: `people ≥ migrate_min` is tested first. Fear: the raid count, kind, capital and another settlement are tested first. Because every draw is keyed, a draw that is skipped shifts no other draw.
   - At most one expulsion draw is made per (settlement, race, year): expulsions run in T6 only. At most one fear draw is made per (monster, settlement, year): raids happen once a year per monster. At most one road-loss draw is made per settlement: a settlement is abandoned once.
   - Lint `history-sequential-rng` is clean. No two draws share one expression.
2. **Integer arithmetic** (spec 20's bounds).
   - The diff has no `float`, `double` or `<cmath>`. Every product and sum on a world value uses `checked_mul`, `checked_add` or `checked_sub`.
   - **Disposition.** `D = B + M / 1000` (`relations.cpp:307`). C++ integer division truncates toward zero for negative M, as spec 6.2.1 requires, on every compiler (guaranteed since C++11). `B = camp + floor_div(s_ab + s_ba, 2)` (`:210`).
   - **Memory.** `std::clamp` keeps M within the bounds × 1000, so |M| ≤ 2 × 10^6 (`:910`). Decay is d = years × rate × 1000 div 100 (`:921`): a quiet step's L × 10^4 × 1000 is far below 2^63.
   - **Shares.** `mix` computes c_g = share × P_b + share × n ≤ 1000 × 2^32 (`:451`). `largest_remainder` then computes c × 1000 < 2^52 (`:151`).
   - **Displacement and conquest.** The displacement split multiplies a share by the people, at most 1000 × P. The conquest share is x + (1000 − x) × permille div 1000 (`:522`).
   - **Drift** is share × rate × Δ ≤ 1000 × 1000 × the span (`:566`). **Expulsion** is P × share div 1000 (`:829`).
   - **Tolls** (`monsters.cpp` `raid`): raw = P × τ div 1000, and the part above `kill_max` is multiplied by ≤ 1000. The overrun chance is min(1000, permille × M div 1000) × f_P div 1000 with f_P ≤ 1000. The hunt chance is min(max, base + (R − 1) × permille). The undead threshold is `undead_min_dead × 1000 div max(1, M)`. Road loss is P × rate div 1000 with rate ≤ 1000.
   - **No division by zero.** `largest_remainder` returns zeros when the sum is 0. `max(1, ·)` guards f_P and the threshold. `pick` gets span ≥ 1, because `peoples_abandon_loss_ordered` checks min ≤ max (`kinds_history.cpp:578`). `weighted` in `beasts_bold` cannot get all-zero weights: `menace_ ≥ bold_on_permille ≥ 1` requires Σ count × menace > 0 (`monsters.cpp:664`, `:711`).
   - `chance_pm` scales p ≤ 1000 by 2^32, as the existing helpers do.
3. **The largest-remainder split** (`relations.cpp:139-164`). The remainders are ordered by (remainder descending, index ascending) under `std::stable_sort`. That is a strict total order, so the result does not depend on the sort algorithm. At every call site the index order is ascending race id:
   - in `mix` the races are sorted and made unique (`:439-440`);
   - in `fill_with` and in the displacement groups the entries are kept in ascending race.
   This is the tie-break of spec 6.3.2 and 6.4.3. The units left over never exceed the number of positive remainders, so none goes to a zero weight.
4. **Iteration order.** No unordered container, `std::hash` or pointer key appears in the diff.
   - Pairs, settlements, civilizations, regions, entries, records and the log are visited in ascending id or record order.
   - `pending_moves` and `pending_displacements` move the record lists out before they iterate, so a record added meanwhile waits for the next step. `expulsions` is the only caller that adds a record during a step, and it runs before `pending_displacements`.
   - The host and meeting choices are minima of `std::tuple` keys ending in an id (`:757`, `:1035`).
   - Conflict's enmity loop iterates the ordered `alive` list. `enmity_opened_` is a `std::set` of id pairs.
   - `matched_form` and `epithets_` use list order and creation order.
5. **Section 6.9 and references.**
   - Relations calls no `found_*`, `open_relation` or `close_relation`. Its only `State` changes are `set_population` (`:792`, `:844`) and `PeoplesSubsystem::note_reduction` (`:845`), which only records an event id (`peoples.cpp:1424`).
   - Loops over settlements and civilizations take a count before the loop and re-fetch by id after `ctx.append`. In `standings` and `mixed`, entries are re-fetched with `place(id).entries[i]` after the append (`:622`, `:660`).
   - **The ledger calls** inside peoples and conflict change no `State` (`relations.cpp:667-692`). `ruin` (`peoples.cpp:606-614`), `refugees` (`conflict_war.cpp:440-457`) and `flee` (`:286`) call them after the population change, and they hold no reference across them that a ledger call could invalidate. `flee` calls `moved` after `found_settlement`, with ids only.
   - **Unknown settlements.** `moved` and `displaced` make every settlement known with only `founder_civ`, `founded_year` and `founding_event`, which never change. Learning a settlement early in a step therefore gives the same entry as learning it at the next step's start.
   - **Truce parties** are read through `event_participants` in stored participant order (`engine.cpp:438`).
   - **Monsters.** In `raid`, `t` is not used after `overrun`, and `after` is re-fetched. In `flight`, `t` is not used after `abandon`.
6. **Rows and the database.**
   - `race_relation` and `settlement_race` are `WITHOUT ROWID` tables with full primary keys (`db/schema.hpp:254-259`), added after `site` in `DDL_STATEMENTS`.
   - Rows are emitted in key order: pairs in ascending (a, b), then year; settlements by id, races ascending (`lower_bound` insert, `:1134-1138`), then year. `put_row` keeps one row per year.
   - `write_race_relation` and `write_settlement_race` refuse a key that is not strictly above the previous one (`writer.cpp:1090-1120`), so an ordering defect fails on every platform alike and never writes a different file.
   - The readers use `visit_table_rows`, which appends `ORDER BY` every primary-key column. The new text columns `bold_phrases` and `raid_phrases` are bound as text. There are no timestamps.
7. **Text.**
   - **Capitalisation** (`event_catalogue.cpp`, the end of `render_event_linked`) is a byte test `'a' ≤ c ≤ 'z'` and `c − 'a' + 'A'`, with no `<cctype>` and no locale. The same holds for `is_phrase_list`, and lint `ctype` is clean.
   - The phrase number is `(event − 1) mod n` on stored values. The report prints integers through the project's formatting (`num`, `int_text`), so the shares are per mille and have no fractional digits.
   - The new data files are ASCII and LF (`.gitattributes` `eol=lf`). No non-ASCII literal appears in the diff.
8. **Platform portability.**
   - The tuple comparisons are of heterogeneous `std::tuple<int64, int, ...>` against `std::tuple<int64, ...>`, which C++20's `operator<=>` defines.
   - No structured binding is captured by a lambda, which MSVC rejects. No `long`, `size_t` value is stored: sizes are cast to `int64` before they reach a row or a report.
   - `std::stable_sort` is used everywhere, with no `std::sort`. There are no statics, threads or clocks in the diff.
9. **Build flags.** No CMake change in scope. Lint `cmake-fp-flag` is clean.

## Blocking findings

None.

## Advisories

**A1. A ruling race can be mixed out of its own settlement** (`relations.cpp:453-476`, `:576-580`). `mix` drops every race whose share rounds to 0, the ruling race q_s included. For q_s to round to 0, an arrival must be about 1000 times the host's population, and the host's room (2 × capacity − P) allows that only for a tiny host in a large region. If it happens, `drift` adds the `gained` shares to the q_s entry, which no longer exists, so the settlement's shares no longer sum to 1000 (spec 5). The same bytes would come out on every platform, so this is not a determinism defect. But it breaks an invariant that the 16.4 checks may test. Suggested fix: in `mix`, keep q_s's entry at a share of at least 1 (take the unit from the largest share), or let drift re-create the entry. The test writer could add a case with a host of population 1 and room for 2000.

**A2. `RelationsSubsystem::disposition` fails before the subsystem's first step** (`relations.cpp:320-323`). Conflict reads the view from `disposition_of` and from its housekeeping. Every history starts with T1 or Q1, and relations runs last in that step, so a conflict read in the first step of the first phase, before relations has run, would fail. No current world does this: conflict has no pair to judge before the first awakening. The fatal error would be the same on every platform. Suggested fix: call `prepare` from the stage once the inputs are known, or document in the header that conflict must not read the view in the history's first step.

**A3. Row year for a quiet step's drift** (`relations.cpp:263-266`). The comment calls it "a reading": every row of a step, drift included, is written at the step's year Y. Spec 6.7 says "a quiet step's Y + L − 1 for drift". This is the same on every platform. It is recorded here so that the lead can confirm that the reading is meant, and that the test writer's reference uses the same year (the golden values agree only if it does).

## Verdict

**Pass.** No blocking findings. Advisories A1 to A3 can go to the engineer and the lead without holding the merge. The lint extension (`history-member-held-reference` for `places_`) is on branch `p5/03-audit`.
