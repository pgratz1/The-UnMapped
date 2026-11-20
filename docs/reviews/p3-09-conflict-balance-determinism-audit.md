# Determinism audit: P3-09 conflict balance (generator 0.18.0, schema 7)

Auditor: determinism auditor, 2026-11-20. Change audited: `origin/p3/09-int` at `5cc5e30` (the engineer's `p3/09-impl` merged with the test writer's tests), against `origin/main` `7763484` (generator 0.17.0). The three-dot diff picks an old merge base (`35b2431`, before P3-08 merged), so the code under audit is the two-dot diff `git diff origin/main origin/p3/09-int -- worldgen/src worldgen/include tools/worldgen-cli game/data`: `origin/main`'s commits after that base touch only docs and golden files, so this diff is exactly P3-09. Change spec `docs/specs/p3-09-conflict-balance.md` r5; normative text P3-06 20.1 to 20.26, P3-05 21, P3-01 3.2 and 3.3, P3-07 21, from `origin/p3/09-spec` `83e19a0`, merged into the audit branch `p3/09-audit` (merge `1d2918a`; the one conflict, two entries of `docs/design/proposals.md`, resolved to the spec branch's newer text; the merge brings no code: the audit branch's `worldgen/`, `tools/worldgen-cli/` and `game/` sources equal `5cc5e30`'s). Line numbers are those of `5cc5e30`.

Scope (non-test code):
- `worldgen/src/history/conflict.cpp`, `conflict_war.cpp`, `conflict_internal.hpp`, `worldgen/include/worldgen/history/conflict.hpp`: presence with hidden settlements, concealment, war goals and weariness (`futile_`, open wars), revenge (`wronged_`), muster and local levies, `declare_odds` and `odds_of` in `best_claim` and the declarations, the directional truce, `enmity_hops`, the search (`hist.conflict.search`, `unfound`, `abandon_search`), the war score from dead and people taken, `last_battle_winner`, `close_war`, `war_won`, the kill bound `loser_dead_of`, the flight of a last refuge (`flight_plan`, `flee`, `refuge_held`), the cost of a lost war (`cost_plan`, `cede`, tribute), the casualty floor of a race's last refuge.
- `worldgen/src/history/peoples.cpp`, `peoples_geo.cpp`, `worldgen/include/worldgen/history/peoples.hpp`: `race_last_refuge` in `abandon_small` and `strike_result`, the seams `site`, `founding_kind`, `hops`, `biome_term`, `settlement_name`, `region_biome_term` (factored out of `region_score`), the automatic civilizations-per-race rule (`regions_per_extra_civ`, `civs_extra_max`, `non_islet_count`).
- `worldgen/src/history/event_catalogue.cpp`: kinds 32 to 36.
- `worldgen/src/history/digest.cpp`, `worldgen/include/worldgen/history/digest.hpp`: `history_note`, `history_ordinal`.
- `worldgen/src/data/kinds_history.cpp`: fourteen `history.conflict` fields, two `history.peoples` fields, three race fields, the check `conflict_enmity_hops`.
- `worldgen/include/worldgen/core/streams.hpp`: `hist.conflict.search` (53 names); `worldgen/include/worldgen/version.hpp`: 0.18.0.
- `game/data`: `history/conflict.toml`, `peoples.toml`, `report.toml`, the six race files, `name_filters/words.toml` (data; read through the existing loader).

Read for context: `conflict_war.cpp` and `conflict.cpp` at 0.17.0 (`origin/main`) for the order of the draws; `engine.cpp` `StepContext::draw` and `visible`; `peoples.cpp` `ruin`, `abandon`, `strike`, `planned_plague_dead`, `SettlementsNow`, `other_standing`, `nearest_standing`, `next_name`; `peoples_geo.cpp` `PeoplesGeo::hops` and its construction, `PeoplesGeo::site`.

**Verdict: pass with notes.** No blocking and no should-fix finding. Every new rule is integer arithmetic on `std::int64_t` through the checked helpers, with `floor_div` for every "div" of the spec; no new `long`, plain `int`, floating point, locale or `<cctype>`. Every new container that decides an outcome is an ordered map or set keyed by integers (`hidden_until_`, `futile_`, `wronged_`), a `std::array` per side (`taken_people`), or a vector built in ascending id and sorted on fully determined integer keys. The only new draw is the search, keyed (year, war, region, 0) on its own stream; no existing draw's key changed, and the battle draw is taken under exactly the conditions of 0.17.0 except where every one of the defender's settlements in the battlefield is concealed. The flight founds inside the loop over the subsystem's own war list by index, never inside a loop over `State`'s lists, and no reference into `State` is held across the founding. Same-seed worlds are byte-identical at `--threads` 1, 2 and 6 and between GCC and Clang, on eight worlds that between them have a flight, held refuges, abandoned searches, tributes and wars won (section 4). One lint extension (founding proxies, section 3), no output change. Two advisories, notes.

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. `flight_plan` holds a reference into the lazily filled hop cache across further fills.** `worldgen/src/history/conflict_war.cpp:192` takes `const std::vector<std::int64_t>& from_t = view.hops(from)` and then, inside the loop at line 206, calls `view.hops(r)` for every region of the attacker's presence, which may compute and store another entry of `PeoplesGeo::hops_` (`peoples_geo.cpp:625-645`, a `mutable std::vector<std::vector<std::int64_t>>`). Today this is safe: the outer vector is sized once in the constructor (`peoples_geo.cpp:572`, `hops_.assign(nr, {})`) and a fill only assigns the inner vector of its own region, so `from_t` is never moved or freed. It would become a dangling reference (whose effect depends on the allocator, so it could differ between standard libraries) if the cache were ever changed to grow lazily, to be cleared, or to be a map that rehashes. The same pattern, without a second fill, is in `local_force` (`conflict.cpp:745`). Fix (no output change): copy the row (`const std::vector<std::int64_t> from_t = view.hops(from);`), or state in `peoples.hpp` next to `hops_` that its outer size is fixed at construction and references stay valid for the subsystem's life. Not required for this merge.

**A2. Two implementations of "a race's last refuge".** `ConflictSubsystem::last_refuge_of` (`conflict.cpp:700-714`) and `PeoplesSubsystem::race_last_refuge` (`peoples.cpp:625-638`) state the same predicate (one standing settlement, no other civilization of the race that has not ended) in two ways: conflict counts standing settlements on the live state; peoples asks `other_standing` through `SettlementsNow`, so that the plague plan's copy (`up` mask) is used when there is one. They agree today: both count every standing settlement, visible or not, and both read `ended_year` from the live state, and within one plague plan only the struck civilization's settlements change, so the other civilizations' `ended_year` cannot differ between the plan and the strikes (checked in section 2.6). This is not cross-platform: the same code runs everywhere. It is fragile: a change to one predicate (for instance visibility, or a plan that spans civilizations) would split the casualty floor (20.20 item 1) from the abandonment rule (P3-05 21 item 6), and invariant 20.13 item 2 would break on all platforms alike. Fix (no output change): one shared helper, or a comment at each pointing to the other.

### Notes

**N1. The battlefield is chosen before the battle draw.** At 0.17.0 `battle` took the battle draw first and chose the target after (`origin/main` `conflict_war.cpp:56`). Now the target (6.2) is chosen first (`conflict_war.cpp:59-73`), the search follows, then the battle draw (line 91). The target choice is a pure read (presence, fronts, `best_target`), and keyed draws are pure functions of (stream, year, a, b, k) (`engine.cpp:1168-1170`): moving a pure computation ahead of a draw changes no draw. The battle draw keeps its key (year, war, 0, 0) and is skipped only when the search fails, which requires every standing visible not-hidden settlement of the attacked side in the battlefield to be concealed (`conflict_war.cpp:75-90`); with `conceal_min` 1001 for every race but the faerie, a world without faerie settlements in forest takes exactly 0.17.0's battle, battle-win, siege and sack draws up to the first point where another P3-09 rule changes the state. The pass test `at_pass = pass_id != 0` (line 96) is equivalent to 0.17.0's `terms.pass` (the pass id is nonzero exactly when `terms.pass`, `conflict.cpp:891-892`).

**N2. The search draw.** `ctx.draw(keys_[SEARCH], word(w0.declared), word(region), 0)` (`conflict_war.cpp:80`): stream `hist.conflict.search`, registered last (`streams.hpp`; the registry feeds only a test, no world byte), key (year, war, region, 0) as 20.17 item 5. `battle` runs at most once per open war per T6 and T6 runs once per active year, so each key is used at most once; it does not depend on which side has the initiative or on the order of the war list. It is a single draw in its own full expression (the `two-draws` rule is clean).

**N3. `best_claim` with odds is a pure read.** `best_claim` (`conflict.cpp:1196-1215`) now calls `odds_of` (lines 908-918), which reads the claim, `live_front` (presence and `reach_from`), `defence_at`, `force` and `effective_aggression`. None of them writes a member or the state; the only side effect is the hop cache of A1, which changes no value. `odds_of` is evaluated only when the claim's value would beat the best so far (`v > best_value && ...`), which selects the same claim as evaluating it for every claim (the first, in ascending (a, b), of the claims with the largest value among those whose odds reach `odds_min`). `tension()` stays a pure read. `force` can still fail with the 2^40 bound in a tension read; that is a deterministic `FatalError`, unreachable with legal data (populations are far below it even with muster 4000).

**N4. P3-04 6.9 for the flight.** The call chain is `run` (T6) → `fight` (`conflict_war.cpp:34-45`, `for i < n` over `wars_` by index, `n` taken before the loop) → `battle` → `siege` → `flee` → `State::found_settlement` (line 261). No loop over `civs()`, `settlements()` or `relations()` is open on that path. In `flee` every read of a settlement goes through `state.settlement(id)` by id; after the founding it reads `state.settlement(t).population` afresh (line 267) and `ruin` fetches its own reference (`peoples.cpp:546`). The references held across the founding are to `State` itself, to `races_` (`siege`'s `ry`) and to the plan (a copy in `siege`'s `std::optional<Flight>`), none into `State`'s vectors. `wars_` is only appended in `declare` (T2); in T6 and Q4 a war is closed by flags (`close_war` line 579, `conquer` line 437-441, housekeeping), never erased, so the indices of `fight` and `quiet_ends` (`conflict.cpp:1325-1332`) stay valid when a war ends, a search is abandoned (`Fought::ended` skips `end_checks`, line 41) or a civilization is conquered mid-loop. The settlement id `h = settlement_count + 1` is checked against `found_settlement`'s result (line 261). The name is drawn before the founding through `next_name` with entity `h` (`peoples.cpp:186-191`), a keyed draw; the registry is filled in program order. The lint's two history rules did not see this chain before (a helper that founds was invisible to them); the extension of section 3 now follows it within a file, and reports nothing on the code.

**N5. Containers and orders.** `hidden_until_` (`std::map<int64, int64>`), `futile_` (`std::map<Pair, Futile>`), `wronged_` (`std::map<Pair, std::set<int64>>`): ordered, integer keys; `wronged` returns its set ascending. `taken_people`, `dead`, `won`, `taken`: `std::array<int64, 2>` by side. Flight candidates: a vector of (−h, −σ, region) built in ascending region and `stable_sort`ed with `std::tuple`'s `operator<` (`conflict_war.cpp:196-213`): a strict weak order whose last component, the region id, is unique, so the order is total. Cession candidates (`cost_plan`, lines 621-646): the front's entries in ascending region, then `best_target`'s smallest (−value, region). Claim goals with revenge: the smallest tuple (wronged-held, wanted-held, −value, region) over `allowed_goals` (`conflict.cpp:1157-1171`), region last. Weariness counts open wars by a loop over `wars_` (vector) and a map lookup (`conflict.cpp:844-853`), order-free. `declare`'s causes are sorted integers. No unordered container, no pointer key, no `std::sort` (the `unstable-sort` rule is clean).

**N6. Arithmetic and divisions.** Every new product goes through `checked_mul`/`checked_add` (a deterministic `FatalError` on overflow on every platform) and every division through `floor_div`. Bounds on legal data: force = levy × prowess (≤ 4000) div 1000 × muster (≤ 10000) div 1000, checked against 2^40 (`conflict.cpp:737-741`, `755-758`); `declare_odds` multiplies a force (≤ 2^40) by a defence (at most 1000 + 4 × 10000 for terrain, pass, river and fort + 10000 home defence = 51000) and then by `declare_ratio` (≤ 10^4) after a division by 1000, at most about 5.6 × 10^17; garrison × (1000 + home ≤ 11000); force × `kill_max_permille` (≤ 10^4); population × `tribute_permille`; dead × 1000 and pop0 × muster in the exhaustion test (`conflict_war.cpp:486-489`): all far below 2^63. Divisors: `max(1, need)` in the odds, `1000 − truce_break_aggression` ≥ 1 (range 0 to 999), `max(1, open_wars_of)` in the local force, `max(1, base)` in the exhaustion test, `regions_per_extra_civ` ≥ 1 (schema range 1 to 100000; the struct default is 1), and the sums guarded by `> 0` in `battle_permille_of` and `siege_permille_of`. An empty local force gives `p` = 0 or 500 by those guards, not a division by zero. The biome term can be negative: `region_biome_term` keeps `floor_div` (floor, as P3-05 4.2's div; unchanged from `region_score`), and every use clamps with `max(0, B)`. The casualty floor `max(0, min(dead, total − abandon_min))` (`conflict_war.cpp:463-465`) is never negative. `ctx.year() − w.start`, `+= 1` counters: `int64`, small. `history_ordinal` uses `%` on positive values only (C++ truncation is defined since C++11 in any case). Size conversions are explicit `static_cast<std::size_t>` of positive ids; nothing new depends on the width of `size_t` or `long` (MSVC).

**N7. Plague plan and the casualty floor.** `strike_result` (`peoples.cpp:675-692`) is the one decision the plague plan (`planned_plague_dead`, lines 965-1029) and the strikes share (P3-08 audit A1); P3-09 adds `!race_last_refuge(now, settlement)` to it, with `now` the plan's copy in the plan and the live state in `strike`. `race_last_refuge` reads `other_standing(now, …)` (the plan's `up` mask when there is one) and the live `ended_year` of the race's other civilizations. A plague plan concerns one civilization's settlements only, so the other civilizations' `ended_year` is the same during the plan and during the strikes; the struck civilization can end only when its last settlement is abandoned, after which no strike of the outbreak remains. So plan and strikes decide the same abandonments (invariant 13 of P3-05 holds). The rule also short-circuits the `abandon` draw (`&&`), which is the spec's "no draw is made" and is the same on every compiler (the order of `&&` is defined). `abandon_small` (line 494) reads the live state, the same as before. The conflict casualty floor (`conflict_war.cpp:463`) runs in T6, outside any plan.

**N8. Cost of a lost war.** `cost_plan` (`conflict_war.cpp:621-646`) is `const` and is computed in `end_war` before the quiet-budget test, so a war that waits leaves no trace; between the plan and `cede` (after `close_war` and `note_futile`, which change no owner or population) nothing changes the front, the owners or the populations, so the applied cost is the planned one. Tribute is taken by `casualties` in ascending settlement id with the 7.4 remainder rule, as battle losses are.

**N9. Data and loader.** The new fields are integers read through `Record::get_int`; the race fields reach the content hash in schema order (they change every world's `content_hash`, as 20.12 states). The `conflict_enmity_hops` check runs after the two existing named checks in `history_conflict_schema` and in `check_conflict_tuning`, with the same message text. The shortened word list (`words.toml`) is read by the existing `name_filter` kind (P3-08 audit), which does not depend on file order.

**N10. `digest.cpp` is not in the output path yet.** `history_note` and `history_ordinal` are used only by tests at `5cc5e30`; nothing in `report`, `hash` or the database calls them. Their text is built from stored rows in id order with `db::format_i64`, ASCII comparisons only (`std::string::compare`), so when P3-07 calls them they will not depend on the platform.

## 2. What was checked

### 2.1 Constructs (by reading every changed line and by the lint)
Floating point, `<cmath>`, `<random>`, `std::rand`, wall clock, environment, locale and `<cctype>`, unordered containers, `std::hash`, pointer ordering, threads and shared state (the history stage is single-threaded: no thread, atomic or parallel call under `worldgen/src/history/`), `long`, `wchar_t`, `__int128`, `std::sort`, `std::abs` on integers, `std::vector<bool>`, two draws in one expression, the sequential generator in history code, range-for loops over `State`'s lists that found, open or close, references into `State` held across a founding: none in the change.

### 2.2 Draws
Every draw call in `worldgen/src/history/` listed and compared with 0.17.0: the five conflict draws (declare, battle, battle-win, siege, sack) keep their keys; the search is the only new one (N2). The declaration draw is now skipped below `odds_min` (20.6 item 3: "no declaration and no draw"); the sack draw is not taken for a flight (20.1 item 1); the abandonment draw is not taken for a race's last refuge (P3-05 21 item 6). Each of these is a rule of the spec and the same on every platform. Peoples' draws (home, civs, abandon, found, outward, plague, famine, crisis, split, names) keep their keys.

### 2.3 Live lists and held references (P3-04 6.9)
N4. Also `update_strain` (claims in ascending (a, b) over a local `std::map` of fronts, appends events and inserts claims, iterates neither), `declarations` (collects candidates from `claims_` before `declare` erases from it), `housekeeping` and `close_relations_of` (relations collected, then closed), `conquer` and `quiet_ends` (flags only on `wars_`).

### 2.4 Uninitialized data
New struct members have default initializers (`ConflictWar::goal_owner`, `taken_people{}`, `last_battle_winner`, `unfound`; `DefenceTerms::home_defense`, `biome_term`; `Futile`, `Cost`); `Flight` is built by aggregate initialization; `target{}` and `best_key{}` are value-initialized. No struct is hashed or serialized as bytes.

### 2.5 Database
Kinds 32 to 36 are appended to the compiled catalogue in id order and written with the existing writer (rows in id order); no schema change (`schema_version` 7). No timestamp, no setting.

### 2.6 The two last-refuge predicates
A2 and N7: read side by side, with the plague plan.

## 3. Lint

Both commands pass on the audit branch: `python3 -B tools/lint/determinism_lint.py --self-test` (18 snippet files, 0 failures) and `python3 -B tools/lint/determinism_lint.py` (164 C++ files, 13 CMake files, 0 findings).

**Extension: founding proxies** (`tools/lint/determinism_lint.py`, `tools/lint/README.md`, new self-test `tools/lint/selftest/bad_history_proxy.cpp`). Until now `history-live-iteration` and `history-held-reference` saw only a literal `found_civilization(` or `found_settlement(` call; the README stated that a helper that founds was not seen. P3-09 is the first change that founds deep inside a call chain of another subsystem (`fight` → `battle` → `siege` → `flee`). The lint now finds, per file, the functions whose body founds, directly or through another such function of the same file (a fixed point over a call graph of the file's function definitions), and treats a call of one of them as a founding: a `Settlement&` (or the other held forms) used after such a call in the same block, and a range-for over `civs()` or `settlements()` whose body calls one, are reported. A loop over `relations()` that calls a founding proxy is not (a founding does not change the relation map). On the code it detects `battle`, `fight`, `flee`, `siege` in `conflict_war.cpp` and `awaken`, `crisis`, `foundings`, `pressures`, `run` in `peoples.cpp` (and `awaken` in `stubs.cpp`), and reports nothing. A helper defined in another file is still not seen (stated in the README). The self-test was checked to fail without the extension (its three expected findings disappear). Tooling only: no build input, no output byte.

## 4. Worlds

`linux-gcc-release` and `linux-clang-release`, built from the audit branch (sources equal to `5cc5e30`), `--data` the branch's `game/data`, each case at `--threads` 1, 2 and 6, SHA-256 of the world file:

| Case | Arguments | SHA-256 (GCC and Clang, threads 1, 2, 6) |
| --- | --- | --- |
| s2-1024 | `--seed 2 --map-size 1024` | `19d3afe0c192d99d60d57038b8960a208da55d54ed3fd4f23d7709b20014cfe8` |
| s4-1024 | `--seed 4 --map-size 1024` | `8e09c9bb8fbbc239b09377acee084bc84376063f3eedcc075a2b282b7da5bce4` |
| s6-1024 | `--seed 6 --map-size 1024` | `cc152486b60c90603ba29029e34f705a1f0a4dd2e2b3ba203552ab5daa1f10e8` |
| s1-512 | `--seed 1 --map-size 512` | `5082ffa7ac3435b40e199ad5dde978f2b383947c28c68b74b7a15ffc507165aa` |
| s18-512 | `--seed 18 --map-size 512` | `534acd960204a1d12d80a5c9114be7d99e2f5c3cbd0c62a056f7005b86103c62` |
| s42-1024-c4 | `--seed 42 --map-size 1024 --civs-per-race 4` | `5a03e6a86deeae20c42eba606608e897a27d914f5ce298b77e2c1467a5c5f622` |
| s7-1024-turb | `--seed 7 --map-size 1024 --event-intensity turbulent` | `88fb50a9f0fbdf0c70734905d5b992eac1f9bda76ebda9c52d67d412b65baa36` |
| s3-2048 | `--seed 3 --map-size 2048` | `a24cafa7b16f2739fb6f26ad34740f078dcf1fab0ef9fc6930eff2ad472c810e` |

Every case gives one hash across the six runs (two compilers × three thread counts). The new rules are exercised (counts of the new kinds in the `--threads 1` GCC worlds):

| Case | Events | `refugees_settled` (32) | `tribute` (33) | `war_won` (34) | `search_abandoned` (35) | `refuge_held` (36) |
| --- | --- | --- | --- | --- | --- | --- |
| s2-1024 | 744 | 0 | 7 | 4 | 6 | 0 |
| s4-1024 | 820 | 0 | 5 | 8 | 9 | 0 |
| s6-1024 | 716 | 0 | 5 | 4 | 12 | 0 |
| s1-512 | 589 | 1 | 3 | 5 | 0 | 9 |
| s18-512 | 527 | 0 | 3 | 3 | 2 | 11 |
| s42-1024-c4 | 2414 | 0 | 27 | 33 | 8 | 0 |
| s7-1024-turb | 1715 | 0 | 12 | 15 | 3 | 0 |
| s3-2048 | 2444 | 0 | 5 | 17 | 8 | 0 |

At this build the 1024 seeds 2, 4 and 6 have no flight (the lead's list of seeds with flights was likely made on an earlier build); the flight, with its founding inside T6, is exercised by s1-512, and held refuges by both 512 cases. Every other new rule (searches and abandoned searches, tribute, wars won) occurs in every case.

No C++ hardening was committed, so no before-and-after comparison is needed; the lint change builds nothing.

## 5. Verdict

**Pass with notes.** No blocking finding. Advisories A1 (copy the hop row in `flight_plan`, or document the cache's fixed outer size) and A2 (one shared last-refuge predicate) are for the engineer at a convenient point; neither changes output, and neither is needed for this merge.
