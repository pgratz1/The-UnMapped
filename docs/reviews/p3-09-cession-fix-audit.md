# P3-09 cession fix: determinism audit

- Change: `origin/p3/09-fix-impl` 901ec5e, base `origin/p3/09-specfix` 623e98c (spec P3-06 section 20.27).
- Files: `worldgen/src/history/conflict.cpp`, `conflict_internal.hpp`, `conflict_war.cpp`, `worldgen/tests/history/conflict_engineer_test.cpp`.
- Scope: this change only. The full P3-09 audit is on `origin/p3/09-validation` and is not repeated here.
- Auditor: determinism-auditor, 2026-11-20.

**Verdict: pass with notes.** I found no blocking findings.

## What was checked

1. **Iteration order of `standing_settlements_in`** (`conflict.cpp` 71-81). The function loops over integer ids 1 to `settlement_count(state)`, so its output is in ascending id order. It touches no unordered container and does not depend on pointers. The filter is owner, region and `standing()`. It applies no visibility test, as 20.27 item 1 requires.
2. **P3-04 6.9 / P3-06 8.3 item 1** (`conflict_war.cpp` 608-618). `cede` takes the id list into a local `std::vector` before it changes anything. Each pass of the loop re-fetches `state.settlement(id)` and copies `population` by value before calling `set_population`, `refugees` or `set_settlement_owner`. No reference to a settlement lives across a state change. Neither `cede` nor `refugees` founds a settlement, so the list cannot grow during the loop. The `const Settlement&` inside the helper lives only within one iteration of a read-only pass.
3. **Random draws.** The changed code makes none, sequential or keyed.
4. **Arithmetic.** Integer only: `checked_mul`, `checked_add` and `std::max<std::int64_t>`. The refugee distance is a squared integer distance. No floating point and no `<cmath>`.
5. **Excess refugees when the nearest other settlement is in G and not yet transferred.** This is well defined. `refugees` (`conflict_war.cpp` 374-403) reads live ownership: a settlement of L in G with a higher id has not been transferred yet, so it is still L's and can receive the refugees. The amount is capped by its room, `2 x C(region, r_L) - p`. When the loop reaches that settlement, its re-fetched population includes the arrivals, and anything above `2 x C(G, r_W)` is sent on again. Settlements already transferred belong to W and are excluded. Each settlement is processed once and `refugees` does not recurse, so the loop terminates. Ties between equal distances go to the lower id (strict `<` while ids ascend). The updated independent reference does the same: `origin/p3/09-fix-tests`, `p3_conflict_reference.py`, the `cede_all` branch with `refugees` using `min` keyed by `(d, id)` over the live `standing_of`. The engineer test that plants a colony at the coordinates of the visible settlement exercises exactly this path.
6. **Spec 20.27 item 2: the other ownership tests.** I checked every `set_region_owner` and `set_settlement_owner` call in the conflict code, plus `PeoplesSubsystem::ruin`:
   - Capture's conquest of the region (`conflict_war.cpp` 328-345): a full `State` scan of standing settlements of y, with no `visible()` test.
   - Conquest when emptied (`conflict_war.cpp` 364-371): a full scan.
   - `ruin`'s release and capital move (`peoples.cpp` 568-583): a full scan.
   - Flight (`conflict_war.cpp` 264): sets the owner of a free region to the refuge's founder. It is not a test of standing settlements.
   - `conquer` (`conflict_war.cpp` 433): releases every region of the ended civilization.

   None of these paths uses `visible_settlements`. They already use the full reading, so the claim holds by construction and the visible-only gap cannot occur there. The remaining callers of `visible_settlements` (`force`, `local_force`, the fortress term of the defence at `conflict.cpp` 910, `seen_settlements`, the goal test of `declare` at 1301, and `casualties`) choose, measure or kill. None of them transfers ownership, and 20.27 item 3 keeps them visible-only.
7. **Lint**, run from a clean worktree of 901ec5e: `--self-test` passed (18 snippet files), and the full scan reported 0 findings (164 C++ files, 13 CMake files). The change needs no new lint rule. "Visible-only reading in an ownership path" is a semantic distinction that a pattern rule cannot tell apart from the legitimate visible-only callers.
8. **Tests** (`conflict_engineer_test.cpp`). These run only in tests, not in the output path. They use no unordered iteration, floating point or wall-clock time that could reach a recorded value.

## Notes (advisory)

- **A1.** The fix changes world bytes for any world with a same-step colony of the loser in a ceded region, including at least 2048 seed 3. The goldens and world hashes must be re-recorded and the cross-preset comparison re-run. This step belongs to validation (20.27 item 9). The independent reference on `p3/09-fix-tests` has to be merged with this change for `p3-06-conflict.txt` to agree.
- **A2.** The destination rule of item 5 is the literal reading of P3-05 8.5 item 2 (live owner at the moment of the move). 20.27 does not state it explicitly for an untransferred settlement in G. The C++ and the reference agree, so the output is defined, but a sentence in the spec would close the reading.
