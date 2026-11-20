# Determinism audit: P3-08 names and peoples follow-ups (generator 0.17.0, schema 7)

Auditor: determinism auditor, 2026-11-20. Change audited: `git diff origin/main...origin/p3/08-int` at the tip `d2fce98` (the engineer's `p3/08-impl` merged with the test writer's tests), non-test code under `worldgen/src`, `worldgen/include` and `tools/worldgen-cli`. Change spec: `docs/specs/p3-08-followups.md` r1, with the stage-spec sections it names (P1-06 r3 to r5 RG11, P3-02 r5 to r7 section 11, P3-04 r10/r11 6.6, 8.4, 8.5, P3-05 r6/r7 6.2, 8.4, 8.5, 9.1, 10.2, 10.3, P3-06 r7 7.3). Line numbers are those of `d2fce98`.

Scope:
- `worldgen/src/data/kinds_names.cpp` (new), `kinds.cpp`, `kinds_history.cpp`, `worldgen/include/worldgen/data/schema.hpp`: kinds `placename` and `name_filter`, the `worldgen.regions` records `name_pool_weights` and `name_traits`, race field `cell_weights`, culture field `god_patterns`, check `naming_realm_forms`, two `peoples` fields.
- `worldgen/src/stages/regions.cpp`, `worldgen/include/worldgen/stages/regions.hpp`: RG11 r3.
- `worldgen/include/worldgen/core/streams.hpp`: stream `geo.regions.name` (52 names).
- `worldgen/src/history/names.cpp`, `names.hpp`: the name filter, `god_patterns`, `names_form_text`; `chronology.cpp`, `history.cpp` and `stubs.cpp` (callers).
- `worldgen/src/history/event_catalogue.cpp`: kind 31, the three-word value form, the "the" rule.
- `worldgen/src/history/engine.cpp`: quiet eras not closed by charged events.
- `worldgen/src/history/peoples.cpp`, `peoples_geo.cpp`, `peoples.hpp`, `inputs.cpp`, `inputs.hpp`: the cell term, fortress cells, three new input layers, the last-settlement rule, decline start and abandonment causes, destination and kind 31, the cap drain, the plague plan, the crisis split plan.
- `worldgen/src/history/conflict_war.cpp`: battle losses from the smaller force.
- `tools/worldgen-cli/cli.cpp`: help text only.

Read for context: `peoples.cpp` `ruin` and `strike` and the T4 spread loop (to compare with the plague plan), `engine.cpp` `StepContext::draw`, `data/content.cpp` (directory listing and definition order), `data/text.hpp`.

**Verdict: pass with notes.** No blocking and no should-fix finding. All new arithmetic is `std::int64_t` (or `std::uint64_t` for keys and sizes) with bounds far below overflow on the legal data ranges; nothing new uses `long`, `int` arithmetic, floating point, `<cctype>` or a locale. Every lower-casing and letter test is an explicit ASCII range. The two new data directories are read through the existing loader, which never uses the listing order, and both kinds reach the content hash in registry order, definitions in ascending id, fields in declaration order. Every new container is ordered by integers or by strings (byte order). The one new sort sorts distinct integers. The plague plan and the split plan are `const`, change no state, and use only keyed draws, which are pure functions of their key and the year, so planning first and striking after draws the same values as before and consumes nothing. Same-seed worlds are byte-identical at `--threads 1`, 2 and 6 (section 4). One lint rule added (`ctype`, section 3), no output change. Two advisories, eight notes.

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** breaks or leaves open a stated contract; **advisory** is fragile but currently safe; a **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. The plague plan repeats the rules of `strike` and `ruin` instead of calling them.**
- `peoples.cpp:923-1017` (`planned_plague_dead`) against `strike` (`peoples.cpp:878-912`), `frail_abandon` (`914-921`), `ruin` step 2 (`peoples.cpp:554-573`) and the spread loop (`peoples.cpp:1071-1094`).
- Today the replay matches the real strikes term by term. Same candidate set: the owner's standing, visible settlements in the origin's region and its neighbours, not yet struck, in ascending id. Same keyed draws: `PLAGUE_SPREAD (origin, u, 0)`, `PLAGUE_TOLL (u, 1, 0)`, `ABANDON (u, 0, 0)`, all with the same year. The same abandonment test, with `others_up` standing in for `has_other_standing`. The same nearest settlement (squared distance, then the smaller id). The same room, 2 × capacity − P. It also reads the status and kind that the real loop would see: within the loop nothing changes a later target's status or kind, only populations, and the plan tracks those.
- This is not a cross-platform risk: both sides are pure integer functions of the same state, so a mismatch would be the same everywhere. But the logic is duplicated, so a later change to `strike`, `frail_abandon` or `ruin` (another abandonment condition, a different destination rule) that is not repeated here would silently break P3-05 invariant 13 (planned total = real toll) and move saliences.
- Fix (no output change if done exactly): factor the per-target decision (toll, abandonment test, destination and room) into one `const` helper that both the plan and the real loop call, or keep invariant 13 in the property tests over many seeds, as the test writer has.

**A2. The decline start can be older than the current decline when a decline begins outside `strike` and the crisis loss.**
- `peoples.cpp:660-669` (causes), `904-906` and `1239` (where `decline_start_` is set).
- `decline_start_` is written only by `strike` and by the no-split crisis loss, and only when the settlement was not already declining. Today every path to `declining` goes through one of those two (the only other `set_settlement_status` calls are the recovery to `thriving`, the ruin and the fade, `peoples.cpp:466`, `576`, `1264`; conflict sets no status), so the value is always the current decline's first blow. A future subsystem (P5 monsters or divine acts) that sets `declining` directly would leave an older event there. The cause list would then be stale but still the same on every platform.
- Fix when such a path is added: set `decline_start_` wherever the status becomes `declining`, or reset it on recovery. No output change today.

### Notes

**N1. Content hash and the new directories.** `placenames/` and `name_filters/` are read by the generic loader (`data/content.cpp:111`, `144-151`): the directory listing is stable-sorted by bytes before use, and errors are sorted. The two kinds are registered after `history` (`kinds.cpp:630-638`), so the canonical encoding writes them last, definitions in ascending id, fields in schema declaration order. `name_filter_from` (`names.cpp:273`) and `placename_data_from` (`regions.cpp`, `kind->definitions`) read the definitions in that same canonical order. The filter is used only for membership (`first_inside`, `first_start`, equality), so its order could not change a result in any case. The placename lists are concatenated in a fixed order (common, land, traits in `REGION_TRAIT_NAMES` order) before a keyed `pick`, and the data checks (`placename_lands_distinct`) make the first-match lookups `land_by_biome` and `land_by_special` unambiguous.

**N2. Region names (RG11 r3).**
- Keys: `k_name = substream(stream_key(seed, "geo.regions.name"), N)` (`regions.cpp:1333`); each value is `hash_at3(k_name, region, attempt, k)` with `k` counting the slots of the drawn form. Keyed, so there is no order of consumption.
- Choices: `history::weighted` and `history::pick` take `std::uint64_t` and `std::int64_t` arguments; pool sizes are `std::size_t` (64-bit on every supported target) and below 2^32 (`pick` is fatal otherwise).
- Order: the keepers come from an ordered `std::map<std::string, std::size_t>` (`regions.cpp:1317`), with ties to the first id. Then every other region is named in id order. The `std::set` of used names serves lookups only and is never iterated.
- Character handling: `lower_char` (`regions.cpp:826`) is ASCII-only. `std::string` ordering compares as `unsigned char` (`char_traits<char>::lt`), so it is the same on every standard library, and the names are ASCII by grammar anyway.
- Inputs: the `has_pass` trait is read from `out.borders` (`regions.cpp:1934-1943`), which RG12 builds sorted and sequentially. The naming loop itself is sequential.
- Arithmetic: everything in `region_traits` (`regions.cpp:1138`) is `int64` with cells ≤ 2^24.

**N3. The name filter and lower-casing (P3-02 r5, r6).** `to_lower` (`names.cpp:104`) and `is_lower` / `is_separator` are explicit ASCII ranges; no `<cctype>`, no locale. A filtered attempt is invalid and the next keyed attempt is drawn with its own keys (`choose_name`), so the filter changes which attempt wins but adds no draw that depends on evaluation order. `god_patterns` replaces `patterns` only for `NameKind::god`; the draw indices (`u(1)`) are unchanged. `check_names_form_text` iterates cultures in ascending id and kinds in a fixed order, and reports the first failure, so the message is the same everywhere.

**N4. Renderer (P3-04 r10 8.4, 8.5).** The three-word form and the "the" rule are byte operations on ASCII text (`event_catalogue.cpp:243`, `822-833`). `#` is replaced by `int_text(v)`, the project's integer formatter, not a locale-dependent conversion. Kind 31 is appended with id 31 and fixed roles and values.

**N5. Quiet eras (P3-04 r10 6.6).** Dropping the `charged_` test (`engine.cpp:576-597`) removes a branch only. `open_eras_` and `charged_` are `std::map<std::int64_t, std::int64_t>`, and civilizations are visited by index.

**N6. Peoples arithmetic and widths.**
- Cell term (`peoples_geo.cpp:649-666`): `elevation` is read as `i16` and widened to `int64`. max(0, e) × 255 ≤ 8.4 × 10^6; the divisor is max(1, `elev_max_dm`), always positive, so `floor_div` never sees a negative operand here and rounding of negatives does not arise. The cell term itself is ≤ 1000 × 255 + 1000 × 255.
- Distances: squared distances in `nearest_other` and the plan are ≤ 2 × 2048^2.
- Populations: `grow_population`'s new target min(share, cap) (`peoples_geo.cpp:451-468`) keeps every product inside `checked_mul`, as does the room 2 × capacity − P.
- Layer checks: the `Cells` check now also requires the layer's catalogue type (`u8`, `u16`, `i16`), which is stricter than before and does not depend on the platform.
- Seed cell: the seed-cell skip for a race with a cell term (`peoples_geo.cpp:720`) is a pure predicate of the data.

**N7. Draw discipline and P3-04 6.9.**
- Draws: `frail_abandon` makes the `ABANDON` draw only when the earlier conditions hold, and `&&` evaluates left to right, so whether a draw is made is defined. Draws are pure (`engine.cpp:1168-1170`), so a skipped draw changes no later value, and drawing `SPLIT` in `plan_split` before the crisis event is appended (rather than after) gives the same value.
- Founding: there is no founding inside a loop over the live lists. `founding_kind` is `const`.
- Held references: the references `abandon` holds (`s`, `d`) are not used across a founding or any state change that could reallocate.
- Lint: the lint rules `history-live-iteration`, `history-held-reference` and `history-sequential-rng` are clean on every changed history file.

**N8. Causes sort and battle losses.**
- Causes: `std::stable_sort(causes.begin(), causes.end())` (`peoples.cpp:667`) sorts at most two `int64` event ids that are distinct by construction (`start != cause`), with `operator<`. That is a strict weak order over fully determined keys.
- Battle losses: `std::min(f_winner, f_loser)` (`conflict_war.cpp:105`) is an `int64` minimum inside `checked_mul`.

### Outside the audit's scope (for the lead)

**O1. The merged branch fails `ctest` on `linux-gcc-release`.** None of the failures is a determinism defect, but the branch is not green:
- Recorded hash constants still at 0.16.0: `cli_golden_test.cpp:132` (`p0-worlds.txt`), `cli_p1_generate_test.cpp:92-175`, `db_determinism_test.cpp:384` (`GOLDEN_CASES`), `stages_pipeline_test.cpp:496`, `509`. These are re-recorded in step 9 of P3-08 section 3 after the cross-preset comparison.
- A test fixture that contradicts P3-05 r7 16.0a: `worldgen/tests/history/p3_04_engine_support.hpp:617` sets `in.walk_area_layer = wat`, the `u8` water layer. `walk_area`'s catalogue type is `u16`, and `PeoplesGeo` now checks the type. Two test cases fail on it with `history: peoples: layer 'walk_area' is not 16 x 16 u16`: `history_engine_schedule_test.cpp:278` (P3-04 3.4 items 2 to 5) and `history_engine_state_test.cpp:383` (P3-04 16.3 item 24). The fix is in the test (a 16 x 16 `u16` zero layer, as for `elevation` two lines above), for the test writer. The library's stricter type check is what the spec asks for.

## 2. Checked and clean

- **Floating point:** no `float`, `double` or `<cmath>` anywhere in the diff, and no use of `<random>` or `std::rand`.
- **Widths:** no `long`, no plain `int` arithmetic in the output path, no `size_t` value stored in the database.
- **Characters:** no `char` signedness dependence (all character tests are ranges on ASCII letters, and comparisons go through `std::string` / `string_view`, which compare as `unsigned char`), and no `std::tolower`, `isalpha` or other `<cctype>` / locale call.
- **Order and environment:** no unordered container, no pointer-keyed map or sort (the `const PlacenameDef*` vectors are built in definition order and never sorted), no wall-clock time, environment variable or thread id.
- **Uninitialized data:** every new struct member has a default initializer (`RegionNameInput`, `RegionNameTraits`, `PlacenameDef`, `SplitPlan`, `Token`, `NameAttempt::filtered`).
- **Database:** the schema is unchanged (7). Kind 31 is appended to the catalogue rows in id order. Rows are still written by the existing ordered writers.
- **Build flags:** no flag change (`cmake/` untouched by the diff).

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test`: passed (17 snippet files). Scan: 162 C++ files and 13 CMake files, 0 findings, both before and after the change below.

Added on `p3/08-audit` (lint only, no output byte changes): rule **`ctype`**. It reports `#include` of `<cctype>`, `<ctype.h>`, `<cwctype>`, `<wctype.h>`, and free, `std::` or `::` calls of `isalnum`, `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`, `tolower`, `toupper`, their wide forms, `iswctype`, `towctrans`, `wctype` and `wctrans`. These functions follow the C locale's `LC_CTYPE`, and a negative `char` argument is undefined behaviour (signed `char` on GCC, Clang and MSVC for x86). The project already avoids them by convention (`data/text.hpp`, `core/hash.cpp` say so in comments), but nothing enforced it, and this change added three new lower-casing helpers. Members (`x.tolower()`), other namespaces (`ascii::toupper`), the project's `to_lower` and `is_lower`, comments and literals are not reported. Files: `tools/lint/determinism_lint.py`, `tools/lint/README.md` (rule table and self-test table), `tools/lint/selftest/bad_constructs.cpp` (six reported lines), `tools/lint/selftest/good_new_ignored.cpp` (near misses).

## 4. Runs

Build: `linux-gcc-release` at `d2fce98` plus the lint change (no library change).

- **Thread counts:** `generate --threads 1`, `2` and `6` with the checkout's `game/data`. The file SHA-256 was identical across the three thread counts for every case: seed 0 / 512 `a6397189…475f`, seed 42 / 512 `136ae410…f62c`, seed 7 / 1024 `20da11ad…6ee2`, seed 42 / 1024 `100f1174…ed19`, seed 123 / 512 `5c13c4b1…4872`.
- **`world-hashes.sh generate linux-gcc-release`:** all fifteen cases generate. `expect` fails on every case, as it must at this stage: `p0-worlds.txt`, `p1-worlds.txt` and `p3-worlds.txt` still hold the 0.16.0 values, and the validation specialist re-records them after the cross-preset comparison (P3-08 section 5, step 9). The 0.17.0 value of `geo-seed-42-512` agrees with the seed 42 / 512 run above.
- **`ctest --preset linux-gcc-release`:** every entry passes except `worldgen-tests-engine`, `worldgen-tests-other` and `worldgen-cli-tests-other`. Every failing case is listed in O1: hash constants still at 0.16.0, and one fixture. The stage goldens of the independent Python references (`p1-06-regions.txt`, `p3-01` to `p3-06`) are re-recorded on the branch, and every entry that reads them (`-regions`, `-history`, `-chronology`, `-engine-worlds`, `-peoples` and the conflict cases) passes. So the C++ agrees with the references on every fixture and shipped world they cover.

Cross-compiler identity (GCC against Clang and MSVC) is for the validation's `world-hashes.sh compare` and CI's `compare-hashes`; nothing in the code read here gives a reason to expect a difference.

## 5. Verdict

**Pass with notes.** No blocking finding. A1 and A2 are advisories for the engineer (no output change today). O1 is not a determinism finding, but the branch needs the step 9 re-recording and the test writer's fixture fix before validation. The lint rule `ctype` is committed on `p3/08-audit`.
