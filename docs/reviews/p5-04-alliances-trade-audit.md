# P5-04 Alliances, trade, mines and arms: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-24.
Scope: `git diff origin/p5/03a-int..origin/p5/04-impl -- worldgen tools/worldgen-cli game/data`, at the engineer's head 0b7b858. Tests, test data and references were read as context only. Spec: `docs/specs/p5-04-alliances-trade.md` r2 (section 14 for the auditor), with the lead's two rulings since r2 (a pair whose alliance or trade row closed earlier in a step does not reopen in that step; at `alliance_formed` the pair's strain and claims are dropped both ways), P3-04 section 6.9 and the P5-03a audit's advisory A4 on `credit` callers. Generator 0.26.0, schema 11, history report format v7.

**Verdict: pass with advisories.** I found no blocking finding. There are three advisories (A1 to A3). The lint needed no new rule.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 21 snippet files, 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 186 C++ files and 13 CMake files scanned, 0 findings.
- The diff adds no `determinism-lint: allow` suppression, no float, no `<cmath>`, no `<random>`, no unordered container, no clock, locale, environment or thread construct, and no `std::sort` (every sort is `std::stable_sort` with a comparator that is a total order on ids or values).
- No CMake or compiler flag change.

## Cross-compiler check

`linux-gcc-release` and `linux-clang-debug` were built at 0b7b858 (`worldgen-cli` only). Five 1024 worlds were generated with each, using `--threads 2` and the checkout's `game/data`. For each pair the comparison covered four outputs: the SHA-256 of `world.db`, `hash --detail`, `report --history --level summary` and `--level full`. All 20 comparisons are byte-identical.

| Case | Options | Alliances formed | Joined | Allied battles | Trades | Spoils owed / withheld | Spurned | Betrayed | Mines | Arms sold |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| s0 | `--seed 0` | 8 | 5 | 7 | 8 | 0 / 0 | 0 | 0 | 4 | 4 |
| s5 | `--seed 5` | 24 | 27 | 50 | 20 | 1 / 1 | 1 | 1 | 1 | 4 |
| s11-civs4 | `--seed 11 --civs-per-race 4` | 108 | 75 | 143 | 82 | 16 / 5 | 5 | 3 | 5 | 14 |
| s7-high | `--seed 7 --monster-pressure high` | 11 | 14 | 21 | 14 | 1 / 0 | 0 | 1 | 5 | 8 |
| s3-civs4-high | `--seed 3 --civs-per-race 4 --monster-pressure high --event-intensity turbulent` | 73 | 45 | 83 | 71 | 10 / 3 | 3 | 3 | 3 | 9 |

Every new rule fired in at least one case: formation, origin, renewal, breaking, betrayal, spurning, calls, allied battles, spoils both ways, trade, mines and arms. MSVC was not run here; the `compare-hashes` CI job covers it.

## What was checked

1. **Streams** (`core/streams.hpp:146-150`, `:230-234`; `alliances.cpp:144-148`).
   - The five new streams are appended after `hist.relations.settlers` in the order of spec 3: `hist.alliances.form`, `.join`, `.trade`, `.spoils`, `.mine`. `all_stream_names` grows from 77 to 82 entries; every name is unique, so no stream key collides with another stream.
   - Every draw is `ctx.draw(key, a, b, k)` (the step's year as w0) with the ids of spec 3's table, never a list position:
     - form (x, y, 0), x < y: `alliances.cpp:912`; the pair loop visits each pair once per T2.
     - join (w, z, 0): `:689`; one call per ally per declaration (`allies(partner)` is a set), a declaration is read once (cursor).
     - trade (x, y, 0): `:999`; once per pair per T2 or Q4 (no T2 and Q4 share a start year).
     - spoils (w, z, 0): `:1091`; drawn only when the event is written, and the record is then settled, so once ever per key.
     - mine (c, R, 0): `:1199`; once per (c, R) per step.
   - The bloc test precedes the form draw (`:909`), as 6.3 says; no draw is spent on a refused pair. No two draws in one expression.
2. **Containers and order.**
   - `alliances_`, `trades_`, `mines_` are `std::map<pair<int64,int64>, ...>`; `betrayed_`, `arms_sold_`, `mined_`, `sellers_at_start_` and `closed_now_` (`std::set<std::pair<Pair,bool>>`) are ordered sets of ids; `presence_`, `share_`, `allies_of_`, `spoils_given_` are maps keyed by ids. `joined_` and `declarations_` are vectors in append order (record order; ascending event id).
   - `allies_of_` is rebuilt from `alliances_` after every change (`:247-258`), each list stable-sorted, so `allies(c)` is ascending as `AllianceView` promises.
   - `closed_now_` is cleared at the top of `run` (`:564`), once per step; it is tested by `origin_alliances` (`:861`), `formations` (`:892`) and `trade_openings` (`:977`), the only places that open an alliance or a trade. `close_alliance` and `close_trade` (`:507-521`) are the only closers that insert; `drop_closed` drops rows conflict closed (ended side, conquest), whose pairs cannot reopen because a side has ended. This matters beyond the history: `relation` is `WITHOUT ROWID` with primary key `(a_type, a_id, b_type, b_id, kind, since)`, so a same-year reopen would fail the insert rather than diverge.
   - Mine closings run in ascending site id (`:1124-1129`, pairs (site, key) stable-sorted); openings in ascending civilization, then region, then the largest population with the smaller id (strict `>` over ascending ids, `:1188`).
   - Bloc sums run over `allies(c)` (ascending) with `checked_add`; integer sums are order independent anyway.
   - The fear reason (`:446-459`) walks `feared_` in ascending id with a strict `>`, so the largest bloc share wins with ties to the smaller id; its cause is the maximum `owner_event` over regions 1..N.
   - The common-enemy reason walks `declarations_` from the largest id down and takes the first that qualifies, which is the latest declaration. The front pruning (`:630-633`) removes only declarations that no reason can use (old and closed), so it changes no result.
3. **Key copies (P3-04 6.9, 6.10 item 1).** Every pass that closes iterates a copy of the keys and re-finds each key: `cooling` (`:738-762`), `renewals` (`:790-837`), `trade_breaks` (`:937-958`), `arms_sales` (`:1240`), and `calls` copies `allies(partner)` before the loop (`:658-659`) and re-finds the alliance per ally. `blocs` reads `allies(c)` in an inner loop that does not close, and closes after it. No loop over `state.relations()` opens or closes; `drop_closed` only reads it. The `Alliance&` in `renewals` (`:800`) is not used after `spurn` erases it, and is re-fetched by `at(p)` after the append (`:831`); `std::map` references are stable anyway.
4. **Fixed points of the seams.**
   - `forgive` (`conflict.cpp:882`) is called only in `open_alliance` (`alliances.cpp:495`), after `open_relation` and before `credit`; it is `drop_pair`, which zeroes both strains and erases both claims (the lead's second ruling). `add_grudge_seam` (`conflict.cpp:889`) is called only at `alliances.cpp:1104`, in `spoils` after the `spoils_withheld` append. Both are called from this subsystem's `run`, never from inside a conflict loop.
   - `join_war` (`conflict.cpp:1662`) is called once, at `alliances.cpp:695`, after the `ally_joined_war` append. It refuses a closed war, a bad side, a principal, a second join and an existing war row; it closes the `peace` row if open and opens the directed `war` row with the join event. It grows `wars_[i].allies` only; it does not touch `wars_` itself.
5. **`credit` callers (P5-03a A4).** All ten calls are inside `AlliancesSubsystem::run`, in the order of 6.1, each with an event already in the log: formation and origin (`open_alliance`, `:502`, the `alliance_formed` just appended), betrayal (`:648`), join (`:697`), refusal (`:709`), renewal (`:835`), trade credits (`:1031`, the trade's own `trade_opened`, an earlier event, as 6.9 says), ends of joined wars (`:1051`, the ally record's `close_event` or the war's `end_event`, called only when non-zero), granted and withheld shares (`:1100`, `:1103`), arms (`:1268`). None is reached through a container of pointers or a callback whose order depends on other subsystems.
6. **Wars in declaration order** (`conflict.cpp:819-828`). `war_index` is a `lower_bound` on `ConflictWar::declared`. The invariant holds: the only write to `wars_` in the whole history code is `wars_.push_back(w)` in `declare` (`conflict.cpp:1657`), with `w.declared` the id of the `war_declared` just appended, and event ids ascend. Nothing erases, inserts, sorts or reassigns `wars_` (searched every `wars_.` and `wars_ =` in `worldgen/src/history` and `worldgen/include/worldgen/history`); the only `war_declared` emitter is `declare` (`conflict.cpp:1626`), so every declaration alliances read from the log has its war, and `war_index` cannot fail on one. Resume re-runs the history from the chronology world, so no path restores `wars_` out of order. Loops that close wars (`housekeeping` `:1278`, `conquer` `conflict_war.cpp:556`) iterate by index up to a count taken before the loop and do not add wars.
7. **Allied battles** (`conflict_war.cpp:99-210`).
   - `w0` is a copy of the war; `Member::ally` indexes `w0.allies`, which has the same order as `wars_[i].allies` (only appended to, never reordered).
   - Members are stable-sorted by civilization id; participants are winner, loser, winner allies ascending, loser allies ascending.
   - The split: each ally takes `floor(total * F_m / F_S)` in ascending id, the principal `total - given`. Since `F_S` is the principal's force plus every member's, Σ shares ≤ total and the principal's rest is non-negative. The winners' side is split before the losers' in two sequenced statements. `rec.dead` adds the computed share (reading A6), the war's `dead[]` the principal's actual casualties.
   - The score (`conflict.cpp:1796`) and `war_spoils.side_dead` add allies' dead per side with `checked_add`.
8. **Ends with allies.** `end_war` calls `close_allies` after the costs (`conflict_war.cpp:725`), `abandon_search` after `close_war` (`:844`); `close_allies_at_side_end` from `housekeeping` and `conquer`; `withdrawals` before `end_checks` in `fight`. Each walks the allies by index on a copy of the record, re-fetches the record by index to write it, and closes and opens relations with an existing event. `close_allies_at_side_end` checks the row is open before closing it (it may already be closed by the ended side's own housekeeping); `close_allies` does not need to, since a war's normal end never ends a side before it runs (`cede` does not conquer).
9. **Arithmetic.** Every new product is `checked_mul`, every sum `checked_add` or `checked_sub`, every division `floor_div` with a denominator the schema keeps positive: `ally_full - ally_min` (check `alliances_dispositions`), `ally_full - trade_min` (`alliances_trade`), `hostility_full` ≥ 1, `trade_credit_years` ≥ 1, `max(1, ...)` for forces, `side_dead` and the exhaustion base. Quiet-step products (`p * L`) are capped at 1000 after a checked product. See A1 for the one product the spec's bound note misses.
10. **Signed and unsigned, narrowing.** Every index is a `static_cast<std::size_t>` of a value already range-checked (region and civilization ids from the state, `(id - 1) % n` of the mine forms with `id ≥ 1`). The report's `tb[e.values[0]]` (`history_report.cpp:1306`) is guarded to 0..2. No `long`, no `size_t` stored: counts written to the report go through `static_cast<std::int64_t>` and `num`.
11. **Sites (7.16).** `next_site_id()` is `sites_.size() + 1`; `open_site` refuses any other id and any kind but 3, so mines and monsters' sites share one ascending id sequence and the `site` rows are written in id order. Monsters runs before alliances in every shared step, so the id order is the step order. `alliances.cpp` holds no `Site&`; `mine_openings` copies the settlement (`town`, `:1203`) before the append and the site opening, and the `const Settlement&` of the candidate loop (`:1184`) is not used after it. `end_site_seam` refuses a site that is not an open mine. The kind-3 exclusions in `holds_site` and the lair placement (`monsters.cpp:544`, `:917`) only narrow existing predicates; monsters' outpost and lair loops already filter by kind.
12. **Uninitialized data.** Every new struct member has an initializer (`Alliance`, `Trade`, `Joined`, `Mine`, `Reasons`, `Declaration`, `ConflictAlly`, `WarSpoils`, `ConflictWar::plunder{}`, `SiteSpec`); every event's values are set where it is built. No struct is hashed or written as bytes.
13. **Evaluation order.** No function call or braced list in the diff has two arguments with side effects (searched for two of `append`, `casualties`, `credit`, `join_war`, `next_site_id` in one call). `Declaration{...}` (`alliances.cpp:611`) is a braced list, evaluated left to right. `checked_add(force(z), force(partner))` (`:677`) calls two const functions whose only cache, `PeoplesView::hops_`, gives the same value whichever is filled first.
14. **Readers and text.** Digest (`digest.cpp`: `alliance_stats`, `blocs_by_year`, `principal_war`), report (`history_report.cpp`: `[alliances]`, `allies:`), timeline (`alliances_at`, `allies_of`, `alliance_groups_at`, `trades_at`), browser group table and the mine marker: every list is built in row or event order and stable-sorted by ids or (since, a, b); the union-find roots by the smaller id over a `std::map`; the bloc maximum keeps the first (year, civilization) on ties; numbers go through `num`; the report text is ASCII. Vectors of `const db::RelationRow*` are sorted by row values, never by address.
15. **Data.** `game/data/history/alliances.toml` and the race fields are integers and short strings; `origin_allies` is resolved to race ids and sorted (`alliances.cpp:159-166`); the reciprocity check (`data_checks.cpp`) reads both lists in file order and reports the first failing entry.
16. **Threads.** The history stage is single-threaded; nothing in the diff touches the parallel geography code.

## Advisories

All three are fragile but safe today. None can make two platforms write different bytes: each one, if it were ever reached, would stop generation with the same fatal error on every platform.

**A1. The casualty split multiplies force by dead, a product that spec 14's bound note does not cover** (`conflict_war.cpp:180`; side totals `:112-130`).
- `share = floor_div(checked_mul(total, m.force), F_S)`, where `total` is a side's dead and `m.force` an ally's local force.
  - The dead are of the order of a force: `loser_dead_of` is at most `force_winner × kill_max_permille / 1000`.
  - So the product is about force², which can reach 2^80 under the stated bound (force < 2^40, checked per civilization at `conflict.cpp:776`, `:909`).
- Spec 14 says the largest product is force × 1000 < 2^50. It is not.
- A side's total (`attack`, `defend`) is a sum of up to `allies_max + 1` forces, and has no bound check of its own.
- The scenario: a world whose realms' local forces pass about 3 × 10^9 would make `checked_mul` fail. That is far beyond any population the peoples rules allow, so it is not reachable today. If it were reached, every platform would stop with the same fatal error.
- Proposed fix (spec writer and engineer):
  - Restate the bound in spec 14: dead ≤ force, so the product is < 2^80 by the stated bounds, and < 2^63 only while forces stay below about 2^31.
  - Either add that bound check on `F_S` after the allies are added, or compute the share with the project's wide multiply-divide in `core/wide.cpp`.

**A2. `close_allies` closes each ally's war row without checking that it is still open** (`conflict_war.cpp:743-764`, the call at `:725`).
- `close_allies_at_side_end` checks with `state.relations().count(key)` before it closes. `close_allies` calls `state.close_relation(civ_relation(m.civ, enemy, "war"), ev)` directly, and then opens a `peace` row with the enemy.
- This is safe today. In `end_war` nothing between `close_war` and `close_allies` can end a side: `cede` does not conquer, and the tribute's `casualties` only lowers population. A realm ends later, in housekeeping.
- The scenario: a later stage (P5-05 vassalage, or a cession that can take a realm's last region) lets a war's end end the loser before `close_allies` runs. Conquest's `close_relations_of(loser)` would then close the ally's row first, and `close_relation` would stop generation ("relation 'war' is not open"). It would also open a `peace` row with an ended realm.
- Proposed fix: guard the close as `close_allies_at_side_end` does, and skip the peace when the enemy has ended. Or assert in a comment at `:725` that no side can end before this call.

**A3. `closed_now_` is per step, but the relation table's key is per year** (`alliances.cpp:564`; `db/schema.hpp:187-189`).
- Ruling A9 is kept by clearing `closed_now_` at the start of every step.
- The `relation` primary key is `(a_type, a_id, b_type, b_id, kind, since)`. A pair that closes and reopens an alliance or a trade in the same year therefore fails the insert, whatever the step.
- This is safe today. Alliances and trades open only in T2 and Q4, and no T2 shares a year with a Q4. T6, the only other step in a T2's year, opens nothing.
- The scenario: a later change opens a relation in T6 (for example a trade after a peace, or an alliance sworn at a war's end). A pair could then close in T2 and reopen in T6 of the same year. `closed_now_`, already cleared, would not stop it, and writing the world would fail.
- Proposed fix: key the set by year. Clear it when `ctx.year()` differs from the year of its last clear, not at every `run`. Or record (pair, kind, year) of every close and test that. The second form also covers rows that conflict closes.

Not a determinism finding, and recorded for the engineer only: `spoils` (`alliances.cpp:1065`) also settles silently a war whose `end_event` is not a `peace`, `war_won` or `civ_conquered`. An example is a side's end recorded with another ending kind. This follows invariant 11 rather than 6.11's wording ("the surviving principal" wins). If the difference was not intended, it belongs in the proposals.

## Not extended

Considered and rejected for the lint: the casualty split's force × dead product (A1) needs value ranges, not text; the `closed_now_` rule (no reopen in a step) is a data-flow property of three functions; a held `Joined&` in `joined_ends` is safe because nothing appends to `joined_` outside `calls` (T2 only, never during `joined_ends`), and a rule for range-for variables of member vectors would flag every such loop.

## Verdict

Pass with advisories (A1 to A3). No blocking finding.
