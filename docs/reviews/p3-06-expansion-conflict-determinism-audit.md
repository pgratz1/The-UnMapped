# Determinism audit: P3-06 expansion and conflict (generator 0.16.0, schema 7)

Auditor: determinism auditor, 2026-11-19. Change audited: `git diff origin/p3/05-int..origin/p3/06-int` at the tip `a121c72`, non-test code. Specs on that branch: `docs/specs/p3-06-expansion-conflict.md` r6 (8.3 and 8.4 hold the P3-04 and P3-05 audit rules as normative text; 16.0 the readings agreed in implementation), `docs/specs/p3-04-simulation-engine.md` r9 (6.7 item 2, 6.9 item 7). Line numbers are those of `a121c72`.

Scope:
- `worldgen/src/history/conflict.cpp`, `conflict_war.cpp`, `conflict_internal.hpp`, `worldgen/include/worldgen/history/conflict.hpp` (new).
- `worldgen/src/history/engine.cpp`, `engine.hpp`: `StepContext::event_kind` / `event_participant`, `State::relation_ops_` and the relation pass of the bookkeeping, two new fatal messages.
- `worldgen/src/stages/history.cpp` (conflict installed after peoples, the only tension source), `event_catalogue.cpp` (kinds 20 to 30), `core/streams.hpp` (five streams, 51 names), `data/kinds*.cpp`, `data/schema.hpp` (id `conflict`, race field `prowess_permille`), `game/data/history/conflict.toml`, `report.toml`, `game/data/races/*.toml`.

Read for context: `history/peoples.cpp` (`ruin`, `note_reduction`, `PeoplesView`, T1's `end_empty_civilizations`), unchanged since the P3-05 audit.

**Verdict: pass with notes.** No blocking and no should-fix finding. The subsystem founds nothing, so the reallocation hazard of the earlier audits cannot arise in it. Every pass that closes relations first copies the keys. Wars are visited by index over a count taken before the loop, and the vector grows only in T2. Every random decision is a keyed draw whose key is unique within its year. All arithmetic is `checked_*` / `floor_div` with every divisor guarded on the legal tuning range. Every map is ordered by integers, every arg-max breaks ties by id, and `tension()` writes nothing. The new relation bookkeeping replays calls in order, needs nothing from the database, and leaves an empty call list at every bookkeeping, so also at the stage's end. Same-seed worlds are byte-identical across thread counts, and the independent reference agrees (section 4). Two advisories, eleven notes. No lint change was needed: both history rules are clean on the new files, and no form used here escapes them (section 3).

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. With legal tuning, casualties can empty a standing settlement; spec 8.4 item 3 says they never do.**
- `conflict_war.cpp:100-103`, `340-360`. dead_L = F_L × `loss_loser_permille` div 1000, with F the side's effective force = levy × prowess. The schema allows `levy_permille` up to 1000, `prowess_permille` up to 4000 (`kinds_history.cpp:658`) and `loss_loser_permille` up to 1000. So dead can reach 4 × the side's population. 7.4 then caps it at T and `casualty_losses` takes every settlement to 0, while they stay standing.
- What follows is deterministic and handled. A later siege of a 0-population settlement has garrison 0, so the siege chance is 1000 when the attacker has force. The next T1 grows a 0 to 1 (P3-05 9.1's max(1, d)) and abandons it below `abandon_min`, with `civ_ended` if it was the last. Shipped tuning stays at 0.1 T, as the spec says.
- So this is no platform risk. But spec 8.4 item 3's "Casualties never empty a settlement" holds for the shipped numbers only, while P3-05 14 claims its invariants for every data set within the schemas.
- Fix asked (spec writer or engineer, low priority): say "with shipped tuning" in 8.4 item 3. Or, if the engine should guarantee it, cap each settlement's loss below its population, or add a named check `loss_loser_permille × max levy × max prowess < 10^9`.

**A2. A Q4 cession can leave a same-step colony of the loser behind in the ceded region.**
- `conflict_war.cpp:436-452`. In Q4 peoples runs first and may found a loser's colony inside the goal region G (P3-05 9.3 "inside"). That colony is invisible to conflict in this step. The cession transfers only the visible settlements (`visible_settlements(ctx, loser, goal)`, `:443`), as 8.3 item 4 says, and then sets G's owner to the winner (`:452`). The loser's new colony keeps standing in a region now held by the winner.
- Deterministic and as specified. Invariant 4 still holds (the winner has a settlement in G). But it is the one place where a region moves while its former owner keeps a standing settlement in it, against 8.4 item 5's summary "a region changes owner only by … a cession with its settlements". Rare: it needs a Q4 founding inside G by the loser in the step its war ends.
- Fix asked (spec writer, low priority): state the case in 8.3 item 4 or 8.4 item 5, or have peoples not found inside a region whose holder is at war. No code change needed for determinism.

### Notes

**N1. Relation bookkeeping (`engine.cpp:769-797`, `1089`, `1100`).**
- **Replay:** `open_relation` and `close_relation` append to `State::relation_ops_` in call order. The bookkeeping replays them:
  - an open adds a row with `since` = Y, or is fatal if the key was already opened since the last bookkeeping (`opened_now`, a `std::set<RelationKey>`);
  - a close sets `until` and `until_event` on the open row, or is fatal if none;
  - the list is then cleared;
  - finally, the recorded open keys must equal the state's open keys (`std::equal` over two `std::map`s in key order).
- **Order:** call order is the subsystems' fixed order (peoples opens none; conflict's passes are ordered, below), and the rows are `stable_sort`ed by their full primary key at the end (`engine.cpp:259`). The key is unique, because a second open of one key before a bookkeeping is fatal, and each bookkeeping has its own Y. So the sort is total.
- **Resume:** the list is in-memory state of one run. It is empty after every bookkeeping, and the last step of the run is T7's bookkeeping, so it is empty at the stage boundary. Nothing of it is read from or needed in the database. A resume restarts the stage from `--until chronology`, never mid-history.
- **No fatal path in P3-06:** conflict never opens one key twice before a bookkeeping.
  - `enmity` is opened at most once per pair ever (`enmity_opened_`).
  - `war` is opened only in T2, at most once per pair per T2 (`at_war` checks both directions before a declaration).
  - `peace` is opened only when the pair's one open war ends, and closed only by the pair's next declaration (a later T2, another bookkeeping) or by a conquest.

**N2. Symmetric keys.**
- `enmity` and `peace` go through `symmetric_relation` (lower id first, `conflict.cpp:84-87`) wherever they are built: opened in housekeeping (`:781`) and at a peace or truce (`conflict_war.cpp:428`), looked up and closed at a declaration (`conflict.cpp:1010-1013`).
- `war` is (attacker, defender) through `civ_relation`, at the declaration and at its end (`conflict.cpp:1014`, `conflict_war.cpp:427`).
- Closings in housekeeping and in a conquest use the stored keys.
- The members `grudge_` and `last_peace_` are keyed by `ordered(a, b)`, while `strain_` and `claims_` are keyed by the ordered pair (a, b) as directed. Each is used consistently.

**N3. Keys copied before closing.**
- Housekeeping collects `(key, ending_event)` while iterating `relations()` and closes after the loop (`conflict.cpp:721-739`). The smaller id's ending event is used when both sides ended, as 16.0 item 8 says.
- `close_relations_of` collects the keys first, then closes (`conflict_war.cpp:293-307`).
- Claims are erased with the erase-return idiom (`conflict.cpp:745-751`, `conflict_war.cpp:329-335`), never during another pass over `claims_`.
- `declarations` collects a civilization's candidates from `claims_` before `declare` drops claims (`:957-988`).
- `update_strain` iterates its local `fronts` map while inserting into `claims_` (`:866-910`).

**N4. Wars by index.**
- `wars_` grows only in `declare` (`conflict.cpp:1025`), called only from `declarations`, which runs only in T2. That includes the flare-up path (`:946-951`, `970-973`).
- `fight` (T6) and `quiet_ends` (Q4) take `n = wars_.size()` before their loops (`conflict_war.cpp:32`, `conflict.cpp:1031`). They copy the war (`const ConflictWar w0 = wars_[i]`, `w = wars_[i]`) before calling anything that appends events or changes state, and take `wars_[i]` by reference only in short blocks that call nothing.
- `conquer` sets `open = false` on other wars through a range-for over `wars_`, which never resizes it (`conflict_war.cpp:324-328`).

**N5. Draw keys are unique within their year.**
- `declare`: (a, b) — one draw per attacker per T2 (`conflict.cpp:986`).
- `battle` and `battle_win`: (w, 0) — each war's `battle` runs once per T6 (`conflict_war.cpp:56`, `97`).
- `siege`: (w, t) — at most one siege per battle and one battle per war per year (`:144`).
- `sack`: (w, t) — once per fall (`:185`).
- w is the stored `war_declared` event id. Conflict draws nothing in quiet steps.
- The five streams are registered after `hist.peoples.split` (51 names) and mapped by position (`conflict.cpp:96-99`, `490-492`).

**N6. `tension()` writes nothing** (`conflict.cpp:934-940`). It calls the `const` `best_claim` (`:915-932`), which reads `claims_`, `strain_`, `contact_` (set in this step's Q4), `wars_` and `last_peace_` and keeps the first maximal pair in ascending order. So the engine's skipped call at an interval's end changes nothing.
- The flare pair of a T2 is the same read, taken once before the first declaration (16.0 item 1).
- `last_t2_phase_` is set at the end of T2 (`:700`).

**N7. Arithmetic.** Every product goes through `checked_mul`, every sum of tuned values through `checked_add`, every division through `floor_div`.
- **Divisors on the legal range:**
  - `hunger_full` ≥ 1; 1000 − `fear_share` ≥ 1;
  - `declare_span` ≥ 1; the contact cost k ≥ 1 (`strain_rate` fatal below 1);
  - `max(1, need)`, `max(1, open_wars)`, `max(1, pop0)`;
  - `A + D'` and `A2 + G` guarded to 500 and 0;
  - `walls_permille` ≥ 1;
  - `non_islet_` regions guarded by `share_of`.

  No division by zero is reachable with legal tuning (the analogue of the P3-05 A1 does not exist here).
- **Bounds:** force and strain have explicit 2^40 fatal bounds (`conflict.cpp:601-603`, `858-861`). The largest product is D × def < 2^40 × 2^16.
- **Chances:** `chance_pm` clamps to [0, ONE] (`:47-51`); `sack` is `min(1000, …)`.
- **Grudges** are unbounded above but checked; a grudge near 2^62 would be a fatal error, not UB.
- **Widths and formatting:** no `long`, no float, no `size_t` in values, text through `format_i64`.

**N8. Iteration order and tie-breaks.**
- **Walks:** civilizations ascending (`alive_civs`); ordered pairs ascending (nested ascending loops, then `std::map<Pair, …>`); wars in declaration order; relations in `RelationKey` order; settlements and regions ascending.
- **Paths:** `reach_from` relaxes in rounds of at most `march_hops` borders with the key (cost, last region) (`conflict.cpp:272-304`).
- **Tie-breaks:** `best_target` (key, region); claim region (wanted, −value, region); the declaration's candidate (strain, smaller b by strict `>` in ascending order); siege target (population, smaller id); refugee destination (distance, smaller id); new capital (population, smaller id); casualty remainder (largest after losses, first).
- **The one sort:** the `stable_sort` of a declaration's two causes (`:1007`) sorts two distinct `int64` event ids (the claim and the last peace), a total order on the written field.

**N9. Visibility and the view.**
- In T2 and Q4, conflict reads its entities through `visible()`: presence, force, casualties, siege targets, the claim's second cause branch, the cession and the goal re-choice. Region ownership is read live. The refugee destinations and the capital move use all standing settlements (the stated exception).
- `pop0` uses `PeoplesView::population`, which includes same-step colonies (16.0 item 2).
- Conflict changes peoples' world only through `State`, `ruin` and `note_reduction`. Besides the view it reads `PeoplesGeo` (regions, islets, neighbours) and `PeoplesTuning` (`river_border_share_permille`, and `intensity` through `peoples_tuning_from`) directly (`conflict.cpp:493-514`, `conflict_war.cpp:76-86`). These are immutable for the run (the geo caches are pure, P3-05 audit N2), so this is no ordering or determinism hazard. The spec's 3.1 says "through `PeoplesView`" for numbers about peoples; these are geography and tuning, not peoples' state.

**N10. The nine points of the P3-05 audit, against the code.**
1. **Order:** `subsystems = {&peoples, &conflict}`, `tension = {&conflict}` (`stages/history.cpp`); `steps()` lists T2, T6, Q4 once each.
2. **`ruin(ctx, s, e)`:** called only in `fall`'s sack branch (`conflict_war.cpp:199`), reached only from `fight`, which runs only in T6. The settlement is checked standing and owned just before (`:176-178`), and `e` is the `sacked` event appended just before. Never in a quiet step.
3. **End what it empties:** after a sack or a capture, `fall` ends y with `civ_conquered` in the same step when no standing settlement is left (`:250-258`). A cession never takes the loser's capital region (`:413-414`), and the capital is a standing settlement.
4. **Ruin only in T6:** yes. So T4 never meets a ruined capital of an alive civilization.
5. **Regions, `note_reduction`, populations:**
   - regions move by capture, cession, `ruin`'s release, or the conquest freeing y's remaining regions (see A2 for the cession case);
   - `note_reduction` is called for every settlement of the side after casualties (`:357`);
   - every population change goes through `set_population` (`:190`, `214`, `290`, `355`, `447`).
6. **Streams:** own streams, keyed by ids and the year (N5).
7. **`wanted_held`:** once per alive civilization in each strain update's snapshot (`conflict.cpp:822`). `share` is never called. `population` twice per declaration.
8. **Ids:** members hold ids. Wars are copied by value or indexed. Local `const Civilization&` references (housekeeping `:729-730`, `alive_visible`) are used where nothing is founded.
9. **`homes()`:** never called.

**N11. The engine's new readers** `event_kind` and `event_participant` (`engine.cpp`) are read-only lookups into the stored log, fatal for an id outside it. They return the kind key from the catalogue and the first participant of the role. Housekeeping uses them once per newly seen civilization (`conflict.cpp:758-763`).

## 2. What later stages (P5, the history report) should know

- The relation pass records every call. A future subsystem that closes and reopens a key in one step gets two rows. One that opens a key twice before a bookkeeping is fatal (`history: state: relation '<kind>' of … opened twice before one bookkeeping`).
- A war whose quiet-step end finds no budget stays open into the next active phase (7.8). So "a war is never carried across a quiet interval" holds only when budgets allow. It is deterministic, and reports should not assume the stricter form.

## 3. Lint

Both commands on `a121c72`: `--self-test` passed (17 snippet files); the default scan reported 0 findings in 161 C++ and 13 CMake files.
- `history-live-iteration` is clean. The two range-for loops over `relations()` (`conflict.cpp:722`, `conflict_war.cpp:296`) only collect keys.
- `history-held-reference` is clean. Conflict founds nothing, and its local `Civilization&` references are never followed by a founding.

No form used here escapes the rules. The forms that would — a loop over `relations()` that calls a helper which closes (`close_relations_of`, `conquer`), or a reference into the member `wars_` kept across `declare`'s `push_back` — are not used: the code collects keys first and copies wars by value. A rule keyed on those helper names would be ad hoc, so I left the lint unchanged.

## 4. What was run

All in a worktree of `origin/p3/06-int` at `a121c72` (branch `p3/06-audit`), build tree outside the source tree, `linux-gcc-release` only, `-j 3`.

| Run | Result |
| --- | --- |
| Lint `--self-test` and default scan | passed (17 files); 0 findings (161 C++, 13 CMake files) |
| `generate --seed 42` (1024, default settings), `--threads 1` and `--threads 5` | byte-identical (SHA-256 `1807d6ca71a65a70…`) |
| A stress world: `--seed 7 --map-size 512 --event-intensity turbulent --civs-per-race 4 --history-detail dense`, `--threads 1` and `--threads 4` (12 flare-ups, 106 wars, 376 + 32 battles, 222 sieges, 32 sacks, 15 captures, 2 cessions, 5 conquests, 226 relation rows) | byte-identical (`4be1f0d049b4eb57…`) |
| `hash --detail` of the seed 42 world against `hist-conflict-42-1024` in `worldgen/tests/golden/p3-06-conflict.txt` (the independent Python reference) | all 13 history table lines equal |
| `worldgen-tests -sf='*history_conflict_*,*conflict_engineer_test*'` (unit, property, data, golden 512 with resume, threads, wars and sentences) | 20 of 20 pass, 421,114 assertions |
| `worldgen-tests -sf='*history_engine_*,*engine_hardening*,*history_peoples_*'` (P3-04 and P3-05 tests at 0.16.0, R1, R2, Y2, Y3, Y4) | 73 of 73 pass, 154,901 assertions |
| `worldgen-tests -sf='*history_engine_golden*' -tc='*R1 and Y2 on the 1024*' -ts=history-slow --no-skip` (resume equality on the 1024 world cases, with conflict installed) | 1 of 1 passes |
| `worldgen-tests -sf='*history_conflict_golden*' -ts=history-slow --no-skip` | 2 of 3 pass; **1 fails**, see below |

**The failing slow case is a test defect, not a determinism finding** (for the test writer and the validation specialist; the merge gate needs it green):
- `P3-06 16.3 item 19, 16.2 item 16: the 1024 golden worlds and the hard bounds of 15.5` throws `history: resume: meta.stages is 'elevation,…,chronology,history', expected the stages up to chronology` on its first case, `geo-seed-1-1024`. It reproduces when run alone.
- Cause: `check_hard_bounds` (`worldgen/tests/history/history_conflict_golden_test.cpp:120`) calls `eng::inputs_from_world(r, shipped())` on the **complete** world (`world_path(g)`). `history_inputs_from_world` correctly refuses any world that already ran `history` (P3-04 3.2).
- The case's own resume check in `check_world_case`, which uses the `--until chronology` world, passed for `geo-seed-1-1024` before the throw. P3-04's R1/Y2 slow case passes on the 1024 worlds.
- Because of the throw, the hard bounds of 15.5 and the world checks of the three other 1024 cases (`hist-races-elf-orc-1024`, `hist-dense-1024`, `hist-conflict-42-1024`) are not exercised by this case.
- Fix: read the inputs of the chronology world there (`world_path(g, 1, "chronology")`), or read the regions directly.

Not run: `linux-clang-asan` and any second compiler (instructions). The cross-compiler `world-hashes.sh` comparison is the validation specialist's; nothing found here predicts a difference.
