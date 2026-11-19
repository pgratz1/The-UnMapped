# Determinism audit: P3-01 history data and settings (generator 0.12.0, schema 5)

Auditor: determinism auditor, 2026-11-19. Change audited: `git diff origin/main origin/p3/01-int` (`origin/main` at `286213d`, `origin/p3/01-int` at `4e1a3c1`, the merge of `p3/01-history-data` at `728a808`/`65833c4` and `p3/01-history-data-tests` at `80af7db`). Spec: `docs/specs/p3-01-history-data-settings.md`, with P3-02 section 3 for the `culture_naming` record. Line numbers are those of `4e1a3c1`.

Scope: `worldgen/src/data/kinds_history.cpp` (the schemas, string grammars, named and kind-level checks of `race`, `culture` with `culture_naming`, `history.chronology`); `worldgen/src/data/kinds.cpp` (registration); `worldgen/src/data/settings.cpp` and `worldgen/include/worldgen/data/settings.hpp` (the history keys, enum names, `parse_races_text`, `race_ids_in_order`, `check_settings_races`, `resolve_races`); `worldgen/include/worldgen/data/schema.hpp`; `worldgen/include/worldgen/version.hpp`; `tools/worldgen-cli/cli.cpp` (the history flags, their checks, the `--verbose` line, help text); the shipped data `game/data/{races,cultures,history}/` and the two fixture copies under `worldgen/tests/data/geo/` and `geo-small/`. The unchanged loader code the new kinds go through was read where order or encoding depend on it: `content.cpp` (discovery, canonical id order, kind-level check reporting), `encoding.cpp` (canonical encoding), `schema.cpp` (`KindRegistry::add`), `load_error.cpp` (`error_less`, `sort_errors`) and `common.cpp` (`is_rejected_scalar`). Test files were read only where they define data.

**Verdict: pass.** No blocking finding. The change adds three kinds to the content hash and nothing else reaches a world: on every input, the only bytes that differ from 0.11.0 are `meta.content_hash` and `meta.generator_version` (section 4). The content hash is a function of the data files' values in a fixed order, independent of platform, compiler, standard library and file-system order. Every check runs in a defined order and every message is built from ASCII literals, ids and the project's integer formatting. The two compilers available here (GCC and Clang) give the same messages byte for byte. The three copies of the data are the same blobs, ASCII and LF. Eight notes (section 1), two of them advisory. One lint rule was added for the history code (see the P3-02 audit, section 5). It finds nothing here.

Classification: **blocking** can change output (a world, a golden vector, `report`/`hash` text or a message) between platforms, compilers, standard libraries or thread counts. **Advisory** means fragile, or outside the determinism contract, but currently safe. A **note** is for the record.

## 1. Findings

### Blocking

None.

### Advisory

**A1. Version-only test expectations still say 0.11.0 on `p3/01-int` (not a determinism defect; already being fixed).** On `4e1a3c1`, built with `linux-gcc-release`, these fail: `cli_conventions_test.cpp:242` (`--version` text), `cli_passes_test.cpp:189` (`GENERATOR_VERSION == "0.11.0"`), the `cli_generate_test.cpp` meta check (`:343`), `cli_audit_test.cpp:131` and `cli_geography_test.cpp:307` (report text with the version), and the P0 golden-hash tests (`db_determinism_test.cpp:383`, cli 9/12.6). They fail because the version moved to 0.12.0 and `p0-worlds.txt` has not been re-recorded yet. `origin/p3/01-int-fix` (`de02d22`) changes only the version-only expectations, in 7 test files. The golden files are for the validation specialist to re-record (CLAUDE.md, "Golden values"). The bytes behind them are identical on GCC and Clang (section 4). Fix asked: merge `p3/01-int-fix` and re-record `p0-worlds.txt` and `p1-worlds.txt` after the cross-preset comparison. No code change.

**A2. The order of every naming list is part of the contract twice.** The canonical encoding (`encoding.cpp:65-71`) writes an array's length and then its entries in file order. The P3-02 generator picks by index into the same lists (`culture_naming_from`, P3-02 `names.cpp:136-149`). Reordering `onsets` or `settlement_forms` in `game/data/cultures/*.toml` therefore changes `meta.content_hash` and also every name that will be drawn. Repeating an entry (elven `codas` lists `""` twice, dwarven `patterns` lists `"ONCONC"` twice) is a deliberate weight and is hashed as such. This is deterministic and intended. It is recorded so that tuning edits treat order as data. No action.

### Notes

**N1. Content-hash input order and encoding.**
- **Kinds.** `KindRegistry::add` inserts by `bytes_less` on the name (`schema.cpp:381-383`), and `load_from` loads in that order (`content.cpp:780`). The encoding therefore holds `biome`, `culture`, `history`, `race`, `worldgen`, which is the order P3-01 7 states.
- **Definitions.** Each kind's definitions are in ascending id by bytes. They come from a `stable_sort` on `bytes_less` (`content.cpp:704-705`) over the discovered files, and the file list is itself sorted (the existing `directory-iteration` suppression at `content.cpp:144`). Directory order cannot reach the hash.
- **Fields.** Fields are in schema declaration order. The validator fills the record by walking `schema.fields()` (`content.cpp:331`), not the TOML table, so the inline-table order of `site_weights = { … }` or of `{ biome = …, weight = … }` does not matter. The declaration orders in `kinds_history.cpp:331-349` (race), `:369-387` (culture_naming), `:391-395` (culture) and `:408-412` (chronology) match spec sections 3, 4 and 5.1 and P3-02 3.
- **Values.** Integers are written as int64 little-endian by arithmetic. `awakening_share` and `awakening_jitter_frac` are `Fx` raw values parsed from decimal text (`Type::fixed("0", "1")`, `("0", "0.05")`). Enums and references are strings with a u64 length. No float is involved anywhere.
- **Reference.** `worldgen/tests/reference/p3_data_reference.py check` reports 97 checks and 0 failures. `golden -` reproduces `worldgen/tests/golden/p3-01-data.txt` byte for byte, from the TOML files alone. The C++ `content_hash` of `game/data` (`be84128b…f65e`) equals the golden value and is what the generated worlds carry (section 4).

**N2. Check order and message determinism.**
- **Named checks.** Per-definition checks run in declaration order: `race_generation_below_lifespan`, `race_site_weights_positive`, `race_biomes_unique`, `race_settlement_kinds_unique` (`kinds_history.cpp:351-356`), then the three naming checks of the culture (`:398-403`) and `chronology_age_name_form` (`:413-415`).
- **Kind-level checks.** These run over the type-valid definitions in id order (`content.cpp:716-743`), in the order unique, dense, awakening (`:360-367`).
- **Containers.** The containers are `std::map<std::int64_t, std::size_t>` (`:196`, `:235`), `std::set<std::int64_t>` (`:213`) and `std::map<std::string, std::size_t, detail::BytesLess>` in `first_repeat` (`:155`). They are used for lookup and first-occurrence only and never iterated to produce output. `race_order_unique` names the earliest definition in id order (`try_emplace` keeps the first). `race_order_dense` reports the smallest missing order by counting `k = 1 … n`. `race_awakening_order` compares against the earliest definition of order k−1. Each is a function of the id-ordered list.
- **Sorting.** Every error goes through `ErrorList::take_sorted` or `sort_errors` (`settings.cpp:436`). Both use `error_less` (path by bytes, line, column, message by bytes; `load_error.cpp:26-37`) under `std::stable_sort` and `std::unique`, so the printed order does not depend on the order in which checks ran.
- **Message text.** Messages are ASCII literals plus ids, ASCII-checked keys and integers through `detail::int_text` and `uint_text` (the project's formatting, no locale or stream). `found_text` (`settings.cpp:124-132`) prints an integer, a string in quotes or a kind phrase. A float in a settings file is rejected by `is_rejected_scalar` (`common.cpp:108-122`) before any text is built, so no floating value is ever formatted.
- **CLI.** `check_flag_races` (`cli.cpp:546-563`) reports the first unknown id in the order given and lists the known ids in race order. `check_settings_races` reports every unknown element at its own position. Observed: the same error output byte for byte from the GCC and Clang builds, for a settings file with five bad history keys, a file with two unknown races, `--races orc,troll,elf` and `--races orc,,elf` (section 4).

**N3. `race_ids_in_order` is fully determined.** `settings.cpp:396-413`. The input is `KindContent::definitions`, already in ascending id by bytes (N1). `std::stable_sort` by `order` keeps equal orders in id order. A loaded set has unique orders (`race_order_unique` refuses duplicates), so the result is the unique ascending-order list. If duplicates were ever let through, it would still be (order, id) order, never a library-dependent one. `resolve_races` (`:440-457`) filters that list, so the stored order is race order whatever order was given. Observed: `--races dwarf,elf` gives `races elf,dwarf`. The CLI checks unknown ids first (`cli.cpp:652-659`), so the `fatal` at `:446` is only reached by a caller that skipped the check. Its message is fixed text.

**N4. No float, locale or platform text.** No `float` or `double`, no `<cmath>`, `<locale>` or stream formatting in any changed file (lint, and reading). Character classes are ASCII range tests on `char` (`is_ascii_letter`, `detail::is_lower`, the grammar functions at `kinds_history.cpp:30-100`). A byte ≥ 0x80 is outside every range whether `char` is signed (negative) or unsigned (above `'z'`), so the grammars do not depend on `char` signedness. The flag values go through `db::parse_canonical_u64`, and the bound is tested in `uint64` before the cast to `int64` (`cli.cpp:490-492, 502-504`), so no out-of-range conversion is possible. The enum tables (`settings.cpp:296-303`) are exact byte comparisons.

**N5. The history settings reach no world yet.** `Settings` gains the eight history values. In P3-01 they are parsed, checked and normalized, and nothing writes them: `meta` gets them from P3-03 on (spec 6.3). The `--verbose` line (`cli.cpp:565-579`, `:662`) goes to standard error only. Observed: a 512 seed-3 world generated from a settings file with every history key set to a non-default value and `--races dwarf,elf --civs-per-race auto --threads 3` is byte-identical to the plain world (`f4c7ea45…`).

**N6. The three copies of the data are identical, ASCII and LF.** On `origin/p3/01-int` the 13 files (6 races, 6 cultures, `history/chronology.toml`) are the same blob in `game/data/`, `worldgen/tests/data/geo/` and `worldgen/tests/data/geo-small/` (compared by `git ls-tree` object id, and with `cmp` in the checkout). `git ls-files --eol` reports `i/lf w/lf attr/text=auto` for each. `.gitattributes` (`* text=auto eol=lf`) is unchanged, no file holds a CR, and no byte is above 0x7F. A Windows checkout therefore hashes the same bytes. The golden values of `game/data` and `geo` coincide (`be84128b…`). That is expected: `geo`'s `worldgen` and `biome` data equal the shipped data, and `geo-small` differs only in its non-history tuning.

**N7. `GENERATOR_VERSION` 0.12.0 is the right bump.** Every world's `content_hash` changes, so the version must move (CLAUDE.md, "Golden values"). There is no schema change and `SCHEMA_VERSION` stays 5, which section 4 confirms: the schema text and the pragmas are unchanged.

**N8. Small observations, no action needed.**
- `Settings::operator==` is written out by hand so that `races_origins` is ignored (`settings.cpp:352-359`). A later field added to `Settings` and not to this function would compare equal when it differs. That only affects tests and normalization checks, never output. Worth remembering when P3-03 adds the `meta` keys.
- `name_of` returns an empty view for an enumerator missing from its table (`settings.cpp:306-313`). Every enumerator is listed, so the empty view cannot be returned.

## 2. What was checked against the hard rules

| Rule | Result |
| --- | --- |
| Integer and fixed-point only in the output path | yes. The only non-integer values are `Fx` from decimal text. No `float` or `double` in any changed library or CLI file |
| No standard transcendentals or distributions | none. Nothing in this change draws or computes |
| One generator, streams by name | no stream, no `Rng`, no draw |
| No unordered iteration, pointer order, thread timing, clock | none. Ordered containers keyed by `int64` or by bytes, for lookup only (N2). The id order comes from `stable_sort` by bytes. File-system order never reaches the hash (N1) |
| No fast-math | no flag change. One source line added to `worldgen/CMakeLists.txt` |
| SQLite pinned, rows in a defined order, no timestamps | no writer change. Only `meta` values differ (section 4) |
| UB and implementation-defined behaviour | none found. Sums of five values ≤ 1000 in `int64` (`kinds_history.cpp:142-146`), bounded casts (N4), `char` signedness irrelevant (N4) |

## 3. Lint

On `4e1a3c1`: `python3 -B tools/lint/determinism_lint.py --self-test` passed (13 snippet files, 0 failures), and `python3 -B tools/lint/determinism_lint.py` scanned 125 C++ files and 13 CMake files with 0 findings. `kinds_history.cpp` is in the scan set. With the rule added on the audit branch (`history-sequential-rng`, P3-02 audit section 5), the same scan of an integration of `p3/01-int`, `p3/02-names` and `p3/02-names-tests` reports 0 findings over 129 C++ files, and the self-test passes with 15 snippet files.

## 4. What was run

All runs were on Linux (6 cores, shared with other agents' jobs), in detached scratch worktrees outside the repository. Builds were under `~/.cache/the-unmapped/build/<worktree>/` and worlds under `~/.cache/the-unmapped/out/p3-01-02-audit/`. The trees were:
- `p301audint`: `origin/p3/01-int` as is.
- `p302audint`: `origin/p3/02-names` merged with `origin/p3/02-names-tests` and then `origin/p3/01-int`. The only conflicts were in `worldgen/tests/CMakeLists.txt` and `worldgen/tests/reference/README.md`, and both blocks were kept.
- `mainbase`: `origin/main` (`worldgen-cli 0.11.0 schema 5`). Its 512/42 world hashes to `ccf7ad97…d2d1`, the value in `main`'s `p1-worlds.txt`.

| Run | Result |
| --- | --- |
| `--until none`, seed 42: 0.11.0 (`mainbase`) against 0.12.0, compared table by table, plus the schema text and the pragmas | only `meta` differs, in `content_hash` (`1543a4ae…` to `be84128b…`) and `generator_version`. The `sqlite_master` text is identical. `page_size` 4096, `user_version` 5, `application_id`, `auto_vacuum` 0, `journal_mode` delete, encoding UTF-8 are the same on both (spec 9 test 10) |
| 512 seed 42, `hash --detail`, 0.11.0 against 0.12.0 | only `file` and `table meta` differ. Every layer and every other table is identical |
| `p3/01-int` alone against the integration with P3-02 | same file hashes for `--until none` seed 42 (`7ce809fb…`) and 512/42 (`dd76b4cf…`) |
| `tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release` (integration tree) | all 12 cases identical across the two presets |
| `world-hashes.sh expect linux-gcc-release` | every case differs from the recorded goldens, which still hold 0.11.0 values (A1). Expected |
| 512 seed 3, GCC and Clang release, threads 1 and 5 | one hash, `f4c7ea45…`. The same hash with every history key set in a settings file plus history flags (N5) |
| Error messages, GCC against Clang (N2) | byte-identical |
| `worldgen-tests -sf='*data_history*'` and `worldgen-cli-tests -sf='*history*'`, GCC and Clang release | 42 and 16 test cases, all pass |
| `p3_data_reference.py check` and `golden -` | 97 checks, 0 failures. The golden file is reproduced exactly |

Not run: MSVC and Windows (not available here; CI's `compare-hashes` covers them, and N6 shows the data bytes are platform-independent), `linux-clang-asan`, and the debug presets (the validation specialist's runs cover them).
